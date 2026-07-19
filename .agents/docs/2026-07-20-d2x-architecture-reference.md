# d2x 架构参考

- 日期：2026-07-20
- 分支：`feat/exercise-framework-protocol`
- 配套设计文档：[`2026-07-19-exercise-framework-protocol-design.md`](2026-07-19-exercise-framework-protocol-design.md)（决策过程与理由）
- 本文定位：**当前实现的参考手册**——协议规范、模块职责、扩展方式、已知缺口

---

## 1. d2x 是什么

**练习驱动学习的通用框架：它拥有学习循环和它的呈现，除此之外什么都不拥有。**

d2x 不知道 C++、不知道 mcpp、不知道怎么编译任何东西。它向下用协议对接课程，向上用协议对接前端。

```
   前端：内置 TUI / print / VSCode 插件 / Web / CI
                    ↑  Frontend Protocol   (NDJSON，单向)
   ┌────────────────────────────────────────────┐
   │  d2x core  —— 学习循环 · 会话状态 · 文件监听 · 编排  │
   └────────────────────────────────────────────┘
                    ↕  Provider Protocol   (NDJSON)
   课程侧：由具体课程实现（如 d2mcpp 的 C++26 Provider）
```

| d2x 拥有 | 归属他方 |
|---|---|
| 学习循环的编排 | 构建工具、编译命令 |
| 会话状态与断点续做 | 通过判定规则 |
| 文件监听与去抖 | 练习内容、顺序、章节 |
| 事件的产生与分发 | 具体渲染方式 |

---

## 2. 下行 · Provider Protocol

课程侧实现的唯一扩展点。命令来自 `.d2x.json` 的 `buildtools`（语义是「Provider 命令行前缀」）。

### 三个动词

```
<provider-cmd> describe
<provider-cmd> exercises
<provider-cmd> check <id>
```

**刻意没有 build / run / test。** 那是编译型语言的形状，焊进通用框架就焊死了适用范围。一次 `check` 内部编译几次、跑不跑测试、判定看退出码还是看输出，全是课程的事。

### 事件格式

每行一个 JSON 对象，写到 stdout。**解析不了的行由 d2x 静默忽略**——这不是宽容，是协议设计的一部分：Provider 常经由启动器间接执行（如 `mcpp run`），启动器会往 stdout 混入空行乃至编译输出。忽略非 JSON 行让噪声天然失效，于是不需要哨兵前缀或「末行即 JSON」这类隐式约定。

```jsonc
// describe
{"event":"describe","protocol":1,"name":"mcpp"}

// exercises —— 每题一行
{"event":"exercise","id":"cpp11-00-auto-and-decltype-0","order":1100000,
 "title":"auto and decltype (0)","chapter":"cpp11/00-auto-and-decltype",
 "files":["/abs/path/to/exercise.cpp"]}

// check —— 按发生顺序流式输出
{"event":"stage","name":"compile"}
{"event":"output","chunk":"...保留 ANSI 的原始输出..."}
{"event":"stage","name":"run"}
{"event":"output","chunk":"..."}
{"event":"verdict","outcome":"pass|fail|blocked","stage":"run","exit_code":0,
 "diagnostics":[{"file":"/abs/path","line":33,"col":0,
                 "severity":"error","message":"断言未通过: ..."}]}
```

### 字段约定

| 字段 | 要求 |
|---|---|
| `id` | 稳定标识，完成状态按它持久化。**必须 shell 安全**——d2x 会做引用，但 Provider 应在源头校验并拒绝异常字符 |
| `order` | 显式顺序。d2x 按它排序，不依赖 id 的字典序 |
| `files` | **绝对路径**。d2x 用它打开编辑器、监听变更 |
| `diagnostics[].file` | **绝对路径**，同上。即使展示用相对路径，协议边界上也要还原 |
| `outcome` | 三态，见下 |

### 三态 outcome

