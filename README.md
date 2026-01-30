# d2x - [xlings](https://github.com/d2learn/xlings)

> 一个交互式教程项目搭建工具 - `Book + Video + Code + X`

[![xlings](https://img.shields.io/badge/C++-23-orange.svg)](https://github.com/d2learn/xlings)
[![xlings](https://img.shields.io/badge/xlings-ok-green.svg)](https://github.com/d2learn/xlings)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE-CODE)

| [xlings工具](https://github.com/d2learn/xlings) - [论坛](https://forum.d2learn.org) - [项目模板及说明](https://d2learn.github.io/d2x-project-template) - [d2x类项目索引仓库](https://github.com/d2learn/xim-pkgindex-d2x) |
| --- |

```cpp
d2x version: 0.1.1

Usage: $ d2x [command] [target] [options]

Commands:
         new            create new d2x project from template
         install        install d2x package via xlings
         book           open project's book
         checker        run checker for d2x project's exercises
         config         configure d2x (.d2x.json)
         list           list available d2x packages
```

## 功能特色

- 使用C++23模块化实现
- 支持一键创建交互式教程项目(模板)
- 支持书籍目录和本地预览
- 支持一键获取d2x类项目和包管理
- 支持交互式的实时代码练习及自动检测验证
- 支持AI智能学习引导, 并可以自定义后端大模型

## 快速开始

<details>
  <summary>点击查看xlings安装命令</summary>

---

#### Linux/MacOS

```bash
curl -fsSL https://d2learn.org/xlings-install.sh | bash
```

#### Windows - PowerShell

```bash
irm https://d2learn.org/xlings-install.ps1.txt | iex
```

> tips: xlings -> [details](https://xlings.d2learn.org)

---

</details>

**安装**

> 通过xlings包管理器安装d2x工具

```
xlings install d2x
```

**创建交互式教程**

> new命令可以快速创建一个基础的交互教程项目

```
d2x new hello
```

- 注: [交互式教程项目及说明文档](https://d2learn.github.io/d2x-project-template)

**一键获取教程**

> d2x可以通过install命令一键安装被收录在 [`d2x类项目索引仓库`](https://github.com/d2learn/xim-pkgindex-d2x) 中的教程项目, 并能自动配置好本地环境

```
d2x install d2mcpp
```

**教程书籍**

> 在一个d2x类教程项目中, 可以通过book命令本地预览教程的电子书

```
d2x book
```

**交互式代码练习**

> 在一个d2x类教程项目中, 可以通过checker进入代码练习模式

```
d2x checker
```

## 项目案例

| 项目 | 简介 | 备注 |
| --- | --- | --- |
| [d2mcpp](https://github.com/mcpp-community/d2mcpp) | 现代C++核心特性入门教程 | |
| [d2ds](https://github.com/d2learn/d2ds) | 强调动手实践的数据结构学习项目 | |

## 其他

- [d2x类项目索引仓库](https://github.com/d2learn/xim-pkgindex-d2x)
- [d2x项目模板](https://github.com/d2learn/d2x-project-template)
- [论坛交流和反馈](https://forum.d2learn.org)
- `交流群`: 167535744