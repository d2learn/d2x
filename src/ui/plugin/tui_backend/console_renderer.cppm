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

static const ftxui::Color kPrimary = ftxui::Color::RGB(64, 120, 255);
static const ftxui::Color kAccent  = ftxui::Color::RGB(120, 213, 255);
static const ftxui::Color kPanel   = ftxui::Color::RGB(24, 28, 38);
static const ftxui::Color kTextDim = ftxui::Color::GrayDark;

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

    return ftxui::hbox({
        ftxui::text("Progress"),
        ftxui::separator(),
        ftxui::gauge(ratio) | ftxui::color(ftxui::Color::Green) | ftxui::flex,
        ftxui::text(std::format(" {}/{}", built_targets, total_targets)),
    });
}

ftxui::Element build_status(bool status, const std::string& target, const std::string& target_file) {
    const auto status_text = status ? "✅  Build & run succeeded" : "❌  Build or run failed";
    const auto status_color = status ? ftxui::Color::Green : ftxui::Color::Red;

    return ftxui::vbox({
        ftxui::hbox({ftxui::text("Target"), ftxui::separator(), ftxui::text(target)}),
        ftxui::hbox({ftxui::text("File"), ftxui::separator(), ftxui::text(target_file)}),
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

private:
    void render(const ConsoleState& state) {
        const auto target_file = internal::normalize_path(
            state.target_files.empty() ? std::string{} : state.target_files.front()
        );

        const auto status_badge = ftxui::text(state.status ? "  RUNNING CLEAN  " : "  NEEDS FIX  ")
            | ftxui::color(ftxui::Color::White)
            | ftxui::bgcolor(state.status ? internal::kPrimary : ftxui::Color::Red);

        const auto spinner_char = internal::spinner();

        auto header = ftxui::window(
            ftxui::hbox({ftxui::text(" d2x checker ") | ftxui::bold}),
            ftxui::hbox({
                ftxui::text(std::format(" {} live reload", spinner_char)) | ftxui::color(internal::kTextDim),
                ftxui::filler(),
                status_badge,
                ftxui::separator(),
                ftxui::text("github.com/d2learn/d2x") | ftxui::color(internal::kTextDim)
            })
        ) | ftxui::bgcolor(internal::kPanel);

        const auto output_view = state.output.empty()
            ? ftxui::paragraph("(no output yet)") | ftxui::color(internal::kTextDim)
            : internal::render_ansi_text(state.output);

        auto document = ftxui::vbox({
            header,
            ftxui::separator(),
            ftxui::hbox({
                ftxui::window(
                    ftxui::text(" Progress ") | ftxui::color(internal::kTextDim),
                    ftxui::vbox({
                        internal::build_progress(state.built_targets, state.total_targets),
                        ftxui::text(std::format("Targets: {} / {}", state.built_targets, state.total_targets)) 
                            | ftxui::color(internal::kTextDim),
                    })
                ) | ftxui::bgcolor(internal::kPanel) | ftxui::flex,

                ftxui::separator(),

                ftxui::window(
                    ftxui::text(" Status ") | ftxui::color(internal::kTextDim),
                    ftxui::vbox({
                        internal::build_status(state.status, state.target, target_file),
                        ftxui::text(state.status ? "Everything looks good." : "Fix errors, file will auto-rerun.")
                            | ftxui::color(state.status ? ftxui::Color::Green : ftxui::Color::Red),
                    })
                ) | ftxui::bgcolor(internal::kPanel) | ftxui::flex,
            }),
            ftxui::separator(),
            ftxui::window(
                ftxui::text(" Output " ) | ftxui::color(internal::kTextDim),
                output_view
            ) | ftxui::bgcolor(internal::kPanel),
        }) | ftxui::borderHeavy | ftxui::bgcolor(ftxui::Color::Black);

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
