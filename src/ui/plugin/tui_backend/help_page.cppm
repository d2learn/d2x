module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

module d2x.ui.plugin.tui:help_page;

import std;
import d2x.ui.interface;
import d2x.platform;

namespace d2x {

// TUI Help Page implementation
class HelpPage : public IHelpPageUI {
    UIState mState;

    void render() {
        d2x::platform::clear_console();

        using namespace ftxui;

        auto content = vbox({
            text(" Help ") | bold | color(Color::Cyan),
            text(""),
            text("Usage: d2x [options]"),
            text(""),
            text("Options:"),
            text("  --target <name>    Specify build target"),
            text("  --file <path>      Specify source file"),
            text("  --ui <backend>     UI backend (print/tui)"),
            text("  --help             Show this help message"),
            text(""),
            text(mState.help) | color(Color::GrayDark)
        });

        auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(content));
        Render(screen, content);
        screen.Print();
    }

public:
    void update(const UIState& state) override {
        mState = state;
        render();
    }
};

} // namespace d2x
