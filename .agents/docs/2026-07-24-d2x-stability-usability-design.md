# d2x 稳定性与可用度优化设计

- 日期：2026-07-24
- 分支：`feat/exercise-framework-protocol`
- 关联：[`2026-07-20-d2x-architecture-reference.md`](2026-07-20-d2x-architecture-reference.md)（架构参考，其 §9 缺口本文逐条消化）
- 方法：**实测证据驱动**——问题清单全部来自本周 d2mcpp「练习即测试」端到端接入期间的真实观察，不做臆测式加固
- 本文定位：设计方案（未实现），核心点供 review 后拆计划

---

## 1. 现状盘点

| 模块 | 健康度 | 依据 |
|---|---|---|
| session/ | ✅ 有 28 断言单测；按 id 持久化、遗漏回收 | 架构参考 §5 |
| watch/ | ⚠️ 无测；去抖/自触发保护逻辑正确（实测静置 40s 零输出） | §6 |
| provider/ | ⚠️ 无测；**无任何超时**；NDJSON 容错未被自动化覆盖 | §9 |
| emit/ | ⚠️ 无测；StdoutSink 逐行 flush ✓ | 代码 |
| ui/print | ⚠️ 渲染尾 flush 刚修（13863df）；词汇/布局遗留 | 本周实测 |
| ui/tui | ⚠️ ftxui 帧渲染；未审计中断恢复 | 代码 |
| xlings 集成（install/new/book/list） | ❌ 多处脆弱点（§2 S3/S5/S10） | 代码审读 |

## 2. 问题清单（编号 = 后文方案引用；★ = 本周真实咬过人）

**稳定性**

- **S1 Provider 无超时**：describe/exercises/check 任一挂死 → checker 永久挂住。课程 Provider 冷启动（如 `mcpp run` 首次要构建 Provider）可达分钟级，**固定总时长超时是错的设计**。
- **S2 并发实例互踩 ★**：两个 checker（或一个泄漏实例）同时监听同一仓库——本周一个泄漏的 `d2x checker` 在 e2e 覆盖答案时并发触发重建，与外部构建竞争产物目录，测试二进制被替换瞬间报 exit 127。无实例锁。
- **S3 `d2x install` 链路五个脆弱点**：① `xlings install d2x:<pkg>` 不带 `-y`（非交互环境卡在确认提示）；② `ensure_xlings_installed()` 返回值被忽略（用户拒绝安装后照样往下执行）；③ `<pkg>` 未做白名单校验直接拼 shell 命令；④ 失败只打退出码，无原因分类与指引（网络？索引过期？建议 `xlings update`/`--mirror CN`？）；⑤ 成功后不校验结果（目标目录 + `.d2x.json` 存在）、无下一步提示。
- **S5 `has_xlings()` 误判**：`regex_match` 要求输出**整体**恰为 `xlings x.y.z`——xlings 将来多打一行 banner 就误判未安装，触发重装流程。
- **S6 state.json 损坏 = 静默清零进度**：解析失败走 `is_discarded → return`，不崩溃但**进度丢失且无告警**。应为：损坏文件改名备份 + 明确告知 + 从空白继续。
- **S12 写入原子性**：state.json 的保存若非「临时文件 + rename」，断电/并发可产生半写文件（正是 S6 的来源）。

**显示 / UX**

- **S4 stdout 契约不完整 ★**：print 页面缓冲滞留（已修 13863df），但「TTY=人读 / 非 TTY=必须逐行或逐页 flush」尚未成为全 UI 层的显式契约与测试项。
- **S7 词汇漂移**：领域层早已改名 exercise，但 `ICheckerPageUI::UIState` 仍是 `target/built_targets/total_targets`，页面打 `Target: hello-mcpp`、`---------E-Files---------` 等旧格式——接口把旧词汇固化了。
- **S8 长输出淹没**：编译错误全量塞进页面，几百行时关键信息（结构化 diagnostics、首个错误）被推出屏幕。
- **S9 i18n 混杂**：UI/日志中英夹杂（`Provider: mcpp` + 中文错误提示）；`lang` 只切练习集不切界面文案。
- **S10 长操作静默 ★**：`d2x book` 内部 `xlings install mdbook -y` 走 `run_command_capture`——下载几十秒期间用户面对完全静止的终端。
- **S11 可见性缺口**：没有不进入监听循环的进度总览（哪些完成/当前在哪/一共多少），学习者只能进 checker 才知道。

