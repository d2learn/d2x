#!/usr/bin/env bash
# 协议一致性测试用假 Provider。
#
# 用法(作为 .d2x.json 的 buildtools):
#   bash tests/fake_provider.sh <mode> <dir>      # d2x 会追加 describe/exercises/check <id>
#
# mode:
#   ok            正常课程:两道练习;check 语义:练习文件含 SOLVED → pass,
#                 含 WAIT → blocked,否则 fail(带一条结构化诊断)
#   describe-fail describe 直接失败(退出 1、无输出)
#   no-verdict    check 只发 stage/output 就正常退出——缺 verdict 必须判 fail
#   garbage       事件流里混入非 JSON 噪声行——必须被忽略,验证仍正常工作
#   hang          check 陷入无输出的沉睡——活性超时必须终止它
#
# 练习文件目录(e2e 准备的临时目录)经第二个位置参数传入,而不是
# `FAKE_DIR=... bash ...` 的环境变量前缀 —— Windows 上 d2x 经 _popen 走
# cmd.exe 启动 Provider,cmd 没有这种前缀语法,整条命令会直接失败。
set -u

MODE="${1:?mode required}"; shift
DIR="${1:?dir required}"; shift
VERB="${1:?verb required}"; shift || true

emit() { printf '%s\n' "$1"; }

case "$VERB" in
describe)
    [[ "$MODE" == describe-fail ]] && exit 1
    emit '{"event":"describe","protocol":1,"name":"fake"}'
    ;;
exercises)
    emit "{\"event\":\"exercise\",\"id\":\"ex-1\",\"order\":1,\"title\":\"one\",\"chapter\":\"ch\",\"files\":[\"$DIR/ex1.txt\"]}"
    emit "{\"event\":\"exercise\",\"id\":\"ex-2\",\"order\":2,\"title\":\"two\",\"chapter\":\"ch\",\"files\":[\"$DIR/ex2.txt\"]}"
    ;;
check)
    ID="${1:?id required}"
    case "$MODE" in
    hang)
        sleep 300
        ;;
    no-verdict)
        emit '{"event":"stage","name":"compile"}'
        emit '{"event":"output","chunk":"compiling...\n"}'
        exit 0
        ;;
    garbage|ok)
        [[ "$MODE" == garbage ]] && { echo "launcher noise: not json"; echo ""; echo "{broken json"; }
        emit '{"event":"stage","name":"check"}'
        FILE="$DIR/${ID/ex-/ex}.txt"
        CONTENT="$(cat "$FILE" 2>/dev/null || true)"
        if [[ "$CONTENT" == *SOLVED* ]]; then
            emit '{"event":"verdict","outcome":"pass","stage":"check","exit_code":0,"diagnostics":[]}'
        elif [[ "$CONTENT" == *WAIT* ]]; then
            emit '{"event":"verdict","outcome":"blocked","stage":"check","exit_code":1,"diagnostics":[]}'
        else
            emit "{\"event\":\"output\",\"chunk\":\"not solved yet\\n\"}"
            emit "{\"event\":\"verdict\",\"outcome\":\"fail\",\"stage\":\"check\",\"exit_code\":1,\"diagnostics\":[{\"file\":\"$FILE\",\"line\":1,\"col\":0,\"severity\":\"error\",\"message\":\"write SOLVED into the file\"}]}"
        fi
        ;;
    esac
    ;;
*)
    exit 2
    ;;
esac
