# d2x 架构重设计：通用练习框架 + 双向协议边界

- 日期：2026-07-19
- 状态：设计已确认，进入实现
- 涉及仓库：`d2learn/d2x`（分支 `feat/exercise-framework-protocol`）、`mcpp-community/d2mcpp`（分支 `feat/mcpp-provider`）
- 相关调研：`d2mcpp/.agents/docs/2026-07-19-mcpp-replace-xmake-research.md`

---

## 1. 定位

**d2x 是练习驱动学习的通用框架：它拥有学习循环和它的呈现，除此之外什么都不拥有。**

d2x 不知道 C++，不知道 mcpp，不知道怎么编译任何东西。它向下定义一套协议对接课程，向上定义一套协议对接前端，两侧都是 NDJSON 事件流。

```
   前端：内置 TUI / print / VSCode 插件 / Web / CI
                    ↑  Frontend Protocol   (NDJSON，单向)
   ┌────────────────────────────────────────────┐
   │  d2x core  —— 学习循环 · 会话状态 · 文件监听 · 编排  │
   └────────────────────────────────────────────┘
                    ↕  Provider Protocol   (NDJSON)
   课程侧：d2mcpp Provider（C++26 + mcpp）
```

### d2x 明确不拥有

| 不拥有 | 归属 |
|---|---|
| 构建工具、编译命令、清单生成 | Provider |
| 通过判定规则（`❌` / `D2X_WAIT` 是 d2mcpp 的 C++ 断言约定） | Provider |
| 练习内容、顺序、章节归属 | Provider |
| 具体渲染方式 | 前端 |

---

## 2. 领域模型

```cpp
struct Exercise {
    std::string              id;        // 稳定标识，状态持久化用它
    int                      order;     // 显式顺序，不再靠字典序
    std::string              title;
    std::string              chapter;
    std::vector<std::string> files;     // 学员编辑的文件，绝对路径
    std::optional<std::string> hint;
    std::optional<std::string> solution;
};

enum class Outcome { Pass, Fail, Blocked };

struct Diagnostic {
    std::string file; int line; int col;
    std::string severity;   // "error" | "warning" | "note"
    std::string message;
};

struct Verdict {
    Outcome                 outcome;
    std::string             stage;        // Provider 自定义："compile" / "run" / "lint"
    std::string             output;       // 给学员看的原始输出，保留 ANSI
    std::vector<Diagnostic> diagnostics;  // 可选，Provider 给不出就是空
};
```

### 三个关键决定

**`Blocked` 是独立的第三态。** 学员代码已经正确，但还有一个显式路障没拆（d2mcpp 里是 `D2X_WAIT` 宏）。它既不是失败也不该前进。当前实现把它塞进 `build_success=false`，同时 `status` 仍为 true，UI 显示成"成功但卡住"，语义是错的（`checker.cppm:88-97`）。

**`order` 显式化。** 当前顺序是 `get_targets()` 从 `std::map` 取 key 的字典序（`buildtools.cppm:45-51`），教学顺序成了命名的副作用——重命名一个练习会悄悄改变课程顺序。

**`diagnostics` 进核心模型但可选。** 框架的价值在呈现；能给出结构化诊断的 Provider，前端就能做行内高亮和跳转，给不出的退化成纯文本。

---

## 3. 下行 · Provider Protocol

### 进程模型

一次调用一个进程，参数走 argv，事件走 stdout 逐行 NDJSON：

```
<provider-cmd> describe
<provider-cmd> exercises
<provider-cmd> check <id>
```

Provider 命令来自课程配置（`.d2x.json` 的 `buildtools` 字段，语义扩展为"Provider 命令行前缀"）。进程间无状态，Provider 可自行在磁盘缓存。

实测启动开销 26ms（经 `mcpp run` 转发），不值得上常驻进程。事件流的形状使得将来换成常驻 JSON-RPC 只是换传输，领域模型不动。

### 命令与响应