| 值 | 含义 | d2x 行为 |
|---|---|---|
| `pass` | 通过 | 标记完成，推进下一题 |
| `fail` | 未通过 | 等文件变更后重试 |
| `blocked` | **代码已正确，但还有显式路障未拆**（如 d2mcpp 的 `D2X_WAIT`） | 同 `fail`，但前端可区分呈现 |

`blocked` 是独立态而非布尔的一部分——旧实现把它塞进 `build_success=false` 而 `status` 仍为 true，UI 显示成「成功但卡住」，语义是错的。

### Provider 实现要点

- **进程无状态**，可自行在磁盘缓存。实测启动开销约 26ms（经 `mcpp run` 转发），不值得上常驻进程。
- **`check` 没有 verdict 事件** = Provider 中途死了或输出被截断。d2x 一律当作 `fail` 并把原始输出原样呈现，绝不当成通过。
- **`describe` 失败是致命错误**，d2x 明确报「Provider 挂了」而非「没有练习」。
- 事件流的形状使得将来换成常驻 JSON-RPC 只是换传输，领域模型不动。

---

## 3. 上行 · Frontend Protocol

**单向。** d2x 保留全部控制权（通过即自动前进，失败即等文件变更），前端纯显示。

`--emit-events` 启用；stdout 是纯 NDJSON 协议流，**日志全部改道 stderr**。

```jsonc
{"event":"session","total":52,"completed":12,"current":"cpp11-04-rvalue-references"}
{"event":"exercise","id":"...","order":0,"title":"...","chapter":"...","files":[...]}
{"event":"stage","name":"compile"}
{"event":"output","chunk":"..."}
{"event":"verdict","outcome":"blocked","stage":"run","diagnostics":[...]}
{"event":"waiting","reason":"file-change"}
{"event":"hint","text":"...AI 助手产出..."}
{"event":"done"}
```

### 两侧的关系

`stage` / `output` / `verdict` **两侧同构**，d2x 对它们基本是转发 + 补会话上下文。只属于上行的是 `session` / `exercise` / `waiting` / `hint` / `done`。

**下行是上行的子集**，不是两套无关的协议。

### 内置前端是同进程客户端

`d2x checker` 对学员必须仍是一条命令，不能让人先起引擎再起前端。所以内置 TUI/print 走内存通道（`UiSink`），外部客户端走管道（`StdoutSink`），二者消费同一套事件类型，只是换传输。

**`--emit-events` 模式下 d2x 不打开编辑器**——外部前端已从事件流拿到 `exercise` 和 `verdict`，开不开、怎么开是它的决定，两边都动只会打架。

---

## 4. 模块职责

```
src/domain.cppm     Exercise · Outcome · Diagnostic · Verdict     纯数据，零依赖
src/provider.cppm   IExerciseProvider + ProcessProvider           唯一下行扩展点
src/session.cppm    StateStore + Session                          纯逻辑，可单测
src/watch.cppm      FileWatcher                                   去抖 + 自触发保护
src/emit.cppm       IEventSink + StdoutSink + UiSink              上行协议
src/checker.cppm    编排                                          只发事件，不拼页面
src/editor.cppm     可配置的编辑器策略
src/config.cppm     配置加载与优先级
```

**关键性质：`session/` 是纯逻辑。** 给它假 Provider 和内存状态就能测完整学习流程，不碰文件系统、不碰构建工具、不碰终端。`tests/session_test.cpp` 有 28 个断言。

### 用词

领域对象叫 **exercise**，不叫 target。`target` 是构建工具的词汇，让它泄漏进领域层正是旧设计的问题所在——d2x 是课程工具。

---

## 5. 会话状态

```
.d2x/state.json   { "current": "<id>", "completed": ["<id>", ...] }
```

**按 id 存，不按下标。** 重排或重命名练习不会毁掉学员进度。这是 rustlings 的经验（其 `.rustlings-state.txt` 同样按名字存）。

### 起点优先级

```
显式指定（d2x checker <substr>） > 持久化 current > 第一个未完成 > 开头
```

### 遗漏回收

推进到末尾时会**绕回去找未完成的练习**（`Session::advance_to_next_incomplete`）。