**质量基建**

- **S13 无自有端到端测试 ★**：d2x 的协议行为（describe 失败、缺 verdict、畸形行、blocked 三态、文件变更推进）只被 d2mcpp 的 CI 间接覆盖——d2x 自身回归要靠下游发现。

## 3. 设计方案

### D1 Provider 活性超时（治 S1）

固定总时长会误杀冷启动，正确模型是**活性（liveness）**：

```
自上次收到任何输出行起 > idle_timeout 秒 → 判定挂死,SIGKILL,
verdict 缺失走既有「无 verdict = fail + 原样呈现已收输出」路径。
```

- 平台层 `run_command_lines` 增加带 idle-deadline 的变体（POSIX：poll + 读循环 + WNOHANG，对照 mcpp `capture_exec_deadline` 的实现经验；Windows 暂记录为尽力而为）。
- 默认 `idle_timeout = 120s`（覆盖最慢的单文件编译间隙），`.d2x.json` 可配 `provider_idle_timeout`；describe/exercises 与 check 共用同一机制。
- 杀死后 UI 明示「Provider 无响应已终止（120s 无输出）」而非无限转圈。

### D2 checker 单实例锁（治 S2）

- `.d2x/checker.lock` 写入 pid + 启动时间；启动时读锁：进程存活（`kill(pid,0)`/Windows OpenProcess）→ 拒绝启动并提示「另一 checker (pid N) 正在运行」；进程已死 → 视为陈旧锁自动接管。
- 正常退出与信号路径（SIGINT/SIGTERM handler）删除锁；TUI 退出路径一并审计终端状态恢复。

### D3 `d2x install` 健壮化（治 S3/S5）

1. 包名白名单 `[A-Za-z0-9._-]`，非法即拒绝（与练习 id 同一纪律）。
2. `ensure_xlings_installed()` 失败/拒绝 → 立即返回，不再继续。
3. `has_xlings`：`regex_match` → `regex_search`；另以 `get_xlings_bin()` 存在性为先导判据（已有）。
4. 命令加 `-y`；检测 `!isatty(stdin)` 时强制非交互。
5. 失败分类：按 xlings 退出码/输出关键词给出**下一步指令**（`xlings update` / `xlings config --mirror CN` / 网络检查），而不是裸状态码。
6. 成功后校验 `<pkg>/.d2x.json` 存在，打印「cd <pkg> && d2x checker」引导；目标目录已存在时先问覆盖/更新语义。
7. `d2x new` 同标准对齐（模板 rename 失败时清理残留）。

### D4 stdout 契约成文 + 测试（治 S4/S10）

- 契约：**stdout 三种角色互斥**——协议流（--emit-events，逐行 flush）｜人读页面（每次渲染整页后 flush）｜透传子进程输出（不捕获）。log 一律 stderr。
- `d2x book`/install 的长操作从 `run_command_capture` 改为透传（用户看得见 xlings 自己的进度条）。
- 回归项进 D7 的 e2e：`checker --ui print > file` 断言文件含页面内容（防 13863df 复发）。

### D5 呈现层重构（治 S7/S8/S9）

- `ICheckerPageUI::UIState` 字段改名：`exercise / files / completed / total`（编译期接口，print/tui 两个内置后端同步改；插件协议客户端化仍是架构参考 §9 的独立缺口，本次不动协议）。
- 页面固定分区（print 与 tui 同构）：

