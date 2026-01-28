module d2x.ui.plugin.print:checker_page;

import std;

import d2x.utils;
import d2x.platform;
import d2x.ui.interface;


namespace d2x {

// print-based checker page implementation
class PrintCheckerPage : public ICheckerPageUI {
    UIState mState__;
    std::mutex mConsoleMutex__;

    void render() {
        std::lock_guard lock(mConsoleMutex__);
        d2x::platform::clear_console();

        // Progress bar
        std::string progress_bar = "";
        if (mState__.total_targets > mState__.built_targets)
            progress_bar += ">" + std::string(mState__.total_targets - mState__.built_targets - 1, '-');
        if (mState__.built_targets > 0)
            progress_bar = std::string(mState__.built_targets, '=') + progress_bar;

        progress_bar = "\033[32m" + progress_bar + "\033[0m";

        // Status
        auto status_str = mState__.status
            ? "OK: Compilation/Running succeeded"
            : "Error: Compilation/Running failed";

        std::string target_file = utils::normalize_path(
            mState__.target_files.empty() ? std::string{} : mState__.target_files[0]
        );

        std::println("Progress: [{}] {}/{}", progress_bar, mState__.built_targets, mState__.total_targets);
        std::println("");
        std::println("Target: {}", mState__.target);
        std::println("{} for {}", status_str, target_file);
        std::println("");

        if (mState__.status) {
            std::println("  The code is compiling!");
        } else {
            std::println("  The code has some errors!");
        }

        std::println("\n---\n");
        std::println("{}", mState__.output);
        std::println("\n---");
        std::println("🤖: {}", mState__.ai_tips);
    }

public:
    void update(const UIState& state) override {
        // Only update ai_tips, preserve other fields
        if (state.only_update_ai_tips) {
            mState__.ai_tips = state.ai_tips;
        } else {
            std::string old_ai_tips = std::move(mState__.ai_tips);
            mState__ = state;
            if (state.ai_tips.empty() && !old_ai_tips.empty()) {
                mState__.ai_tips = std::move(old_ai_tips);
            }
        }
        render();
    }
};

} // namespace d2x