原因：起点优先「持久化 current」是对的（学员主动跳级后重启不该被硬拉回开头），但副作用是课程作者在学员当前位置**之前**插入新练习时，那道题会被永久静默跳过。绕一圈保证「学员不被打断，内容也不丢」。这个缺陷是 session 单测发现的。

---

## 6. 文件监听

`src/watch.cppm`。按文件记 `mtime + size`，内置安静期去抖，提供 `resync()` 做自触发保护。

**检查完全由文件变更驱动。** 等待窗口到期只是继续等，绝不重新构建——早先的实现在窗口到期后无条件重跑，结果 TUI 每 20 秒自己刷一屏、白白重编一遍，学员什么都没做却看到界面在动。实测修复后静置 40 秒零输出。

替换掉的旧实现有三个问题：把所有文件 mtime **相加比总和**（两文件一增一减则互相抵消，改动被漏掉）；无去抖（编辑器多次写入会读到半截文件）；去抖手写在调用方。

---

## 7. 配置

`.d2x.json`（本地）与 `~/.d2x.json`（全局）。

| 键 | 说明 |
|---|---|
| `buildtools` | **Provider 命令行前缀。无默认值**——Provider 是课程特有的，必须由课程仓库声明 |
| `ui_backend` | `tui` / `print` |
| `lang` | 课程语言，透传给 Provider |
| `editor` | 编辑器命令。支持 `{file}` 占位符；**显式配空串 = 关闭**；未配置时按 `$VISUAL` → `$EDITOR` → `code` 回退 |
| `llm` | AI 助手配置 |

### 优先级

```
命令行 > 环境变量 > 本地配置 > 全局配置 > 默认值
```

命令行参数是通过写环境变量传进来的（`cmdprocessor::apply_global_options`），所以环境变量必须**覆盖**配置文件而非「只填空缺」——原先是后者，导致 `.d2x.json` 反过来压住了 `--ui` / `--lang`。

---

## 8. 安全约定

**练习 id 来自课程仓库的文件名，是不可信输入。** 它有两个危险去向：d2x 拼进 shell 命令交给 `popen`；Provider 可能写进生成的构建清单。带反引号、`]`、引号或换行的文件名在任一处都能越界——对社区课程仓库而言，一个恶意 PR 文件名就足以在任何跑 checker 的人机器上执行命令。

**纵深防御：**
- d2x 侧：`ProcessProvider` 对 id 做 shell 引用（POSIX 单引号包裹；Windows 拒绝含引号的 id）
- Provider 侧：应在发现阶段用白名单校验并**拒绝**，而不是想办法安全地传递

---

## 9. 已知缺口

| 缺口 | 影响 |
|---|---|
| **macOS / Windows 从未验证** | Windows 尤其存疑：`_popen`、`_putenv_s`、`shell_quote` 的 cmd.exe 分支全是纸面推断 |
| **`provider/` 层无单测** | 只有 session 层有。NDJSON 解析的容错（畸形行、缺 verdict、输出截断）未被自动化覆盖 |
| **`emit` / `watch` 层无单测** | 同上 |
| **`diagnostics` 只覆盖运行期断言** | 编译错误尚未解析成结构化诊断，需要 `-fdiagnostics-format=json` 或解析编译器输出 |
| **前端仍是编译期插件** | `IUIBackend` + `UILoader` 尚未真正改造成协议客户端，`UiSink` 是适配层而非重构 |
| **Provider 无超时上限** | Provider 挂死会让 checker 一起挂住 |
| **多文件练习支持不完整** | `Exercise.files` 是数组，但 AI 助手只读 `files.front()` |

---

## 10. 相关记录

- 决策过程与替代方案对比：`2026-07-19-exercise-framework-protocol-design.md`
- xmake → mcpp 的调研（含 rustlings/cargo 横向对照）：d2mcpp 仓库 `.agents/docs/2026-07-19-mcpp-replace-xmake-research.md`
- Provider 实现范例：d2mcpp 仓库 `.agents/docs/2026-07-20-mcpp-provider-reference.md`
