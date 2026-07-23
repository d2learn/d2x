# d2x 稳定性批次 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落实 `2026-07-24-d2x-stability-usability-design.md` 全部方案（P0–P7），版本切换为日期制 **2026.07.24.1**，单 PR → CI 全绿 → bypass squash 合入 → 发布 + xlings 生态验证（含本地 d2mcpp 联动）。

**Architecture:** 先 P0 把协议能力析出为 rooted-workspace 成员 `protocol/`（模块 `d2x.protocol.*`：types/codec/transport），d2x core 经 path 依赖消费；活性超时与 conformance 套件随后落位其中。其余任务按设计文档 D1–D8 逐项实现，每项带 fake-provider e2e 或单测。

## Global Constraints

- 双向协议字段零改动；d2mcpp 联动 e2e 全程作为回归闸门。
- 版本双源同步:`mcpp.toml` + `src/config.cppm Info::VERSION` = `2026.07.24.1`;release.yml 的版本比对逻辑兼容日期制。
- 每任务一提交;PR 单个;合入用 `gh pr merge --squash --admin`(目标已授权)。
- 生态验证:本地 d2mcpp 全链路(checker 推进/e2e)+ 发布产物 + xlings 安装链路(镜像/索引按记忆中 d2x 发布流程)。

## Tasks

- [x] **T0 版本日期制**:mcpp.toml + Info::VERSION → 2026.07.24.1;release.yml 版本核对与 tag 命名兼容(v2026.07.24.1)。
- [x] **T1 (P0) 协议层析出**:`protocol/` 成员(types.cppm=domain 类型迁移+`export import` 兼容壳,codec.cppm=parse_event/escape/downlink serialize,transport.cppm=ProcessProvider+shell_quote);root mcpp.toml 变 rooted workspace + path 依赖;行为等价,session 单测+d2mcpp 冒烟不变绿。
- [x] **T2 (P1a) 活性超时**:platform `run_command_lines_idle`(POSIX poll+WNOHANG+idle 判定,Windows 回退无超时);transport 接入,默认 120s,`.d2x.json provider_idle_timeout`/env 可配;杀死后 log 明示。
- [x] **T3 (P1b) 单实例锁**:`.d2x/checker.lock`(pid);启动探活拒绝/陈旧接管;SIGINT/SIGTERM 清锁。
- [x] **T4 (P2) xlings 简化+install 健壮化**:require_xlings(缺失→分平台命令报错退出,删代装链路);包名白名单;`-y`;失败指引;成功校验 .d2x.json+引导;new/book/list 对齐;has_xlings regex_search。
- [x] **T5 (P3) 状态原子化**:StateStore 保存 tmp+rename;损坏→改名备份+stderr 告警+空态继续。
- [x] **T6 (P4) conformance 套件**:tests/fake_provider.sh(场景:ok/describe-fail/no-verdict/garbage/hang/blocked)+tests/e2e.sh(闯关推进/容错/超时/锁/flush 五组)+protocol 单测(tests/protocol_test.cpp)。
- [x] **T7 (P5) stdout 契约**:book 的 mdbook 安装改透传;契约注释成文;flush 断言在 T6。
- [x] **T8 (P6) 呈现重构**:UIState 改名(exercise/files/completed/total);print+tui 新分区布局;输出截断(头20尾30)+`.d2x/last-output.log`;消息目录 zh/en 随 lang。
- [x] **T9 (P7) `d2x status`**:只读总览(按章节聚合),--emit-events 出 JSON。
- [x] **T10 文档**:架构参考 §9 缺口勾销更新+新增条目;README 命令表;设计文档路线图勾选。
- [ ] **T11 PR+CI+合入**:push → PR(标题带 2026.07.24.1) → CI 绿 → squash 合入。
- [ ] **T12 发布+生态验证**:release.yml(version=2026.07.24.1) → 产物→ xlings-res/d2x 双源镜像+sha256 → xim-pkgindex d2x.lua bump PR → 合并;`xlings install d2x@2026.07.24.1`;本地 d2mcpp 联动(checker 闯关+e2e all)复验;结果回记。