```
Progress [====>----] 12/52          ← 单一进度行
Exercise: cpp11-04-rvalue-references (cpp11/04-rvalue-references)
Status:   ❌ 编译失败 | 🚧 已通过,待拆路障 | ✅ 通过
Checks:   （结构化 diagnostics 置顶,最多 5 条,file:line + message）
Output:   头 20 行 + «… 省略 N 行,完整输出见 .d2x/last-output.log» + 尾 30 行
Hint:     AI 提示（未启用则单行说明）
```

- 全量输出落 `.d2x/last-output.log`（每次 check 覆写）——截断不丢信息。
- 文案走消息目录（zh/en 两份，键控），随 `lang`；默认 en，`lang=zh` 全中文。

### D6 `d2x status`（治 S11）

只读命令：Provider `exercises` + state.json → 输出总览（完成数/当前/各章节完成度），不构建、不监听。`--emit-events` 下输出对应 JSON。列表长时按章节聚合。

### D7 fake-provider 测试基建（治 S13，兼 §9 三个无测模块）

- `tests/fake-provider.sh`：≤50 行 bash，按 argv 输出可配置的协议脚本（正常流/describe 失败/无 verdict/畸形行/挂死 sleep/blocked）。
- e2e（不依赖 mcpp/d2mcpp）：
  1. 正常闯关：fake 先 fail 后 pass，改文件触发推进，断言 state.json；
  2. 协议容错：畸形行忽略、缺 verdict=fail、describe 失败=明确报错；
  3. D1 超时：挂死 provider 在 idle_timeout 后被杀且 UI 有明示；
  4. D2 锁：双实例第二个被拒；
  5. D4 flush：重定向下页面内容存在。
- provider/emit/watch 的纯逻辑部分补单测（解析、转义、去抖计时可注入时钟）。

### D8 状态写入原子化（治 S6/S12）

- StateStore 保存：写 `state.json.tmp` + `rename`；加载失败：原文件改名 `state.json.corrupt-<ts>` + stderr 告警 + 空状态继续。

## 4. 路线图

| # | 项 | 治 | 验收 |
|---|---|---|---|
| P1 | D1 活性超时 + D2 单实例锁 | S1/S2 | fake-provider 挂死 e2e;双实例 e2e |
| P2 | D3 install 健壮化 | S3/S5 | 非交互安装全流程;失败注入出指引文案 |
| P3 | D8 状态原子化 | S6/S12 | 损坏注入 e2e:进度备份+告警+可继续 |
| P4 | D7 fake-provider 基建 | S13 | e2e 独立于 mcpp 全绿,进 CI |
| P5 | D4 stdout 契约 + 透传 | S4/S10 | 重定向断言;book 安装可见进度 |
| P6 | D5 呈现重构 | S7/S8/S9 | 新布局截图对照;书中示例同步 |
| P7 | D6 status 子命令 | S11 | 只读、亚秒返回 |

依赖：P1 的平台层 deadline 变体是 P4 超时用例的前置；P6 会改动 d2mcpp 书中的控制台示例（跨仓库联动，放最后与 d2x 发版一起做）。

## 5. 兼容性与不动项

- **双向协议一个字段不动**（Provider Protocol / Frontend Protocol）；D5 仅改编译期内置接口与文案。
- `platform.cppm` 的 `LD_LIBRARY_PATH` 清理**保留**（mcpp 0.0.104 虽已根治 mcpp 链路，但它还保护 editor 等非 mcpp 子进程）。
- 「前端插件协议客户端化」「多文件练习 files.front()」「编译错误结构化」仍挂在架构参考 §9，不并入本批（范围控制）。

## 6. 风险

- D1 的 idle 判定依赖 Provider 有持续输出——一个合法但完全静默 3 分钟的 Provider 会被误杀；120s 默认 + 可配置 + 杀前 UI 倒计时提示作为缓解。
- D5 改 UIState 是编译期破坏性改动——print/tui 同仓库同步改,无外部插件消费者（现状），成本可控。
- Windows：D1/D2 的进程探活与杀死需要 `_WIN32` 分支，延续「纸面推断」风险——与架构参考 §9 的 Windows 验证缺口一起在发版前过一轮。
