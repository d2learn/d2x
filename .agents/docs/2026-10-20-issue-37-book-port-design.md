# Issue #37：book 端口配置与自动避让方案

状态：已进入实现与验收。设计分析基线：`7dde6cf`（2026.08.02.2）。

## 1. 建议结论

为 `d2x book` 增加 `-p/--port`，支持环境变量和配置文件；**未指定端口时自动避让，指定端口时严格使用**。默认从 3000 开始，最多探测 100 个端口（3000–3099），选定后交给 mdBook 启动。

实现只分三层：命令入口负责串联，Config 负责取值，platform 负责原生 socket 探测。无需引入网络库、后台守护进程或通用重试框架。

## 2. 问题与现状

[Issue #37](https://github.com/d2learn/d2x/issues/37) 反馈：本机 3000 被占用，`d2x book` 无法通过参数调整，最终 mdBook 绑定失败。Issue 提议增加 CLI、配置和环境变量入口，并希望未配置时继续使用 mdBook 自身默认值。

当前代码核对如下：

| 位置 | 现状 | 对方案的影响 |
| --- | --- | --- |
| `src/cmdprocessor.cppm` 的 `book` | 直接执行 `mdbook serve --open <bookdir>`，无端口选项 | 增加命名选项及端口决策 |
| 同一 action | 安装与服务命令的返回值均被忽略 | 安装失败立即停止；服务失败让 d2x 非零退出 |
| 同一 action | 保留 `book/<lang>` 查找和 `book/` 回退；路径直接拼接 | 保留语言行为，修复路径作为单个 shell 参数传递 |
| `src/config.cppm` | 本地加载后，仅在 UI/LLM 条件满足时合并全局配置 | 不能只加字段，否则全局 `book_port` 可能不生效 |
| 同一模块 | 交互配置单独读取、重建并保存 JSON | 新字段必须贯穿读写，避免运行 `d2x config` 后丢失 |
| `src/platform.cppm` | `exec()` 返回 `std::system()` 原始结果 | POSIX 的 wait status 不能直接作为 main 的退出码 |
| cmdline 0.0.2 对应本地源码 | action 为 `void`，`App::run()` 执行成功解析后返回 0 | lambda 改成返回 int 并不能实现错误码传递 |

外部核对：mdBook [0.4.43](https://github.com/rust-lang/mdBook/blob/v0.4.43/src/cmd/serve.rs) 和 [0.5.2](https://github.com/rust-lang/mdBook/blob/v0.5.2/src/cmd/serve.rs) 的 serve 均提供 `-p/--port`，默认值为 3000；监听地址默认为 `localhost`。两版均先解析地址，再启动监听；热更新 WebSocket 与 HTTP 共用端口，不需要额外选择一个热更新端口。

Issue 所述项目的 xlings shim / PATH 行为属于报告中的环境信息；本次未在该课程环境复现。方案继续通过现有 xlings 调用链启动 mdBook，无需替换 shim 或升级 mdBook。

## 3. 对外行为

```bash
d2x book                    # 3000 空闲则使用；被占则尝试 3001、3002……3099
d2x book --port 8080         # 严格使用 8080
d2x book -p 8080             # 同上
D2X_BOOK_PORT=8080 d2x book  # 严格使用 8080（POSIX 示例）
```

配置延续现有平铺风格：

```json
{ "book_port": 8080 }
```

端口优先级：**CLI > `D2X_BOOK_PORT` > 项目 `.d2x.json` > 用户 `~/.d2x.json` > 自动模式**。

| 情况 | 行为 |
| --- | --- |
| 没有任何端口配置 | 从 3000 顺序探测至 3099，使用第一个可用端口 |
| CLI、环境变量或配置指定了端口 | 只检查该端口；占用则报错，不自动修改明确配置 |
| 自动范围全部被占 | 非零退出，提示 `d2x book --port <N>` |
| 生效的端口值非法 | 在安装和启动前报错，指出来源及合法范围 |
| 探测遇到权限、地址解析或其他系统错误 | 报出原因并停止，不按“占用”跳过 |

“未指定”用 `std::optional` 表达，不将 0 暴露为特殊值。外部有效值统一为 1–65535；JSON 必须是整数，拒绝字符串、浮点数、布尔值和 null。CLI / env 使用完整十进制解析，检查 `from_chars` 的错误和消费位置，拒绝 `3001abc`、负数、空串、溢出等。环境变量不存在才表示未配置。

校验生效来源的候选值；高优先级合法值可以覆盖低优先级的错误端口字段。低优先级未生效字段不阻止启动，其他子命令也不应因 book 专属字段报错。配置文件整体 JSON 损坏仍按现有配置错误路径处理，不能把“解析失败”描述成合法的字段缺失。

自动切换时只输出一次汇总，例如：

```text
Book ports 3000–3001 are in use; trying http://localhost:3002
```

这条信息表示已选定候选端口；真正的启动日志继续由 mdBook 输出。不要在启动前宣称服务器已经就绪。显式端口冲突时，提示冲突来源及换端口命令。

## 4. 端口检测的最小机制

平台层增加一个小接口，概念签名如下：

```cpp
// true = 可用，false = 占用；其他错误保留错误说明。
std::expected<bool, std::string> probe_tcp_port(int port);
```

接口只回答能否绑定或返回系统错误；自动顺延、次数上限和日志都属于 book 命令策略。

1. 对 `localhost` 使用 `getaddrinfo` 获得 TCP 地址。实现按候选端口解析，范围最多 100 次，避免为这一本地解析引入额外对象生命周期。
2. 对可用的 IPv4 / IPv6 地址执行 `socket → bind → close`，用 RAII 释放句柄。对该端口检查期间保留已绑定句柄，结束后一起释放。
3. 任一有效地址明确返回 `EADDRINUSE` / `WSAEADDRINUSE`，将该候选视为占用。两个地址族都可用时均检查，避免只检查 IPv4 却在 mdBook 使用 `::1` 时失败。
4. 不受系统支持的地址族可略过；至少一个有效地址完成检查才可判定可用。其余错误保留系统错误码及说明上报；Windows 的 `WSAEACCES` 也不能一概解释成占用。
5. 不开启 `SO_REUSEPORT`；探测 socket 不使用会放宽独占绑定的复用选项。Windows 使用 `SO_EXCLUSIVEADDRUSE`，包含 Winsock 初始化/清理，并确保链接 `ws2_32`。

保持 mdBook 的 `localhost` 默认监听，不顺带开放到 `0.0.0.0`。同时检查多个地址是一种保守策略：即使 mdBook 恰好能用另一个地址，也会跳过存在冲突的端口。这比依赖 C++ 与 Rust 的地址排序完全一致更稳定。

不依赖 `lsof`、`netstat`、`ss` 或 shell 工具，也不使用 TCP connect 探测：连接失败不能证明端口可绑定。

### 探测与启动之间的竞态

探测句柄释放后，端口可能被其他进程抢占；mdBook 本身还会先构建书籍，窗口无法完全消除。**此方案提供启动前检测，不承诺原子占用或并发启动必定成功。**

将探测安排在 mdBook 安装成功之后、执行 serve 之前，缩小不必要的间隔。若启动仍失败，保留 mdBook 原始输出，d2x 非零退出，提示重新运行或指定其他端口。

本次不根据非零退出码自动重启：书籍构建失败也会非零退出；事后发现端口被占，也不足以证明它是本次失败原因。解析 panic 文本做重试会增加版本耦合。若将来要求严格处理并发抢占，应另行设计与 mdBook 的监听交接或结构化就绪协议。

## 5. 默认值的取舍

Issue 希望“未配置时不传 `--port`，默认值完全由 mdBook 决定”。新增自动避让后，d2x 必须先知道检测起点。可选路径：

| 路径 | 代价 | 建议 |
| --- | --- | --- |
| d2x 明确以 3000 为默认起点，始终传最终 `--port` | 未来 mdBook 改默认值时需主动评估 | **采用：行为稳定，实现最少** |
| 每次解析 `mdbook serve --help` 获取默认值 | 依赖非结构化文本和版本格式 | 不采用 |
| 先无参数启动，失败后解析日志并重试 | 错误分类、输出捕获与浏览器副作用更复杂 | 不采用 |

因此，本方案主动调整 issue 的这一条建议。两个已核对版本的默认值都为 3000，当前正常场景端口不变。文档和 help 明确写“默认从 3000 自动选择”，避免承诺跟随未来 mdBook 默认值。也不增加 `--auto-port`、`--strict-port`、扫描范围等配置项。

## 6. 代码组织与必要衔接

### 命令入口

`src/cmdprocessor.cppm`：通过 `.option("port").short_name('p').takes_value()` 注册命名选项，不能使用 `.arg("port")`，后者是位置参数。将书籍启动流程提为本文件的 `run_book()` 小函数，返回归一化后的状态码，无需为单个命令再建模块。

```text
apply_global_options
→ 解析并校验生效端口来源
→ 按既有 lang 规则定位书籍目录
→ 检查目录并 require_xlings
→ 透传执行 xlings install mdbook -y，失败即停
→ 探测显式端口 / 选择自动端口
→ 透传执行 mdbook serve --open --port <N> <quoted-bookdir>
→ 将服务状态传回 main
```

action 捕获 `run()` 的局部 `book_exit_code`，执行 `run_book()` 并赋值；最后优先返回 `app.run()` 的解析错误，否则返回 `book_exit_code`。这样无需修改 cmdline 依赖，也无需在深层调用 `std::exit()`。局部变量必须在 builder 和 action 创建前声明。

继续用 `platform::exec()` 保留安装进度、mdBook 输出和现有前台运行方式；在 platform 提供一个小的 wait-status 归一化函数供 book 使用，避免改变其他调用方的返回值语义。POSIX 按正常退出/信号解码，Windows 保留其退出码；执行失败也需非零退出。尤其不能把 `system()` 的 256 直接返回 main，它可能截断为 0。

书籍路径必须作为一个 shell 参数传递，覆盖中文、空格和单引号。现有 `protocol::shell_quote()` 的 POSIX 算法可参考；Windows 的双引号包装不能阻止 `%VAR%` 等 shell 展开，不能宣称能安全处理任意路径。本次采用小型平台参数引用辅助函数；对 Windows 无法可靠表示的特殊路径明确拒绝并报错。无需为此重写通用进程管理。

### 配置

`src/config.cppm` 增加可选 `book_port` 和 `D2X_BOOK_PORT`，保留缺失状态。端口来源选择要能携带原始值/类型及来源信息，到 book 命令解析最终生效值时再校验，避免低优先级错误提前阻断覆盖。

本字段的全局回退必须不受现有 UI/LLM 条件影响：增加针对 book 字段的独立全局补缺读取，沿用现有配置路径和 JSON 读取能力，不借本 issue 重构所有配置项的优先级。CLI 值直接覆盖解析结果，无需再通过写环境变量转一圈。

同步 `load_config_with_error_handling`、`save_config`、`run_interactive_config` 和 `print_config`：读写并保留已有 book 字段，缺失时省略序列化；交互向导未新增端口问题时也要将原值带到 `new_cfg`。保留未知配置字段是现有配置系统的另一项改进，不在本次扩展。

### 平台与文档

`src/platform.cppm` 暴露探测、参数引用和状态归一化能力；`src/platform/{linux,macos,windows}.cppm` 承接系统差异。POSIX socket 代码尽量复用，系统头必须放在带平台守卫的全局模块片段，避免跨平台构建污染。

按构建系统支持的方式补 Windows socket 系统库链接；现有传递依赖可能已带入 Winsock，但应验证并显式声明本功能需要的链接依赖，具体 manifest 写法实施时核实。

更新中英文 README 的 book 示例、配置说明和帮助文本；新增针对本功能的测试，不涉及 checker/provider 协议、课程 shim 和版本发布。

## 7. 验收范围

| 场景 | 必须验证 |
| --- | --- |
| 未配置，3000 空闲 | 探测后向 mdBook 传入 3000 |
| 3000 被占；3000–3002 连续被占 | 分别选 3001、3003，只汇总提示一次 |
| 3000–3099 全占 | 有界失败，未启动 serve，非零退出 |
| `-p 8080` / `--port 8080` | 参数生效；占用时拒绝自动切换 |
| CLI / env / 本地 / 全局组合 | 优先级正确；特别覆盖本地 UI/LLM 已完整时的全局回退 |
| 错误低优先级值 + 正确高优先级值 | 高优先级生效；错误的最终值明确报来源 |
| 边界与非法输入 | 1、65535 接受语法校验；0、65536、负数、尾随字符、错误 JSON 类型拒绝 |
| IPv4、IPv6、通配地址占用及 IPv6 不可用 | 避让或按实际系统错误退出，不产生“检查 IPv4、启动 IPv6”的漏检 |
| 权限/解析错误、Windows 独占绑定 | 不当成普通端口冲突扫描；没有句柄泄漏 |
| 安装失败 / mdBook 失败 | 安装失败不继续；退出码不丢失；原始输出可见 |
| 路径与语言 | 中文、空格、POSIX 单引号路径正确；`book/<lang>` 与默认回退维持；Windows 不支持的特殊路径报错 |
| 保存已有配置 | `d2x config` 不丢失 `book_port` |
| 正常前台运行 | HTTP 可访问，自动打开 URL 的端口正确，修改书籍触发热更新；Ctrl+C 后实际监听进程退出 |

纯决策和错误分类用小测试覆盖；端口占用用持有真实监听 socket 的测试进程制造。命令拼装/退出码测试使用 fake xlings/mdbook，记录 argv 并返回指定退出码，避免真实安装和打开浏览器；测试须确认 fake 确实被现有解析链路调用。固定扫描范围的测试在隔离环境运行，不能擅自终止开发机已有服务。

Linux、macOS、Windows 均需构建和 socket 行为验证，尤其检查 Windows 链接与独占绑定。最后以真实 mdBook 0.4.43 和 0.5.2 各做一次默认占用后的手工冒烟；fake 命令测试不能代替真实监听、浏览器 URL、热更新和 Ctrl+C 检查。

## 8. Review 核心决策

1. **默认自动、显式严格**：默认占用自动换端口；CLI/env/配置给出的端口被占则报错。
2. **默认起点由 d2x 明确为 3000，最多探测至 3099**：接受与 issue“完全跟随 mdBook 默认值”建议的差异，换取简单、稳定的实现。
3. **检测能力有清晰边界**：原生 bind 检测，保留 localhost；承认探测后的竞争窗口，本次不引入日志解析重试。
4. **一次补齐必要衔接**：配置覆盖链与保存、安装失败处理、退出码、路径参数引用均随功能一起验收。

## 9. 实现记录

- 平台原语集中在 `src/platform/network.cppm` 分区，复用 POSIX 实现，并对 Windows 做原生 Winsock 适配。
- `Config::book_port()` 只读取最终需要的来源，独立于既有 UI/LLM 的条件合并；配置向导保留原始字段。
- cmdline 的 option builder 会返回所在子 App，因此 book App 单独构建再注册到根 App；测试覆盖根命令注册与全局语言参数。
- 为满足当前依赖索引最低版本，同步 `.xlings.json` 和 CI 到 mcpp 2026.10.19.1。
- 本地/跨平台 CI 的功能测试使用 `tests/book_test.py`；所有失败路径断言真实进程退出码，冲突使用真实 socket。

### 本地验收与自我 review（发布前）

- `mcpp build` 通过；`tests/book_test.py` 24 项中 22 项通过，IPv6 localhost 未映射和 Windows 专属项按环境跳过；`tests/e2e.sh` 全绿。
- 真实 mdBook 0.4.43 和 0.5.2 均验证：3000 被占后自动避让，HTTP 返回页面，浏览器打开的 URL 使用最终端口，WebSocket 收到 reload 且页面内容更新，SIGINT 后实际监听关闭。
- review 修正：独立构建 book App，避免 cmdline 的 option builder 使后续命令挂到错误节点；补充根 help 回归用例。
- review 确认：配置只校验获胜来源；全局字段不受 UI/LLM 条件影响；向导往返保留字段；安装失败不启动 serve；POSIX 退出码已归一化。
- CI 前置修复：历史 xlings 0.4.51 资源已 404，CI/release 改用有完整平台资产的 2026.10.19.2。
- release 工作流新增权威归档 SHA256 sidecar，为双镜像与索引校验提供同源依据。

- 跨平台验收修正：macOS 的 `/var` 与 `/private/var` 指向同一目录，测试先 canonicalize 临时根目录；Windows 原有程序在中文 cwd 的静态初始化阶段崩溃，通过目标的 `windows_code_page = "utf-8"` 清晰声明 UTF-8 应用清单，保持中文路径验收要求。
