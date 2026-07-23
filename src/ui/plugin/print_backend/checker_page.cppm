module;

// stdout 是宏,import std 不提供,flush 需要它走全局模块片段
#include <cstdio>

export module d2x.ui.plugin.print:checker_page;

import std;

import d2x.utils;
import d2x.platform;
import d2x.msg;
import d2x.ui.interface;


namespace d2x {

// print 页面:固定分区(Progress / Exercise / Status / Checks / Output / Hint)。
// 结构化诊断置顶(最多 5 条);长输出头 20 + 尾 30 行截断,全量在
// .d2x/last-output.log——截断不丢信息(2026-07-24 设计文档 D5)。
class PrintCheckerPage : public ICheckerPageUI {
    UIState mState__;
    std::mutex mConsoleMutex__;

    static constexpr int kHeadLines   = 20;
    static constexpr int kTailLines   = 30;
    static constexpr int kMaxChecks   = 5;

    void render() {
        std::lock_guard lock(mConsoleMutex__);
        d2x::platform::clear_console();

        // Progress
        std::string bar;
        if (mState__.total > mState__.completed)
            bar += ">" + std::string(static_cast<std::size_t>(mState__.total - mState__.completed - 1), '-');
        if (mState__.completed > 0)
            bar = std::string(static_cast<std::size_t>(mState__.completed), '=') + bar;
        std::println("Progress: [\033[32m{}\033[0m] {}/{}", bar, mState__.completed, mState__.total);
        std::println("");

        // Exercise
        auto file = utils::normalize_path(
            mState__.files.empty() ? std::string{} : mState__.files.front());
        if (mState__.chapter.empty())
            std::println("Exercise: {}", mState__.exercise);
        else
            std::println("Exercise: {} ({})", mState__.exercise, mState__.chapter);
        std::println("File:     {}", file);
        std::println("");

        // Status(三态;空 = 检测中)
        std::string_view status_line =
            mState__.outcome == "pass"    ? msg::text(msg::Key::StatusPass)
          : mState__.outcome == "blocked" ? msg::text(msg::Key::StatusBlocked)
          : mState__.outcome == "fail"    ? msg::text(msg::Key::StatusFail)
                                          : msg::text(msg::Key::StatusChecking);
        std::println("Status:   {}", status_line);

        // Checks:结构化诊断置顶——学习者第一眼看到「哪一行没过」
        if (!mState__.checks.empty()) {
            std::println("");
            int shown = 0;
            for (const auto& c : mState__.checks) {
                if (shown++ == kMaxChecks) {
                    std::println("  … ({} more)", mState__.checks.size() - kMaxChecks);
                    break;
                }
                std::println("  • {}", c);
            }
        }

        // Output:头尾截断,全量另存
        std::println("\n---\n");
        auto lines = split_lines(mState__.output);
        if (std::cmp_less_equal(lines.size(), kHeadLines + kTailLines)) {
            for (auto& l : lines) std::println("{}", l);
        } else {
            for (std::size_t i = 0; i < kHeadLines; ++i) std::println("{}", lines[i]);
            std::println("");
            std::size_t omitted = lines.size() - kHeadLines - kTailLines;
            std::println("\033[33m{}\033[0m",
                std::vformat(msg::text(msg::Key::OutputOmitted),
                             std::make_format_args(omitted, mState__.output_log_path)));
            std::println("");
            for (std::size_t i = lines.size() - kTailLines; i < lines.size(); ++i)
                std::println("{}", lines[i]);
        }
        std::println("\n---");

        // Hint
        std::println("🤖: {}", mState__.hint.empty()
                                   ? std::string(msg::text(msg::Key::AiDisabled))
                                   : mState__.hint);

        // 页面渲染完整体 flush 一次。stdout 重定向到管道/文件时是全缓冲,
        // 而 clear_console 经子进程直写 fd 绕过了缓冲——不 flush 的话,
        // 非 TTY 消费者只能看到清屏序列,页面内容永远滞留在缓冲区。
        std::fflush(stdout);
    }

    static std::vector<std::string> split_lines(const std::string& s) {
        std::vector<std::string> lines;
        std::string cur;
        for (char c : s) {
            if (c == '\n') { lines.push_back(std::move(cur)); cur.clear(); }
            else           { cur += c; }
        }
        if (!cur.empty()) lines.push_back(std::move(cur));
        return lines;
    }

public:
    void update(const UIState& state) override {
        if (state.only_update_hint) {
            mState__.hint = state.hint;
        } else {
            std::string old_hint = std::move(mState__.hint);
            mState__ = state;
            if (state.hint.empty() && !old_hint.empty()) {
                mState__.hint = std::move(old_hint);
            }
        }
        render();
    }
};

export std::unique_ptr<ICheckerPageUI> make_print_checker_page() {
    return std::make_unique<PrintCheckerPage>();
}

} // namespace d2x
