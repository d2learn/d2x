module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>

export module d2x.ui.plugin.tui.simple_tui;

import std;
import d2x.ui.interface;
import d2x.utils;
import d2x.platform;

namespace d2x {

// Minimal Pixel-art TUI Backend
export class TUIBackend : public IUIBackend {
public:
    void start() override {
        animation_frame_ = 0;
    }
    
    void stop() override {}

    void update(UIState state) override {
        std::lock_guard lock(mutex_);
        if (state.ai_tips.empty())
            state.ai_tips = state_.ai_tips;
        state_ = state;
        animation_frame_++;
        render();
    }

    void update_ai_tips(std::string ai_tips) override {
        std::lock_guard lock(mutex_);
        state_.ai_tips = std::move(ai_tips);
        animation_frame_++;
        render();
    }

private:
    static constexpr int STATUS_LINES = 5;
    static constexpr int AI_MIN_LINES = 6;

    std::vector<std::string> split_lines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    void render() {
        using namespace ftxui;
        
        platform::clear_console();

        auto terminal_size = Terminal::Size();
        const int term_height = terminal_size.dimy > 0 ? terminal_size.dimy : 24;

        const int bar_width = 40;
        const int total = state_.total_targets;
        const int built = state_.built_targets;
        const float ratio = total > 0 ? static_cast<float>(built) / total : 0.0f;
        const int filled = static_cast<int>(ratio * bar_width);

        std::string bar_content;
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) {
                bar_content += (animation_frame_ % 2 == 0) ? "█" : "▓";
            } else if (i == filled && filled < bar_width) {
                const std::string cursors[] = {"▒", "░", "▒"};
                bar_content += cursors[(animation_frame_ / 2) % 3];
            } else {
                bar_content += "░";
            }
        }

        // Status Area
        auto progress_display = hbox({
            text(" 🌏 ") | bold | color(Color::Cyan),
            text(bar_content) | (ratio >= 1.0f ? color(Color::Green) : color(Color::YellowLight)),
            text(std::format(" {}/{} ", built, total)) | bold | color(Color::White)
        });

        const char* status_icon = state_.status ? "✓" : "✗";
        auto status_color = state_.status ? Color::Green : Color::Red;

        auto target_display = hbox({
            text(" "),
            text(status_icon) | bold | color(status_color),
            text(" "),
            text(state_.target) | color(Color::Magenta)
        });

        const auto target_file = utils::normalize_path(
            state_.target_files.empty() ? std::string{} : state_.target_files.front()
        );

        Elements status_elements;
        status_elements.push_back(progress_display);
        status_elements.push_back(text(""));
        status_elements.push_back(target_display);
        if (!target_file.empty()) {
            status_elements.push_back(hbox({
                text(" +") | color(Color::Yellow),
                text(" → ") | color(Color::Blue),
                text(target_file) | color(Color::GrayDark)
            }));
        }
        status_elements.push_back(text(""));

        auto status_section = vbox(std::move(status_elements));
        auto status_screen = Screen::Create(Dimension::Full(), Dimension::Fit(status_section));
        Render(status_screen, status_section);

        status_screen.Print();
        std::println("");
        
        // AI Area Height Calculation
        auto ai_lines = split_lines(state_.ai_tips);
        int ai_height = state_.ai_tips.empty() ? 0 : static_cast<int>(ai_lines.size()) + 2;

        // Output Area Height Calculation
        int available_for_output = term_height - STATUS_LINES - std::max(ai_height, AI_MIN_LINES);
        available_for_output = std::max(available_for_output, 3);

        auto output_lines = split_lines(state_.output);
        int total_output_lines = static_cast<int>(output_lines.size());
        int display_count = std::min(total_output_lines, available_for_output - 1);
        int remaining = total_output_lines - display_count;

        for (int i = 0; i < display_count; ++i) {
            std::println(" {}", output_lines[i]);
        }
        if (remaining > 0) {
            std::println("\033[90m   ▼ {} more lines below...\033[0m", remaining);
        }
        std::println("");

        // AI Area
        if (!state_.ai_tips.empty()) {
            // Blinking animation icons
            const char* ai_icons[] = { "🤖", "✨", "👾", "🧠", "🎮" };
            const char* ai_icon = ai_icons[(animation_frame_ / 2) % 5];

            std::print("\033[1;36m{}:\033[0m", ai_icon);
            for (const auto& line : ai_lines) {
                std::println("\033[90m   {}\033[0m", line);
            }
            std::println("");
        }

        std::fflush(stdout);
    }

    UIState state_;
    std::mutex mutex_;
    int animation_frame_{0};
};

// Factory
export class TUIBackendFactory : public IUIBackendFactory {
public:
    std::unique_ptr<IUIBackend> create() override {
        return std::make_unique<TUIBackend>();
    }

    std::string_view name() const override {
        return "tui";
    }
};

} // namespace d2x