```
describe   → {"event":"describe","protocol":1,"name":"mcpp"}

exercises  → {"event":"exercise","id":"...","order":0,"title":"...",
              "chapter":"cpp11/00-auto-and-decltype","files":["/abs/..."]}
             × N

check <id> → {"event":"stage","name":"compile"}
             {"event":"output","chunk":"...保留 ANSI..."}
             {"event":"stage","name":"run"}
             {"event":"output","chunk":"..."}
             {"event":"verdict","outcome":"pass|fail|blocked",
              "stage":"run","diagnostics":[...]}
```

### 为什么只有三个动词，没有 build / run / test

`build` + `run` 两段式是编译型语言的形状。一次 `check` 内部要编译几次、跑不跑测试、判定看退出码还是看输出里的 `❌`，全是课程的事。把两段式焊进通用框架就等于焊死了适用范围。

### 为什么是事件流而不是请求/响应

这一个决定同时解掉四个问题：

1. **不需要哨兵或"末行 JSON"约定** —— 每一行都是 JSON，解析不了的行直接忽略，正好吞掉启动器噪声。实测 `mcpp run -q` 会吐一个前导空行；若 Provider 需要重新编译，mcpp 的编译输出也会混进来。
2. **不需要 `prepare` 命令** —— 耗时准备就是 verdict 之前的一串 `stage` 事件。
3. **编译输出实时可见** —— 当前 d2x 用 `popen` 读到 EOF 才显示，全程黑屏。
4. **`2>&1` 混流不再是问题** —— `platform::run_command_capture` 硬编码了 `cmd + " 2>&1"`（三个平台实现都是），噪声天然被过滤。

### `output` 为什么走 JSON 字段

`check` 内部跑的编译器输出和练习程序输出是**给学员看的**，必须原样保留 ANSI 颜色。由 Provider 自己捕获后塞进事件，d2x 拿到的就是干净的结构化结果。这同时消除了当前"在混了 mcpp 横幅的文本里做 `❌` 子串搜索"的脆弱性。

### 暂不引入

`describe` 只返回 `protocol` + `name`。`capabilities` 协商等真有第二个 Provider 实现时再加。

---

## 4. 上行 · Frontend Protocol

**单向。** d2x 保留全部控制权：通过即自动前进，失败即等文件变更。前端纯显示。

```
{"event":"session","total":51,"completed":12,"current":"cpp11-04-rvalue-references"}
{"event":"exercise","id":"...","title":"...","chapter":"...","files":[...]}
{"event":"stage","name":"compile"}
{"event":"output","chunk":"..."}
{"event":"verdict","outcome":"blocked","diagnostics":[...]}
{"event":"waiting","reason":"file-change"}
{"event":"hint","text":"...AI 助手产出..."}
{"event":"done"}
```

### 两侧的关系

`stage` / `output` / `verdict` 三类事件**两侧同构**，d2x 对它们基本是转发 + 补上会话上下文。只属于上行的是 `session` / `exercise` / `waiting` / `hint` / `done`。

**下行是上行的子集**，不是两套无关的协议。

### 内置前端必须是同进程

`d2x checker` 对学员必须仍然是一条命令，不能让人先起引擎再起前端。所以内置 TUI/print 是**同进程客户端走内存通道**，与外部客户端走管道用同一套事件类型，只是换传输。现有的 `IUIBackend` + `UILoader`（`src/ui/ui_interface.cppm`、`src/ui/loader.cppm`）从"编译期插件"改造成"协议客户端"。

---

## 5. d2x 内部分层

```
src/domain.cppm            Exercise · Verdict · Diagnostic · SessionState    纯数据，零依赖
src/provider.cppm          IExerciseProvider
src/provider/process.cppm  ProcessProvider —— 起子进程，逐行读 NDJSON
src/session.cppm           Session —— 遍历/推进，纯逻辑
src/session/state.cppm     StateStore —— .d2x/state.json
src/watch.cppm             FileWatcher —— 去抖 + 自触发保护
src/emit.cppm              EventSink —— 上行事件发射（内存通道 或 stdout）
src/ui/**                  前端客户端，只消费 domain 类型
src/assistant.cppm         已有，产出 hint 事件
src/app.cppm               装配
```

**关键性质：`session/` 是纯逻辑。** 给它一个假 Provider 和内存状态，就能测完整学习流程，不碰文件系统、不碰构建工具、不碰终端。当前 `checker::run()` 是一个 100 行函数，把构建、判定、开编辑器、问 AI、刷 UI、等文件全缠在一起（`checker.cppm:28-131`），一行都测不了。

