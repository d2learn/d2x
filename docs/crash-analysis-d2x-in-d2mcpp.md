# d2x 在 d2mcpp 场景启动崩溃分析报告

## 背景

- 目标二进制：`/Users/speak/workspace/github/d2learn/d2x/build/macosx/arm64/release/d2x`
- 复现目录：`/Users/speak/test/d2mcpp`
- 现象：直接运行 `d2x` 即崩溃，退出码 `134`（`SIGABRT`）
- 初始怀疑：与 `.d2x.json` 加载有关

## 分析路径（思路与拆解）

本次定位采用“从启动最早路径向下分解”的方法，优先确定崩溃发生在：

1. 入口与参数解析前后（`main` / `cmdprocessor`）
2. 配置文件路径解析与读取（`.d2x.json`）
3. 早期静态初始化（`checker` / `buildtools`）
4. 平台层命令执行封装（`popen`/`pclose`）

为避免误判，采取“插桩 + 对照复现 + 调试器回溯”三段式：

- 插桩：给启动关键节点加日志，不改业务行为
- 对照复现：正常配置 vs 损坏配置
- 调试器：`lldb` 抓崩溃堆栈确认最后阶段

## 代码插桩具体项

### 1) 配置链路（`src/config.cppm`）

- `Config` 初始化入口日志
- local/global 配置路径解析结果日志
- `exists()` 检查前后日志
- `load_from_file()` / `merge_from_file()` 阶段日志
- 异常分类日志：
  - `nlohmann::json::parse_error`
  - `std::filesystem::filesystem_error`
  - `std::exception`
  - `...`（未知异常）
- 环境变量回填来源日志（敏感值只记录“来源”，不打印明文）

### 2) 平台链路（`src/platform.cppm` / `src/platform/macos.cppm`）

- `get_rundir()`、`get_home_dir()` 返回值日志
- `run_command_capture()` 执行与退出码日志
- `popen` 失败日志
- `pclose` 非零状态日志

### 3) 早期初始化与边界保护（`src/buildtools.cppm` / `src/checker.cppm`）

- `load_buildtools()` 开始/结束日志，记录 buildtools 命令来源
- `bt.init()` 失败时输出摘要日志（截断，防噪声）
- `checker` 中 `files.empty()` 防护，避免 `files[0]` 越界
- 读取目标文件时增加异常保护与错误日志

## 复现与对照验证

### 场景 A：有效 `.d2x.json`

- 在 `/Users/speak/test/d2mcpp` 执行二进制
- 结果：`exit code = 134`

### 场景 B：损坏 `.d2x.json`（故意破坏 JSON）

- 执行同一二进制
- 结果：仍为 `exit code = 134`

结论：两种配置状态结果一致，说明“仅由 `.d2x.json` 语法错误触发崩溃”的概率较低。

## 调试器证据

通过 `lldb` 运行与回溯，捕获到：

- 信号：`SIGABRT`
- 关键报错：`malloc: pointer being freed was not allocated`
- 栈中出现模块初始化相关符号，包含 `_ZGIW3d2xW12cmdprocessor`

这表明崩溃点更靠近“模块/静态初始化阶段”，并非典型业务逻辑运行期崩溃。

## 初步归因

当前更可疑方向：

1. 启动早期对象构造/析构生命周期问题（内存释放异常）
2. 静态初始化链路中的跨模块交互问题
3. `checker` 的全局静态 `btools` 触发过早初始化（高可疑）

`.d2x.json` 仍是需要保留观察的影响因子，但从对照结果看，不是当前最主要触发源。

## 定位性修复验证（已完成）

针对高可疑点，已实施如下验证性改动：

- 将 `src/checker.cppm` 中的全局静态
  - `static auto btools = d2x::load_buildtools();`
- 改为延迟初始化：
  - `get_buildtools()` 内函数静态对象，在 `checker::run()` 首次执行时再初始化

验证结果：

1. 重新构建 `release` 成功
2. 在 `/Users/speak/test/d2mcpp` 下连续运行 3 次，全部 `exit code = 0`
3. 补充压力验证：连续运行 10 次，全部 `exit code = 0`
4. 在仓库根目录与空目录运行，也不再复现启动崩溃

## 结论（当前阶段）

`d2x` 启动 crash 的主要根因已收敛为：

- **`checker` 模块的全局静态 `btools` 过早初始化，触发了启动期静态初始化链路问题（生命周期/初始化顺序风险）**

`.d2x.json` 不是本次启动崩溃的主因。

## 后续建议

1. 保持 `buildtools` 的延迟初始化实现，避免回退到全局静态初始化
2. 在 CI 增加“空命令启动 + 多次重复启动”稳定性检查
3. 后续若需进一步提升健壮性，可为关键静态初始化点补充单元/集成测试

