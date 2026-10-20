# d2x

> 练习驱动学习框架 —— 把任何课程变成「编辑 → 保存 → 自动检测 → 推进」的闯关体验

[English](README.md) | **简体中文**

[![C++23](https://img.shields.io/badge/C%2B%2B-23-orange.svg)](https://en.cppreference.com/w/cpp/23)
[![xlings](https://img.shields.io/badge/xlings-ok-green.svg)](https://github.com/openxlings/xlings)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE-CODE)

d2x 只拥有学习循环——会话进度、文件监听、自动推进,除此之外什么都不拥有。
课程经由一个小巧的 NDJSON **Provider 协议**从下方接入;前端消费上行的单向事件流。
它不认识 C++、不认识构建工具、不知道怎么编译任何东西。

## 快速开始

```bash
xlings install d2x -y        # 安装 d2x(依赖 xlings: https://xlings.d2learn.org)
d2x install d2mcpp           # 一键获取课程,环境自动配置
cd d2mcpp && d2x checker     # 开始学习——编辑、保存,自动重测并推进
```

## 命令

| 命令 | 作用 |
|---|---|
| `d2x checker [name]` | 交互式练习循环(子串匹配可跳转到指定练习) |
| `d2x status` | 只读进度总览,按章节聚合 |
| `d2x install <pkg>` | 从[课程索引](https://github.com/d2learn/xim-pkgindex-d2x)获取课程 |
| `d2x book [-p PORT]` | 本地预览课程电子书，默认端口占用时自动避让 |
| `d2x new <name>` | 从[模板](https://d2learn.github.io/d2x-project-template)创建新课程 |
| `d2x config` | 交互式配置 `.d2x.json`(语言、界面、编辑器、大模型) |
| `d2x list [query]` | 搜索可用课程 |

常用参数:`--lang zh|en`、`--ui tui|print`、`--emit-events`(NDJSON 事件流,供外部前端消费)。

## 电子书预览

`d2x book` 在 **3000–3099** 中选择第一个可用的 localhost 端口。
使用 `d2x book --port 8080`（或 `-p 8080`）指定端口；显式指定的端口
被占用时会报错，不会自动更换。

优先级：`--port` > `D2X_BOOK_PORT` > 项目 `.d2x.json` > `~/.d2x.json` > 自动选择。可在配置文件添加 `"book_port": 8080` 作为持久设置；合法值为
1–65535 的整数，删除字段可恢复自动选择。运行 `d2x config` 会保留此字段。

检测覆盖 IPv4 和 IPv6 的 localhost 地址。检测结束后到 mdBook 启动之间，
端口仍可能被其他进程抢占，此时重新运行或指定其他端口即可。
安装和启动失败会返回非零退出码，并保留 mdBook 的原始输出。

## 面向课程作者

课程 = 一个带 `.d2x.json` 的仓库,声明一条 **Provider** 命令,回答三个动词:
`describe` / `exercises` / `check <id>`。任何语言均可实现;参考实现与一致性测试
套件见 [`protocol/`](protocol/) 与 [`tests/`](tests/),协议规范见
[架构参考](.agents/docs/2026-07-20-d2x-architecture-reference.md)。

## 基于 d2x 的课程

| 课程 | 简介 |
|---|---|
| [d2mcpp](https://github.com/mcpp-community/d2mcpp) | 现代 C++ 核心语言特性,练习即测试 |
| [d2ds](https://github.com/d2learn/d2ds) | 强调动手实践的数据结构 |

## 链接

[论坛](https://forum.d2learn.org) · [课程索引](https://github.com/d2learn/xim-pkgindex-d2x) · [项目模板](https://d2learn.github.io/d2x-project-template) · [xlings](https://github.com/openxlings/xlings)

## Provider 首次启动与诊断

首次 `d2x checker` 可能需要构建课程 Provider 和准备工具链。
`provider_startup_timeout` 控制初次 `describe` 的总期限，默认 900 秒，必须为正整数。
优先级为 `D2X_PROVIDER_STARTUP_TIMEOUT` 环境变量、本地配置、全局配置、默认值。
输出不会延长期限。后续 `exercises` / `check` 仍使用 `provider_idle_timeout`
限制静默，默认 120 秒；该 idle 值 <= 0 时关闭静默限制。

print/TUI 会显示准备信息。`--emit-events` 在 session 前每十秒发送一次
`preparing`，stderr 以 output 事件展示；外部前端应允许这些事件先于 session。
初始化或传输失败产生带 `code`、`phase`、`message` 的 error 事件并非零退出；
练习本身未通过仍产生 verdict 并等待修改。

本轮 checker 的 Provider 原始输出分别保存在 `.d2x/provider.stdout.log` 和
`.d2x/provider.stderr.log`，stderr 同时实时呈现。分享前请检查日志内容。
超时或调用期间取消会终止子进程树；调用期间 Ctrl+C 返回 130。

详见 [启动与传输契约](docs/provider-startup.md)。
