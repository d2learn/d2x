// 上行 Frontend Protocol：d2x → 前端。
//
// 单向。d2x 保留全部控制权（通过即自动前进，失败即等文件变更），前端纯显示。
//
// 与下行 Provider Protocol 同一种机制（NDJSON 事件流），不同词汇表。
// stage / output / verdict 三类事件两侧同构，d2x 对它们基本是转发 + 补上
// 会话上下文；只属于上行的是 session / exercise / waiting / hint / done。
// 所以不是两套无关的协议，而是「下行是上行的子集」。
//
// 内置前端走内存通道（UiSink），外部客户端走管道（StdoutSink），
// 二者消费同一套事件类型 —— `d2x checker` 对学员仍然是一条命令。
module;

// stdout / fflush 是 C 运行时的宏与符号，import std 不提供
#include <cstdio>

export module d2x.emit;

import std;

import d2x.domain;
import d2x.ui;
import d2x.json;

namespace d2x::emit {

using domain::Exercise;
using domain::Outcome;
using domain::Verdict;

export class IEventSink {
public:
    virtual ~IEventSink() = default;

    virtual void session(int total, int completed, std::string_view current) = 0;
    virtual void exercise(const Exercise& ex) = 0;
    virtual void stage(std::string_view name) = 0;
    virtual void output(std::string_view chunk) = 0;
    virtual void verdict(const Verdict& v) = 0;
    virtual void waiting(std::string_view reason) = 0;
    virtual void hint(std::string_view text) = 0;
    virtual void done() = 0;
};

// ── 外部客户端：NDJSON over stdout ─────────────────────────────────
//
// VSCode 插件、Web 前端、CI 都只是这条流的消费者，不需要链接 d2x。
export class StdoutSink final : public IEventSink {
    static void line(const nlohmann::json& ev) {
        std::println("{}", ev.dump());
        std::fflush(stdout);   // 前端是逐行读的，缓冲会让实时性失效
    }

public:
    void session(int total, int completed, std::string_view current) override {
        line({{"event", "session"}, {"total", total},
              {"completed", completed}, {"current", current}});
    }

    void exercise(const Exercise& ex) override {
        line({{"event", "exercise"}, {"id", ex.id}, {"order", ex.order},
              {"title", ex.title}, {"chapter", ex.chapter}, {"files", ex.files}});
    }

    void stage(std::string_view name) override {
        line({{"event", "stage"}, {"name", name}});
    }

    void output(std::string_view chunk) override {
        line({{"event", "output"}, {"chunk", chunk}});
    }

    void verdict(const Verdict& v) override {
        nlohmann::json diags = nlohmann::json::array();
        for (const auto& d : v.diagnostics) {
            diags.push_back({{"file", d.file}, {"line", d.line}, {"col", d.col},
                             {"severity", d.severity}, {"message", d.message}});
        }
        line({{"event", "verdict"}, {"outcome", domain::to_string(v.outcome)},
              {"stage", v.stage}, {"diagnostics", diags}});
    }

    void waiting(std::string_view reason) override {
        line({{"event", "waiting"}, {"reason", reason}});
    }

    void hint(std::string_view text) override {
        line({{"event", "hint"}, {"text", text}});
    }

    void done() override { line({{"event", "done"}}); }
};

// ── 内置前端：内存通道 ─────────────────────────────────────────────
//
// 现有的 TUI/print 后端一次性接收整页状态，而协议是增量事件。
// 这里承担二者之间的适配：累积事件，在合适的时机刷新页面。
//
// 这段「攒状态」的逻辑原先散在 checker 的循环里，搬到这里之后编排层
// 只管发事件，不再关心页面怎么拼。
export class UiSink final : public IEventSink {
    Exercise    mExercise;
    int         mTotal{};
    int         mCompleted{};
    std::string mStage;
    std::string mOutput;
    std::string mHint;
    bool        mOk{false};

    void refresh() {
        auto body = mStage.empty() ? mOutput
                                   : std::format("[{}]\n{}", mStage, mOutput);
        ui::update_checker_page(mExercise.id, mExercise.files,
                                mCompleted, mTotal, body, mOk, mHint);
    }

public:
    void session(int total, int completed, std::string_view current) override {
        mTotal = total;
        mCompleted = completed;
        (void)current;
    }

    void exercise(const Exercise& ex) override {
        mExercise = ex;
        mStage.clear();
        mOutput.clear();
        mHint.clear();
        mOk = false;
    }

    void stage(std::string_view name) override {
        mStage.assign(name);
        mOutput.clear();   // 新阶段开始，上一阶段的输出已经看过了
        refresh();
    }

    void output(std::string_view chunk) override {
        mOutput.append(chunk);
        refresh();
    }

    void verdict(const Verdict& v) override {
        mOk = (v.outcome == Outcome::Pass);
        mStage.clear();
        refresh();
    }

    void waiting(std::string_view) override { /* TUI 用进度条表达，无需额外动作 */ }

    void hint(std::string_view text) override {
        mHint.assign(text);
        refresh();
    }

    void done() override {}
};

} // namespace d2x::emit
