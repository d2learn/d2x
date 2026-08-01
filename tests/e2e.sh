#!/usr/bin/env bash
# d2x 自有端到端测试:以假 Provider 驱动,不依赖 mcpp/d2mcpp。
#
# 覆盖(2026-07-24 设计文档 D7):
#   1 协议容错   垃圾行忽略、缺 verdict=fail、describe 失败=明确报错
#   2 闯关推进   fail → 改文件 → pass → 推进,state.json 持久化
#   3 活性超时   挂死 Provider 被终止并明示
#   4 单实例锁   第二实例被拒
#   5 stdout 契约 print 页面在重定向下仍可见(防 13863df 回归)
#
# 用法: D2X=/path/to/d2x bash tests/e2e.sh
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
D2X="${D2X:?D2X=/path/to/d2x required}"
FAKE="$HERE/fake_provider.sh"

# Windows(Git Bash / MSYS)下 d2x 是原生 exe:它经 cmd.exe 启动 Provider,
# 也用原生 API 打开练习文件,认不得 /tmp/... 这类 MSYS 路径。所以凡是要
# 交给 d2x 的路径都先转成 C:/... 形式 —— bash 同样接受这种写法,两边通用。
IS_WINDOWS=0
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) IS_WINDOWS=1 ;; esac

native() {  # $1 = path -> d2x 能直接使用的路径
    if [[ $IS_WINDOWS -eq 1 ]]; then cygpath -m "$1"; else printf '%s' "$1"; fi
}

FAKE_N="$(native "$FAKE")"

rc=0
fail() { echo "E2E FAIL: $*"; rc=1; }

# 每个场景独立的临时"课程仓库"
setup() {   # $1 = mode
    local dir
    dir=$(mktemp -d)
    printf 'unsolved\n' > "$dir/ex1.txt"
    printf 'unsolved\n' > "$dir/ex2.txt"
    cat > "$dir/.d2x.json" <<EOF
{
  "buildtools": "bash $FAKE_N $1 $(native "$dir")",
  "lang": "en",
  "ui_backend": "print"
}
EOF
    echo "$dir"
}

run_events() {   # $1=dir $2=timeout-secs [extra env...]
    local dir="$1" t="$2"; shift 2
    ( cd "$dir" && env "$@" timeout "$t" "$D2X" checker --emit-events 2>/dev/null )
}

# ── 1a 垃圾行忽略:混入噪声仍能给出正确 verdict ──────────────────────
dir=$(setup garbage)
out=$(run_events "$dir" 15)
echo "$out" | grep -q '"event":"verdict"' && echo "$out" | grep -q '"outcome":"fail"' \
    || fail "garbage: 未在噪声中给出 fail verdict: $out"
echo "$out" | grep -q '"event":"session"' || fail "garbage: 缺 session 事件"
rm -rf "$dir"

# ── 1b 缺 verdict = fail(绝不能当通过)────────────────────────────────
dir=$(setup no-verdict)
out=$(run_events "$dir" 15)
echo "$out" | grep -q '"outcome":"fail"' || fail "no-verdict: 未判 fail: $out"
echo "$out" | grep -qi "did not report a verdict" || fail "no-verdict: 缺原因说明"
rm -rf "$dir"

# ── 1c describe 失败 = 明确报「Provider 挂了」,而非「没有练习」──────────
dir=$(setup describe-fail)
err=$( cd "$dir" && timeout 15 "$D2X" checker --emit-events 2>&1 >/dev/null )
echo "$err" | grep -q "Provider 无响应" || fail "describe-fail: 缺明确报错: $err"
rm -rf "$dir"

# ── 2 闯关推进:fail → 写入 SOLVED → pass → 推进到 ex-2,状态落盘 ──────
dir=$(setup ok)
( cd "$dir" && timeout 30 "$D2X" checker --emit-events > events.ndjson 2>/dev/null ) &
CHK=$!
sleep 3
printf 'SOLVED\n' > "$dir/ex1.txt"
sleep 6
kill $CHK 2>/dev/null; wait $CHK 2>/dev/null
grep -q '"outcome":"pass"' "$dir/events.ndjson" || fail "advance: 未见 pass verdict"
grep -q '"current":"ex-2"' "$dir/events.ndjson" || fail "advance: 未推进到 ex-2"
grep -q '"ex-1"' "$dir/.d2x/state.json" 2>/dev/null || fail "advance: state.json 未记录 ex-1 完成"
rm -rf "$dir"

# ── 3 活性超时:挂死 Provider 在 idle 秒后被终止并明示 ────────────────
# 注意:checker 判 fail 后会驻留等待文件变更(设计行为),所以不量总时长;
# 判据是「20s 窗口内出现 verdict」——若 idle 超时未生效,hang(300s)不可能
# 在窗口内给出任何 verdict。
#
# Windows 跳过:protocol/src/process.cppm 的 run_lines_idle 在 _WIN32 下
# 显式回退为「无超时运行」(缺按句柄终止进程树的安全路径),这条场景在该
# 平台上没有被测行为可言。跳过是如实反映实现,不是掩盖失败。
if [[ $IS_WINDOWS -eq 1 ]]; then
    echo "E2E SKIP: idle-timeout(Windows 无 run_lines_idle 实现,见 process.cppm)"
else
    dir=$(setup hang)
    out=$(run_events "$dir" 20 D2X_PROVIDER_IDLE_TIMEOUT=2)
    echo "$out" | grep -q '"outcome":"fail"' || fail "idle-timeout: 20s 内未出现 fail verdict(超时未生效)"
    echo "$out" | grep -qi "terminated\|no output" || fail "idle-timeout: 缺终止说明: $out"
    rm -rf "$dir"
fi

# ── 4 单实例锁 ───────────────────────────────────────────────────────
dir=$(setup ok)
( cd "$dir" && timeout 20 "$D2X" checker --emit-events > /dev/null 2>&1 ) &
CHK=$!
sleep 3
second=$( cd "$dir" && timeout 8 "$D2X" checker --emit-events 2>&1 >/dev/null )
echo "$second" | grep -q "正在此仓库运行" || fail "lock: 第二实例未被拒: $second"
kill $CHK 2>/dev/null; wait $CHK 2>/dev/null
rm -rf "$dir"

# ── 5 stdout 契约 + 展示路径形态 ─────────────────────────────────────
dir=$(setup ok)
( cd "$dir" && timeout 12 "$D2X" checker --ui print > page.out 2>/dev/null )
grep -q "ex-1" "$dir/page.out" || fail "flush: 重定向下页面不可见(13863df 回归)"

# Provider 报的是绝对路径,而 checker 的 cwd 就是该目录,所以展示路径必须被
# 压成相对形式,任何情况下都不该以分隔符打头。旧 normalize_path 只掐 '/',
# Windows 上留下 "\ex1.txt"(Win10/Win11 CI 实测)——这条断言钉死该回归。
file_val=$(sed -n 's/^File: *//p' "$dir/page.out" | head -1)
[[ "$file_val" == *ex1.txt ]] \
    || fail "path: 练习页未展示 ex1.txt: '$file_val'"
[[ "$file_val" != /* && "$file_val" != \\* ]] \
    || fail "path: 展示路径带前导分隔符(normalize_path 回归): '$file_val'"
rm -rf "$dir"

if [[ $rc -eq 0 ]]; then echo "E2E: ALL GREEN"; else echo "E2E: FAILED"; fi
exit $rc
