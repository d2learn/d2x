# Task 02 - 新增 CI workflow（xlings quick_install）

## 目标
- 新增常规 CI，在 push / pull_request 触发 Linux、macOS 构建检查。

## 子任务
- [x] 新增 `.github/workflows/ci.yml`。
- [x] Linux job 使用 quick_install 安装 xlings，并验证 `xmake --version`。
- [x] macOS job 使用 quick_install 安装 xlings，并以 LLVM toolchain 构建。
- [x] 设置非交互安装（`XLINGS_NON_INTERACTIVE=1`）。

## 完成判定
- CI workflow 文件存在，结构正确，命令可执行。