### 状态持久化

```
.d2x/state.json   { "current": "<id>", "completed": ["<id>", ...] }
```

**用 id 不用下标**——重排或重命名练习不会毁掉学员进度。这是 rustlings 的教训：它的 `.rustlings-state.txt` 同样按名字存，且"每次重编所有练习"的性能投诉（#121/#132/#1843）正是靠这个状态缓存解决的，与换构建工具无关。

---

## 6. 一次 check 的数据流

```
Session 取当前 Exercise
 └→ ProcessProvider 起 `<provider> check <id>`
      └→ Provider 写 .d2x/build/_current/mcpp.toml（只含这一题）
         mcpp build -p _current   → stage:compile + output 事件
         运行产物                  → stage:run + output 事件
         扫 ❌ / D2X_WAIT          → verdict 事件
 └→ d2x 逐行转发 stage/output，补 session 上下文，发给前端
 └→ pass          → StateStore 记完成 → 下一题
    fail/blocked  → 开编辑器（仅首次）→ 问 AI → hint 事件
                  → FileWatcher 等变更 → 重试
```

---

## 7. 错误处理

三类分开，各有明确行为：

| 类别 | 行为 |
|---|---|
| Provider 起不来 / `describe` 失败 | 致命，清晰报错退出 |
| 输出畸形（非 JSON 行） | 忽略该行并记 debug 日志 |
| 一次 check 完全没有 verdict 事件 | 当作 Fail，把原始输出原样呈现给学员 |
| 练习本身没通过 | 正常业务路径，不是错误 |

当前实现的两层误导要消除：`Failed to load targets with exit code: N`（`buildtools.cppm:83`）后面紧跟 `No targets found for checking.`（`checker.cppm:40`），学员看到的是"没有练习"而不是"Provider 挂了"。

### 两条现在缺失的防御

- **`files` 为空的练习**由 Session 层拒绝并跳过。当前 `checker.cppm:76` 无保护直接 `files[0]`，某个 target 列出零文件就崩。`d2x/docs/crash-analysis-d2x-in-d2mcpp.md:52` 声称加过保护，实际没有。
- **Provider 超时上限**，不能让 checker 无限挂住。

---

## 8. d2mcpp 侧：Provider 实现

### 形态

C++26 + mcpp 构建，位于 `d2x/buildtools/mcpp/`，是 d2mcpp 根 workspace 的一个成员。

```
d2mcpp/mcpp.toml                     [workspace] members = ["d2x/buildtools/mcpp"]   ← 提交
d2mcpp/d2x/buildtools/mcpp/          Provider 包，standard = "c++26"                 ← 提交
d2mcpp/.d2x/build/                   生成物                                          ← gitignore
    mcpp.toml                          独立 workspace 根
    cpp11/mcpp.toml                    全量 target，供 clangd
    _current/mcpp.toml                 只含当前一题，供 checker
```

Provider 包**不属于**生成的那个 workspace，避免循环依赖。

### 引导（零脚本、Windows 安全）

`.d2x.json`：

```json
{ "buildtools": "mcpp run -q -p d2x/buildtools/mcpp --" }
```

d2x 拼接后得到 `mcpp run -q -p d2x/buildtools/mcpp -- check <id>`。已实测：从仓库根、不 `cd`、参数正确传递、首次自动构建 Provider、暖开销 26ms。d2x 从不 `chdir`（`platform.cppm:12` 在命名空间静态初始化时捕获 CWD），所以必须免 `cd`。

注意 `-p` 匹配的是**目录 basename 或完整相对路径**，不是包名。

### 内部模块

```
discovery/  目录约定扫描 dslings/**/<章节>-<序号>.cpp → id / order / chapter / files
manifest/   生成 .d2x/build/{cppNN, _current}/mcpp.toml
runner/     调 mcpp build -p / run -p，捕获输出
verdict/    ❌ 与 D2X_WAIT 判定
emit/       NDJSON 事件输出
```

### 为什么是"双 member"

