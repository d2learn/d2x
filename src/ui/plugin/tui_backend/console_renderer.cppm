module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

export module d2x.ui.plugin.tui.console_renderer;

import std;
import d2x.ui.interface;
import d2x.ui.plugin.tui.ui_thread_helper;

namespace d2x {

namespace internal {

static const ftxui::Color kBg         = ftxui::Color::RGB(242, 243, 245);
static const ftxui::Color kCard       = ftxui::Color::RGB(252, 253, 255);
static const ftxui::Color kCardBorder = ftxui::Color::RGB(218, 223, 232);
static const ftxui::Color kAccent     = ftxui::Color::RGB(76, 146, 255);
static const ftxui::Color kSuccess    = ftxui::Color::RGB(156, 209, 165);
static const ftxui::Color kTextSubtle = ftxui::Color::RGB(114, 122, 138);
static const ftxui::Color kTextStrong = ftxui::Color::RGB(40, 50, 70);
static const ftxui::Color kOutputBg   = ftxui::Color::RGB(22, 24, 30);

ftxui::Decorator color_from_code(int code) {
    switch (code) {
        case 30: return ftxui::color(ftxui::Color::Black);
        case 31: return ftxui::color(ftxui::Color::Red);
        case 32: return ftxui::color(ftxui::Color::Green);
        case 33: return ftxui::color(ftxui::Color::Yellow);
        case 34: return ftxui::color(ftxui::Color::Blue);
        case 35: return ftxui::color(ftxui::Color::Magenta);
        case 36: return ftxui::color(ftxui::Color::Cyan);
        case 37: return ftxui::color(ftxui::Color::White);
        case 90: return ftxui::color(ftxui::Color::GrayDark);
        case 91: return ftxui::color(ftxui::Color::LightCoral);
        case 92: return ftxui::color(ftxui::Color::LightGreen);
        case 93: return ftxui::color(ftxui::Color::YellowLight);
        case 94: return ftxui::color(ftxui::Color::BlueLight);
        case 95: return ftxui::color(ftxui::Color::MagentaLight);
        case 96: return ftxui::color(ftxui::Color::CyanLight);
        case 97: return ftxui::color(ftxui::Color::White);
        default: return ftxui::Decorator{};
    }
}

ftxui::Element render_ansi_text(const std::string& text) {
    std::vector<ftxui::Element> lines;
    std::string current;
    std::optional<ftxui::Decorator> active_color;

    auto flush_line = [&](std::vector<ftxui::Element>& line_parts) {
        if (!current.empty()) {
            auto elem = ftxui::text(current);
            if (active_color) {
                elem = elem | *active_color;
            }
            line_parts.push_back(std::move(elem));
            current.clear();
        }
    };

    std::vector<ftxui::Element> line_parts;

    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '\n') {
            flush_line(line_parts);
            lines.push_back(ftxui::hbox(std::move(line_parts)));
            line_parts.clear();
            continue;
        }

        if (ch == '\x1B' && i + 1 < text.size() && text[i + 1] == '[') {
            flush_line(line_parts);
            i += 2;
            std::string code;
            while (i < text.size() && text[i] != 'm') {
                code.push_back(text[i]);
                ++i;
            }

            size_t start = 0;
            while (start <= code.size()) {
                const auto end = code.find(';', start);
                const auto token = code.substr(start, end == std::string::npos ? code.size() - start : end - start);
                if (!token.empty()) {
                    const int parsed = std::atoi(token.c_str());
                    const int last_code = parsed;
                    if (last_code == 0) {
                        active_color.reset();
                    } else {
                        auto deco = color_from_code(last_code);
                        active_color = deco;
                    }
                }
                if (end == std::string::npos) break;
                start = end + 1;
            }
            continue;
        }

        current.push_back(ch);
    }

    flush_line(line_parts);
    if (!line_parts.empty()) {
        lines.push_back(ftxui::hbox(std::move(line_parts)));
    }

    if (lines.empty()) {
        return ftxui::paragraph(" ");
    }

    return ftxui::vbox(std::move(lines));
}

char spinner() {
    static constexpr std::array<char, 4> frames {'|', '/', '-', '\\'};
    static std::size_t tick = 0;
    const auto ch = frames[tick % frames.size()];
    tick = (tick + 1) % frames.size();
    return ch;
}

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

ftxui::Element build_progress(int built_targets, int total_targets) {
    const auto ratio = total_targets > 0
        ? std::clamp(static_cast<double>(built_targets) / static_cast<double>(total_targets), 0.0, 1.0)
        : 0.0;

    const int percent = static_cast<int>(std::round(ratio * 100.0));

    return ftxui::vbox({
        ftxui::text("Progress") | ftxui::bold | ftxui::color(kTextStrong),
        ftxui::text(" "),
        ftxui::hbox({
            ftxui::gauge(ratio) | ftxui::color(kAccent) | ftxui::flex,
            ftxui::text(std::format(" {}%", percent)) | ftxui::color(kTextSubtle) | ftxui::bold,
        }),
        ftxui::text(std::format("Targets: {} / {}", built_targets, total_targets)) | ftxui::color(kTextSubtle),
    });
}

