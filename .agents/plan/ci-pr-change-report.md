# d2x CI 修改与实验分析报告（分支：add_ci_and_optimize_build）

## 本次修改摘要
- 修改文件：`.github/workflows/ci.yml`
- 关键提交：`ca01153`
- 目标：
  - 增加/完善跨平台 CI（Linux/macOS/Windows）
  - 在构建后验证 `d2x --version`
  - macOS 强制使用 Xlings 安装的 LLVM，而不是 Xcode 默认 clang

## PR 代码改动点
1. 新增并完善三个构建 job：
- `build-and-test-linux`
- `build-and-test-macos`
- `build-and-test-windows`

2. macOS 增强（核心）
- 安装 `llvm`：`xlings install llvm -y`
- 增加工具链验证：`which clang/clang++` 和 `clang --version`
- 构建前显式指定：
  - `xmake f -y -m release --toolchain=llvm --cc=clang --cxx=clang++`

## 实验信息（本地）
围绕 xmake 工具链生效逻辑做了对照实验（macOS）：

### 实验 A：仅依赖 `xmake.lua` 中 `set_toolchains("llvm")`
- 命令：`xmake f -c -y -m release -vD`
- 结果：编译器检测可能落到 Xcode clang（`/Applications/Xcode.app/.../clang++`）

### 实验 B：命令行加 `--toolchain=llvm`
- 命令：`xmake f -c -y -m release --toolchain=llvm -vD`
- 结果：能够检测到 Xlings LLVM（`~/.xlings/data/xpkgs/local-x-llvm/.../clang++`）

### 实验 C：再加 `--cc/--cxx`
- 命令：`xmake f -c -y -m release --toolchain=llvm --cc=clang --cxx=clang++ -vD`
- 结果：最稳定，可重复命中 Xlings LLVM

### 实验结论
- `set_toolchains("llvm")` 不是失效，而是对“具体 clang 路径”约束不够强。
- 在 macOS CI 下，最稳方案是命令行显式指定 `--toolchain=llvm --cc=clang --cxx=clang++`。
- 该方案只作用于 macOS job，不影响 Linux/Windows。

## CI 运行实验信息（远端）
针对提交 `ca011530c2784b031ba75f7f7485e5d7dda01ee9` 的最新 CI：
- Run: `22508449012`
- URL: `https://github.com/d2learn/d2x/actions/runs/22508449012`
- 总结论：`failure`

Job 结果：
1. `build-and-test-windows`: `success`
2. `build-and-test-linux`: `failure`（失败步骤：`Build with xmake`）
3. `build-and-test-macos`: `failure`（失败步骤：`Test d2x`）

## 原因分析
### 1) macOS 失败（`Test d2x`）
高概率原因：
- 测试步骤里执行了 `d2x install d2mcpp`（无 `-y`），命令可能触发交互或外部依赖下载失败，导致 CI 无法继续。

建议：
- 把 macOS（以及 Linux）测试中的 `install d2mcpp` 改为非交互参数（例如 `-y`），并增加失败时的诊断输出。

### 2) Linux 失败（`Build with xmake`）
已知信息：
- 失败发生在构建步骤，返回 `exit code 255`（从 check annotation 可见）。

目前无法直接拿到完整日志文本的原因：
- 当前会话下 `gh` 未登录，且匿名 API 无权限下载 job logs（403）。

基于现有 workflow 的可疑点：
- Linux 构建步骤仅执行 `xmake -vv -y -j...`，未显式 `xmake f -m release -y`，在某些环境下可能导致探测/配置不一致。
- 与 `release.yml` 对比，release 流程在 Linux 会先显式配置再编译，稳定性更高。

建议：
- Linux build step 调整为：
  1. `xmake f -m release -y`
  2. `xmake -vv -y -j"$(nproc)"`
- 同时在失败时输出 `xmake f -vD` 关键配置，便于定位。

## 最终方案建议
1. 保持当前 macOS 的显式 LLVM 指定（已完成）。
2. 调整 Linux 构建为“先 `xmake f` 再 `xmake`”。
3. `d2x install d2mcpp` 统一改成非交互参数。
4. 补充失败诊断日志（编译器路径、xmake configure 摘要）。

## 本轮已执行修复（已落地到代码）
已根据以上分析直接修改 `.github/workflows/ci.yml`：

1. Linux 构建阶段改为显式先配置再编译
- 从：
  - `xmake -vv -y -j\"$(nproc)\"`
- 调整为：
  - `xmake f -m release -y`
  - `xmake -vv -y -j\"$(nproc)\"`

2. Linux 测试安装命令改为非交互
- 从：`build/linux/x86_64/release/d2x install d2mcpp`
- 调整为：`build/linux/x86_64/release/d2x install d2mcpp -y`

3. macOS 测试安装命令改为非交互
- 从：`build/macosx/arm64/release/d2x install d2mcpp`
- 调整为：`build/macosx/arm64/release/d2x install d2mcpp -y`

说明：
- 本次修复保持“仅在 CI 层处理”的策略，不改变 `xmake.lua` 的跨平台全局行为。