**逐题隔离是硬需求**：dslings 的练习默认就编译不过（49 个带 `D2X_YOUR_ANSWER`），而 mcpp 的 `build` 和 `run` 都会先全量构建整个包，一个坏兄弟拖垮全部且零产物。实测：

```
member 含全部 3 题（后两题未做完）→ mcpp build -p → exit=1     ← 当前这题被拖挂
适配器只生成当前这一题             → 0.071s，exit=0
```

member 粒度（标准/特性/练习）本身**不解决**逐题隔离。所以分两个：

- `cpp11/` 等按标准分的 member 持全量 target，供 clangd 拿到完整 `compile_commands.json`
- `_current/` 每次只写当前一题，checker 只构建它

实测动态改写清单代价极低：切到下一题 0.118s、切回上一题 0.018s，**fingerprint 目录始终只有 1 个**（改写 target 集合不会让缓存爆炸）。

### 零文件搬迁

`main` 可以用 `../` 逃逸出包根（已实测），所以练习源文件原地不动，生成的清单放 `.d2x/build/` 即可。

### C++ 标准

全部按 `c++23` 编译（决策：不改 mcpp 上游）。已知代价：`04-rvalue-references` 的移动构造教学点会被 C++17 保证复制省略静默抹掉（实测复现，51 个参考答案里只有这 1 个漂移）。该练习需要重写以在 C++17+ 下仍可观测移动，或改写书本章节说明。**这是本方案唯一的教学内容损失，必须单独跟进。**

---

## 9. 测试策略

| 层 | 方式 |
|---|---|
| `session/` | FakeProvider + 内存 StateStore，全流程无 IO 单测 —— **本次重构最大收益**，当前 0 覆盖 |
| `provider/` | 喂预录 NDJSON 流，专测容错：畸形行、缺 verdict、输出截断 |
| Provider 侧 | 假练习树，验证 id/order/chapter 推导与生成的清单 |
| 端到端 | 真 d2mcpp + 真 mcpp：**断言每个参考答案通过、每个练习不通过** |

端到端这条抄 rustlings 的 `cargo dev check --require-solutions`。它顺带补上 d2mcpp 那个静默空转的 CI：`dslings-ref-ci.yml` 只挑 `-ref` 结尾的 target，而 `solutions/` 在 `xmake.lua:6` 被注释掉，grep 返回空、循环全跳过、job 退出 0，实际校验零个 target。

---

## 10. 迁移与兼容

**不做 v1 回落。** 本设计是干净重做，不受现有 xmake 插件布局约束。现有 `xmake d2x-buildtools` 插件在新协议下不再工作；d2mcpp 切到新 Provider 后 xmake 路径整体退役。

其他仍在用 v1 的课程仓库需要各自实现 Provider——这正是"具体工具由具体项目实现"的定位所要求的。

---

## 11. 实现与验证结果（2026-07-19）

分支：d2x `feat/exercise-framework-protocol` · d2mcpp `feat/mcpp-provider`

### 已实现

**d2x**（新增 3 个模块，删除 `buildtools.cppm`，重写 `checker.cppm`）

| 文件 | 内容 |
|---|---|
| `src/domain.cppm` | Exercise / Outcome 三态 / Diagnostic / Verdict |
| `src/provider.cppm` | `IExerciseProvider` + `ProcessProvider`（NDJSON 解析、非 JSON 行静默丢弃） |
| `src/session.cppm` | `StateStore`（`.d2x/state.json`，按 id）+ `Session`（定位起点、推进） |
| `src/platform*.cppm` | 新增 `run_command_lines` 流式逐行读，并修正 `pclose` 的 wait status 解码 |
| `src/checker.cppm` | 只剩编排，从 100 行缠绕逻辑降为分层调用 |

**d2mcpp Provider**（C++26 + mcpp，`d2x/buildtools/mcpp/`）

`emit.cppm`（NDJSON + JSON 转义）· `discovery.cppm`（目录约定 + `// d2x:cxxflags:` 就近指令）· `manifest.cppm`（双 member 生成，内容比对后才落盘）· `runner.cppm`（调 mcpp、三态判定）· `tests/e2e.sh`

### 实测结果

