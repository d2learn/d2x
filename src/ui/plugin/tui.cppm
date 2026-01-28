module;

#include <cstdio>

export module d2x.ui.plugin.tui;

import std;

import d2x.ui.interface;
import :checker_page;
import :help_page;

namespace d2x {

// TUI Backend with multi-page support
export struct TUIBackend : public IUIBackend {
    void start() override {
        register_page<ICheckerPageUI>(PageID::Checker, std::make_unique<CheckerPage>());
        register_page<IHelpPageUI>(PageID::Help, std::make_unique<HelpPage>());
    }

    void stop() override {
        // Cleanup TUI if needed
    }
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
