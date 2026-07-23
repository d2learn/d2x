// session 层单测：不碰文件系统、不碰构建工具、不碰终端。
//
// 这是把 checker::run() 从 100 行缠绕逻辑里拆出来的全部意义所在 ——
// 学习流程的推进规则现在可以脱离一切外部依赖来验证。

#include <cstdio>   // stderr —— import std 不提供 C 运行时的宏

import std;
import d2x.domain;
import d2x.session;

using d2x::domain::Exercise;
using d2x::session::Session;
using d2x::session::StateStore;

namespace {

int g_failed = 0;
int g_total  = 0;

void check(bool ok, std::string_view what, std::source_location loc = std::source_location::current()) {
    ++g_total;
    if (ok) return;
    ++g_failed;
    std::println(stderr, "FAIL [{}:{}] {}", loc.file_name(), loc.line(), what);
}

// 每个用例一个临时目录，互不干扰
std::filesystem::path temp_state(std::string_view name) {
    auto dir = std::filesystem::temp_directory_path() / std::format("d2x-session-test-{}", name);
    std::filesystem::remove_all(dir);
    return dir / "state.json";
}

std::vector<Exercise> make_exercises(int n) {
    std::vector<Exercise> out;
    for (int i = 0; i < n; ++i) {
        out.push_back(Exercise{
            .id      = std::format("ex-{}", i),
            .order   = i,
            .title   = std::format("Exercise {}", i),
            .chapter = "test",
            .files   = {std::format("/tmp/ex-{}.cpp", i)},
        });
    }
    return out;
}

// ── 用例 ────────────────────────────────────────────────────────────

void empty_session_is_done() {
    auto path = temp_state("empty");
    StateStore state(path);
    Session s({}, &state);
    check(s.empty(), "空会话 empty()");
    check(s.total() == 0, "空会话 total()==0");
    s.seek_start("");
    check(s.done(), "空会话立即 done()");
}

void advances_in_order() {
    auto path = temp_state("advance");
    StateStore state(path);
    Session s(make_exercises(3), &state);
    s.seek_start("");

    check(s.current().id == "ex-0", "从第一题开始");
    s.complete_current();
    check(s.current().id == "ex-1", "推进到第二题");
    s.complete_current();
    check(s.current().id == "ex-2", "推进到第三题");
    s.complete_current();
    check(s.done(), "全部完成后 done()");
}

void completed_count_tracks_progress() {
    auto path = temp_state("count");
    StateStore state(path);
    Session s(make_exercises(4), &state);
    s.seek_start("");

    check(s.completed_count() == 0, "起始完成数为 0");
    s.complete_current();
    check(s.completed_count() == 1, "完成一题后计数为 1");
    s.complete_current();
    check(s.completed_count() == 2, "完成两题后计数为 2");
}

// 断点续做：这是 rustlings 的教训 —— 按 id 存而非下标，
// 重排或重命名练习不会毁掉学员进度。
void resumes_from_persisted_state() {
    auto path = temp_state("resume");
    {
        StateStore state(path);
        Session s(make_exercises(5), &state);
        s.seek_start("");
        s.complete_current();   // ex-0
        s.complete_current();   // ex-1
    }
    {
        StateStore fresh(path);                 // 从磁盘重新加载
        Session s(make_exercises(5), &fresh);
        s.seek_start("");
        check(s.current().id == "ex-2", "重启后从 ex-2 继续");
        check(s.completed_count() == 2, "重启后完成数仍为 2");
    }
}

// 顺序变了，进度不该丢 —— 用下标存就会在这里错位
void reordering_preserves_progress() {
    auto path = temp_state("reorder");
    {
        StateStore state(path);
        Session s(make_exercises(4), &state);
        s.seek_start("");
        s.complete_current();   // ex-0 完成
    }
    {
        // 课程作者在开头插入一道新练习，原有 id 全部后移
        auto shifted = make_exercises(4);
        shifted.insert(shifted.begin(), Exercise{
            .id = "ex-new", .order = -1, .title = "新增", .chapter = "test",
            .files = {"/tmp/ex-new.cpp"},
        });

        StateStore fresh(path);
        Session s(shifted, &fresh);
        s.seek_start("");
        check(s.completed_count() == 1, "重排后 ex-0 仍算已完成");

        // 持久化的 current 优先 —— 学员回到离开时的位置，不被硬拉回开头
        check(s.current().id == "ex-1", "从离开时的位置继续，而不是新插入的那道");

        // 但新插入的那道不能被永久跳过：做完后面所有题后应回收它
        s.complete_current();   // ex-1
        s.complete_current();   // ex-2
        s.complete_current();   // ex-3
        check(!s.done(), "还有漏掉的练习，不算全部完成");
        check(s.current().id == "ex-new", "走到末尾时回收被跳过的 ex-new");
        s.complete_current();
        check(s.done(), "回收完才真正结束");
    }
}

void explicit_start_wins_over_state() {
    auto path = temp_state("explicit");
    StateStore state(path);
    Session s(make_exercises(5), &state);
    s.seek_start("");
    s.complete_current();       // current 变成 ex-1

    Session again(make_exercises(5), &state);
    again.seek_start("ex-3");
    check(again.current().id == "ex-3", "显式指定优先于持久化状态");
}

void start_matches_substring() {
    auto path = temp_state("substr");
    StateStore state(path);
    std::vector<Exercise> list{
        {.id = "cpp11-04-rvalue-references", .order = 0, .title = "t", .chapter = "c", .files = {"/tmp/a.cpp"}},
        {.id = "cpp11-05-move-semantics-0",  .order = 1, .title = "t", .chapter = "c", .files = {"/tmp/b.cpp"}},
    };
    Session s(list, &state);
    s.seek_start("move");
    check(s.current().id == "cpp11-05-move-semantics-0", "子串匹配定位（学员只敲关键词）");
}

void unknown_start_falls_back_to_first_incomplete() {
    auto path = temp_state("unknown");
    StateStore state(path);
    Session s(make_exercises(3), &state);
    s.seek_start("no-such-exercise");
    check(s.current().id == "ex-0", "未匹配到时退回第一道未完成练习");
}

// 已完成的练习不该被重复要求做
void skips_completed_on_fresh_seek() {
    auto path = temp_state("skip");
    StateStore state(path);
    state.mark_completed("ex-0");
    state.mark_completed("ex-1");

    Session s(make_exercises(4), &state);
    s.seek_start("");
    check(s.current().id == "ex-2", "跳过已完成的 ex-0/ex-1");
}

// 学员把做完的题改坏了，应该能退回重做
void unmark_allows_redo() {
    auto path = temp_state("unmark");
    StateStore state(path);
    state.mark_completed("ex-0");
    state.unmark("ex-0");

    Session s(make_exercises(2), &state);
    s.seek_start("");
    check(s.completed_count() == 0, "撤销完成标记后计数归零");
    check(s.current().id == "ex-0", "撤销后回到该题");
}

// 状态文件损坏不该让学员的会话崩掉
void corrupt_state_file_is_tolerated() {
    auto path = temp_state("corrupt");
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "{ this is not valid json";

    StateStore state(path);                     // 不应抛异常
    Session s(make_exercises(2), &state);
    s.seek_start("");
    check(s.completed_count() == 0, "损坏的状态文件当作空状态");
    check(s.current().id == "ex-0", "损坏状态下仍从头开始");
}

// 没有 StateStore 也要能工作（一次性检查、CI 场景）
void works_without_state_store() {
    Session s(make_exercises(3), nullptr);
    s.seek_start("");
    check(s.completed_count() == 0, "无状态存储时完成数为 0");
    check(s.current().id == "ex-0", "无状态存储时从头开始");
    s.complete_current();
    check(s.current().id == "ex-1", "无状态存储时仍能推进");
}

} // namespace

int main() {
    empty_session_is_done();
    advances_in_order();
    completed_count_tracks_progress();
    resumes_from_persisted_state();
    reordering_preserves_progress();
    explicit_start_wins_over_state();
    start_matches_substring();
    unknown_start_falls_back_to_first_incomplete();
    skips_completed_on_fresh_seek();
    unmark_allows_redo();
    corrupt_state_file_is_tolerated();
    works_without_state_store();

    std::println("session: {}/{} 通过", g_total - g_failed, g_total);
    return g_failed == 0 ? 0 : 1;
}