| 验证项 | 结果 |
|---|---|
| Provider 枚举 | 52 个练习（1 hello + 49 cpp11 + 2 cpp14，与调研数一致） |
| 三态判定 | 未完成 → `fail@compile`；参考答案 → `pass@run`；答案对但留 `D2X_WAIT` → `blocked@run` |
| **端到端断言** | **51/51 参考答案通过，0 失败**；每个未完成练习都正确不通过 |
| d2x 全链路 | Provider 加载 → 52 题枚举 → `[compile]` 阶段**实时显示** → 编译错误呈现 → 等待文件变更（退出 124 为健康） |
| 推进与持久化 | 放入 6 份参考答案后自动连推 6 题，`current` 前进到下一道未完成练习 |
| 断点续做 | 重启后进度条 `6/52`，直接从 `cpp11-01-default-and-delete-0` 开始 |

### 实现中发现的两个真实缺陷

**1. `mcpp run` 向子进程泄漏 `LD_LIBRARY_PATH`，导致嵌套 mcpp 段错误。**

Provider 由 `mcpp run` 启动时，mcpp 会把 `LD_LIBRARY_PATH` 指向它私有的 glibc
（`~/.mcpp/registry/data/xpkgs/xim-x-glibc/2.39/lib64`）并注入子进程。Provider 接着
spawn 嵌套的 `mcpp`（另一个二进制）时被迫加载错配的 glibc，在动态链接器里段错误，
输出里只留下 `<pid>:\t__vdso_time` 这样的 trace 残片。

冷启动稳定复现 3/3；直接执行 Provider 二进制则 3/3 通过——这是决定性对照。

修复：`runner.cppm` 在每次 spawn 前 `unsetenv("LD_LIBRARY_PATH")`，mcpp 会为它自己的
子进程重新设置正确的值。d2x 侧对同一问题早有相同处理（`platform.cppm` 的
`run_command_capture`，仓库里还留着 `workaround_ld_library_path_issue` 分支）——
**说明这是 mcpp 的既有问题，值得单独向上游报。**

**2. 冷启动时 `check` 找不到 workspace。**

`check` 原本只写 `_current/mcpp.toml`，而根清单由 `exercises` 写。全新仓库上学员
直接跑 `d2x checker` 时根清单尚不存在，mcpp 以退出码 2 报 `workspace member not found`。
修复：`check` 先 `write_full` 再 `write_current`；两者都做内容比对后才落盘，
重复调用不会推进 mtime、不会让 mcpp 的快速路径失效。

### 未完成

- **`session/` 的单测尚未编写。**「纯逻辑可单测」是本次重构的最大收益，但目前只有端到端验证，FakeProvider 单测还没落地。
- **`diagnostics` 只走通了协议管道，Provider 还没真正产出。** 需要解析编译器输出（或改用 `-fdiagnostics-format=json`）才能填充，前端的行内高亮也就还没兑现。
- **前端仍是编译期插件。** 上行 Frontend Protocol 已在设计中定稿，但内置 TUI/print 尚未改造成协议客户端，目前仍直接调 `ui::update_checker_page`。
- **文件监听未改造。** 仍是 `utils::wait_files_changed` 轮询 mtime，去抖与自触发保护还没做。
- **`04-rvalue-references` 的教学漂移未处理。** 见第 8 节。

---

## 12. 第二轮：功能补齐与缺陷修复（2026-07-20）

### 补齐的功能

| 功能 | 位置 | 说明 |
|---|---|---|
| 上行 Frontend Protocol | `d2x/src/emit.cppm` | 单向 NDJSON。内置 TUI 走 `UiSink`（内存通道），外部客户端走 `StdoutSink`（管道），同一套事件类型。`--emit-events` 启用 |
| 文件监听 | `d2x/src/watch.cppm` | 按文件记 mtime+size、内置安静期去抖、`resync()` 自触发保护 |
| session 单测 | `d2x/tests/session_test.cpp` | 28 个断言，无 IO 覆盖完整学习流程 |
| 教学漂移修复 | `dslings/**/04-rvalue-references.cpp` | 改用具名对象 `std::move`，并加断言钉住 |

原先的监听实现「把所有文件 mtime 相加再比总和」有三个问题：求和会抵消（两文件一增一减则漏检）；无去抖（编辑器多次写入会读到半截文件）；去抖手写在调用方。

