# d2x

> Exercise-driven learning framework — turn any course into a game of `edit → save → auto-check → advance`

**English** | [简体中文](README.zh-CN.md)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-orange.svg)](https://en.cppreference.com/w/cpp/23)
[![xlings](https://img.shields.io/badge/xlings-ok-green.svg)](https://github.com/openxlings/xlings)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue.svg)](LICENSE-CODE)

d2x owns the learning loop — session progress, file watching, auto-advance — and nothing else.
Courses plug in underneath through a small NDJSON **Provider protocol**; frontends consume a
one-way event stream on top. It doesn't know C++, build tools, or how to compile anything.

## Quick start

```bash
xlings install d2x -y        # install d2x (needs xlings: https://xlings.d2learn.org)
d2x install d2mcpp           # grab a course, environment auto-configured
cd d2mcpp && d2x checker     # start learning — edit, save, it re-checks and advances
```

## Commands

| Command | What it does |
|---|---|
| `d2x checker [name]` | interactive practice loop (substring match to jump to an exercise) |
| `d2x status` | read-only progress overview, grouped by chapter |
| `d2x install <pkg>` | fetch a course from the [d2x index](https://github.com/d2learn/xim-pkgindex-d2x) |
| `d2x book` | preview the course's book locally |
| `d2x new <name>` | scaffold a new course from the [template](https://d2learn.github.io/d2x-project-template) |
| `d2x config` | interactive `.d2x.json` configuration (language, UI, editor, LLM) |
| `d2x list [query]` | search available courses |

Useful flags: `--lang zh|en`, `--ui tui|print`, `--emit-events` (NDJSON stream for external frontends).

## For course authors

A course is a repository with a `.d2x.json` declaring one command — the **Provider** — that
answers three verbs: `describe`, `exercises`, `check <id>`. Any language works; the reference
implementation and conformance suite live in [`protocol/`](protocol/) and [`tests/`](tests/).
See the [architecture reference](.agents/docs/2026-07-20-d2x-architecture-reference.md).

## Courses built with d2x

| Course | About |
|---|---|
| [d2mcpp](https://github.com/mcpp-community/d2mcpp) | Modern C++ core language features, exercises-as-tests |
| [d2ds](https://github.com/d2learn/d2ds) | Hands-on data structures |

## Links

[Forum](https://forum.d2learn.org) · [Course index](https://github.com/d2learn/xim-pkgindex-d2x) · [Project template](https://d2learn.github.io/d2x-project-template) · [xlings](https://github.com/openxlings/xlings)