ftxui::Element build_status(bool status, const std::string& target, const std::string& target_file) {
    const auto status_text = status ? "✅  Build & run succeeded" : "❌  Build or run failed";
    const auto status_color = status ? ftxui::Color::Green : ftxui::Color::Red;

    return ftxui::vbox({
        ftxui::text("Status") | ftxui::bold | ftxui::color(kTextStrong),
        ftxui::text(" "),
        ftxui::hbox({ftxui::text("Target") | ftxui::color(kTextSubtle), ftxui::separator(), ftxui::text(target) | ftxui::color(kTextStrong)}),
        ftxui::hbox({ftxui::text("File") | ftxui::color(kTextSubtle), ftxui::separator(), ftxui::text(target_file) | ftxui::color(kTextStrong)}),
        ftxui::text(status_text) | ftxui::color(status_color) | ftxui::bold,
    });
}

} // namespace internal

// TUI Backend using FTXUI with 24fps rendering thread
export class TUIBackend : public IUIBackend {
public:
    TUIBackend(int fps = 24) {
        ui_thread_ = std::make_unique<UIThread<ConsoleState>>(
            [this](const ConsoleState& state) { render(state); },
            fps
        );
    }

    void start() override {
        ui_thread_->start();
    }

    void stop() override {
        ui_thread_->stop();
    }

    void update(ConsoleState state) override {
        ui_thread_->update_state(std::move(state));
    }

    void update_ai_tips(std::string ai_tips) override {
        // Update only the ai_tips field in the current state
        ui_thread_->update_field([tips = std::move(ai_tips)](ConsoleState& state) mutable {
            state.ai_tips = std::move(tips);
        });
    }

private:
    void render(const ConsoleState& state) {
        const auto target_file = internal::normalize_path(
            state.target_files.empty() ? std::string{} : state.target_files.front()
        );

        const auto spinner_char = internal::spinner();

        const auto status_badge = ftxui::text(state.status ? " RUNNING CLEAN " : " NEEDS FIX ")
            | ftxui::bgcolor(state.status ? internal::kSuccess : ftxui::Color::LightCoral)
            | ftxui::color(internal::kTextStrong)
            | ftxui::borderRounded
            | ftxui::bold;

        auto live_reload_toggle = ftxui::hbox({
            ftxui::text("Live Reload") | ftxui::color(internal::kTextSubtle),
            ftxui::text("  " ),
            ftxui::text(" ON ") | ftxui::bgcolor(internal::kAccent) | ftxui::color(ftxui::Color::White) | ftxui::borderRounded | ftxui::bold,
            ftxui::text(std::format("  {}", spinner_char)) | ftxui::color(internal::kTextSubtle),
        });

        auto card = [](ftxui::Element content) {
            return ftxui::vbox({
                ftxui::text(""),
                ftxui::hbox({ftxui::text("  "), content | ftxui::flex, ftxui::text("  ")}),
                ftxui::text(""),
            }) | ftxui::bgcolor(internal::kCard) | ftxui::borderRounded | ftxui::color(internal::kCardBorder);
        };

        auto header = card(
            ftxui::vbox({
                ftxui::text("d2x checker") | ftxui::bold | ftxui::color(internal::kTextStrong),
                ftxui::text(" "),
                ftxui::hbox({
                    live_reload_toggle,
                    ftxui::filler(),
                    status_badge,
                    ftxui::text("  "),
                    ftxui::text("github.com/d2learn/d2x") | ftxui::color(internal::kTextSubtle),
                }),
            })
        );

        const auto output_view = state.output.empty()
            ? ftxui::paragraph("(no output yet)") | ftxui::color(internal::kTextSubtle)
            : internal::render_ansi_text(state.output);

        auto progress_card = card(internal::build_progress(state.built_targets, state.total_targets));
        auto status_card = card(
            ftxui::vbox({
                internal::build_status(state.status, state.target, target_file),
                ftxui::text(state.status ? "Everything looks good." : "Fix errors, file will auto-rerun.")
                    | ftxui::color(state.status ? ftxui::Color::Green : ftxui::Color::Red),
            })
        );

        auto output_card = card(
            ftxui::vbox({
                ftxui::text("Output") | ftxui::bold | ftxui::color(internal::kTextStrong),
                ftxui::text(" "),
                ftxui::vbox({output_view | ftxui::flex})
                    | ftxui::bgcolor(internal::kOutputBg)
                    | ftxui::borderRounded
                    | ftxui::flex,
            })
        );

        auto document = ftxui::vbox({
            header,
            ftxui::text(" "),
            ftxui::hbox({progress_card | ftxui::flex, ftxui::text("  "), status_card | ftxui::flex}),
            ftxui::text(" "),
            output_card,
        }) | ftxui::bgcolor(internal::kBg) | ftxui::flex;

        auto screen = ftxui::Screen::Create(ftxui::Dimension::Full(), ftxui::Dimension::Full());
        ftxui::Render(screen, document);
        
        std::print("\033[2J\033[H{}", screen.ToString());
        std::fflush(stdout);
    }

    std::unique_ptr<UIThread<ConsoleState>> ui_thread_;
};

// Factory for creating TUI backends
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