### 单测立刻抓到的设计缺陷

起点优先级是「显式指定 > 持久化 current > 第一个未完成」。这条本身是对的——学员主动跳级后重启不该被硬拉回开头。但副作用是：**课程作者在学员当前位置之前插入新练习，那道题会被永久静默跳过**。

修法不是回退优先级，而是让推进逻辑走到末尾时绕回去回收遗漏的练习（`Session::advance_to_next_incomplete`）。学员不被打断，内容也不丢。

### 对抗性审查发现的三个真缺陷

**1. 练习 id 注入（严重）。** id 直接取自文件名，有两个危险去向：d2x 把它拼进 shell 命令交给 `popen`，Provider 把它写进生成的 TOML（`[targets.<id>]`）。带反引号、`]` 或引号的文件名在任一处都能越界——对社区课程仓库而言，一个恶意 PR 文件名就足以在任何跑 checker 的人机器上执行命令。

在 `discovery.cppm` 源头做白名单校验并**拒绝**，而不是在两个下游各自转义；d2x 侧同时加 shell 引用做纵深防御。实测 `` 99-evil`touch pwned_marker`.cpp `` 被拒绝、命令未执行。

**2. `e2e.sh` 把所有英文参考答案静默 SKIP。** 前缀剥离顺序错了——`${sol#en/}` 执行时 `sol` 已经以 `solutions/` 开头，匹配不到任何东西，是个静默 no-op。

**这正是本脚本存在的理由所要防的那种空转，和旧 CI 一模一样的毛病。** 除修顺序外另加防线：`pass == 0` 直接判失败，杜绝「0 失败」蒙混。修复后 en 也是 51/51 真验证（此前 0 通过 / 52 跳过）。

**3. `d2x_assert_eq` 的日志分支仍用裸 `std::to_string`**，而上报分支已改用 SFINAE 安全的 `show()`。`std::to_string` 没有 `std::string` / `const char*` / scoped enum 的重载——下一个比较字符串或强类型枚举的练习会直接编译失败。`show()` 存在的意义就是避免这个，却只用了一半。

### 其他修复

- `DEFAULT_BUILDTOOLS` 从 `"xmake d2x-buildtools"` 改为空。xmake 已退役，留着会让未配置的仓库拿到必定失败的命令，报错还指向 xmake。
- `read_source` 包住读文件异常。原先无保护，练习文件读不到就整个会话崩。
- `--emit-events` 模式下日志改道 stderr。实测修复前有 5 行日志混进事件流。
- `e2e.sh` 增加脏树前置检查。该脚本会把参考答案覆盖到练习上再还原，天然会吃掉练习目录里未提交的改动——**这个陷阱咬过两次**（一次丢了脚手架，一次丢了刚修好的练习）。现在不干净就拒绝运行并列出文件。

### 当前验证状态

| 项 | 结果 |
|---|---|
| d2x session 单测 | 28/28 |
| Provider 端到端（zh） | 51/51 参考答案通过 |
| Provider 端到端（en） | 51/51 参考答案通过 |
| TUI 全链路 | 正常，进度 0/52，健康挂起 |
| 事件流全链路 | 12 行 JSON，**0 行污染** |
| 注入防护 | 恶意文件名被拒绝，命令未执行 |

### 仍然欠着的

- **macOS / Windows 从未验证。** Windows 尤其存疑：`_popen`、`_putenv_s`、`unsetenv` 的 `#ifdef` 分支、`shell_quote` 的 cmd.exe 分支，全是纸面推断。
- **新 CI 从未真跑过。** workflow 是手写的，`xlings install -y` 在 CI 环境能否装上 mcpp 未验证。
- **`--ui print` 参数不生效**——`.d2x.json` 的 `ui_backend` 覆盖了 CLI 参数（d2x 既有问题）。因此 print 后端路径未被真正验证。
- **`diagnostics` 只在断言失败时产出**，编译错误尚未解析成结构化诊断（需要 `-fdiagnostics-format=json` 或解析编译器输出）。
- **模块化练习的填空占位符没有约定。** `D2X_YOUR_ANSWER` 是宏，无法跨模块导出；cpp20/cpp23 章节需要另设方案。
