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
| `d2x book` | 本地预览课程电子书 |
| `d2x new <name>` | 从[模板](https://d2learn.github.io/d2x-project-template)创建新课程 |
| `d2x config` | 交互式配置 `.d2x.json`(语言、界面、编辑器、大模型) |
| `d2x list [query]` | 搜索可用课程 |

常用参数:`--lang zh|en`、`--ui tui|print`、`--emit-events`(NDJSON 事件流,供外部前端消费)。

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
