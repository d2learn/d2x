# xlings 安装、CI 与 macOS release 打包改造计划

## 目标

1. 在仓库内落地一份清晰可执行的方案文档。
2. 新增常规 CI（push / pull_request）。
3. 更新 release workflow：
   - Linux / macOS 使用 xlings quick_install。
   - macOS 打包时携带 LLVM libc++ 运行库到包内 `libs/`。
   - 修改 `d2x` 的 rpath 指向包内相对路径 `@loader_path/libs`。
   - 若相对 rpath 不可用，输出明确提示并终止发布。

## 实施范围

- `.github/workflows/ci.yml`（新增）
- `.github/workflows/release.yml`（更新）
- `.agents/tasks/*.md`（任务拆解）

## 执行策略

1. **任务拆解并落盘**
   - 创建 tasks 文档，便于追踪执行状态。

2. **新增 CI workflow**
   - Linux + macOS 编译验证。
   - 使用 quick_install 安装 xlings（含 xmake）。
   - 使用非交互安装模式以适配 GitHub Actions。

3. **更新 release workflow**
   - 统一 Linux/macOS 的 xlings 安装方式为 quick_install。
   - macOS 打包阶段加入：
     - 复制依赖的 LLVM libc++ dylib 至 `libs/`
     - 添加相对 rpath：`@loader_path/libs`
     - 删除旧绝对 rpath（若存在）
     - 显式验证（`otool -l` / `otool -L` / `libs` 非空）

4. **验证**
   - 本地执行 YAML 语法与结构检查。
   - 检查 workflow 关键路径（artifact 名称与上传路径）一致性。

## 验收标准

- CI 在 push/PR 触发并成功构建 Linux/macOS。
- Release 工作流使用 quick_install 安装 xlings。
- macOS 产物中包含 `libs/` 且 `d2x` 具有 `@loader_path/libs` rpath。
- 相对 rpath 设置失败时，日志明确提示并 fail-fast。
