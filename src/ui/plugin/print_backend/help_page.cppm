module d2x.ui.plugin.print:help_page;

import std;

import d2x.ui.interface;

namespace d2x {

// print-based help page implementation
struct PrintHelpPage : public IHelpPageUI {
    void update(const UIState& state) override {
        std::println("{}", state.help);
    }
};

} // namespace d2x
