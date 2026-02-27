# Task 03 - 更新 release workflow（macOS libs + 相对 rpath）

## 目标
- release 流水线中 macOS 包内携带 LLVM libc++ 依赖，并把 d2x 的 rpath 指向 `@loader_path/libs`。

## 子任务
- [x] Linux/macOS 安装步骤改为 quick_install。
- [x] macOS 打包目录新增 `libs/` 并复制所需 dylib。
- [x] 使用 `install_name_tool` 写入 `@loader_path/libs`。
- [x] 清理旧绝对 rpath（存在时）。
- [x] 添加验证：rpath 存在、libs 非空、`otool -L` 能看到 `@rpath` libc++。
- [x] 若相对 rpath 设置失败，输出明确提示并退出。

## 完成判定
- release workflow 中上述逻辑完整且路径一致。
