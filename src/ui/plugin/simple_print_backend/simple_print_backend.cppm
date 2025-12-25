module;

#include <cstdio>

export module d2x.ui.plugin.simple_print.backend;

import std;

import d2x.platform;
import d2x.ui.interface;


namespace d2x {

// Simple print-based backend - no threads, just immediate output
export class SimplePrintBackend : public IUIBackend {
    ConsoleState mState;
    std::mutex mConsoleMutex;

private:
    std::string normalize_path(std::string path) {
        if (path.empty()) {
            return "N/A";
        }

        const auto current = std::filesystem::current_path().string();
        if (path.find(current) == 0) {
            path = path.substr(current.length());
            if (!path.empty() && path.front() == '/') {
                path.erase(path.begin());
            }
        }
        return path;
    }

public:
    void start() override {
        // No-op for simple print
    }

    void stop() override {
        // No-op for simple print
    }

    void update_ai_tips(std::string ai_tips) override {
        mState.ai_tips = std::move(ai_tips);
        update(mState);
    }

    void update(ConsoleState state) override {

        // add lock
        std::lock_guard lock(mConsoleMutex);

        // clear screen
        d2x::platform::clear_console();

        // 🌏Progress: [====>------] x/y
        // x == built targets, current progress
        // y == total targets
        std::string progress_bar = "";

        if (state.total_targets > state.built_targets)
            progress_bar += ">" + std::string(state.total_targets - state.built_targets - 1, '-');

        if (state.built_targets > 0) {
            progress_bar = std::string(state.built_targets, '=') + progress_bar;
        }

        progress_bar = std::string("\033[32m") + progress_bar + std::string("\033[0m");

        // Status
        auto status_str = state.status ? "✅ Ok: Compilation/Running succeeded" : "❌ Error: Compilation/Running failed";

        // target file (relative path)
        std::string target_file = normalize_path(state.target_files.empty() ? std::string{} : state.target_files[0]);

        // layout
        std::println("🌏Progress: [{}] {}/{}", progress_bar, state.built_targets, state.total_targets);
        std::println();    
        std::println("🎯 [Target: {}] 🎯", state.target);
        std::println("");
        std::println("{} for {}", status_str, target_file);

        std::println("");

        if (state.status) {
            std::println("\t🎉   The code is compiling!   🎉");
        } else {
            std::println("\t   The code exist some error!");
        }

        std::println("\n---\n");
        std::println("{}", state.output);
        std::println("---\n");

        if (state.ai_tips.empty())
            state.ai_tips = mState.ai_tips;

        std::println("🤡: {}", state.ai_tips);

        mState = std::move(state);
    }
};

} // namespace d2x

namespace d2x {

// Factory for creating simple print backends
export class SimplePrintBackendFactory : public IUIBackendFactory {
public:
    std::unique_ptr<IUIBackend> create() override {
        return std::make_unique<SimplePrintBackend>();
    }

    std::string_view name() const override {
        return "simple_print";
    }
};

} // namespace d2x
