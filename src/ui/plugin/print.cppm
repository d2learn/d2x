module;

#include <cstdio>

export module d2x.ui.plugin.print;

import std;

import d2x.ui.interface;
import :checker_page;
import :help_page;

namespace d2x {

//  print-based UI backend with multi-page support
export struct PrintBackend : public IUIBackend {
    void start() override {
        register_page<ICheckerPageUI>(PageID::Checker, std::make_unique<PrintCheckerPage>());
        register_page<IHelpPageUI>(PageID::Help, std::make_unique<PrintHelpPage>());
    }

    void stop() override {
        // No-op for  print
    }
};

// Factory
export class PrintBackendFactory : public IUIBackendFactory {
public:
    std::unique_ptr<IUIBackend> create() override {
        return std::make_unique<PrintBackend>();
    }

    std::string_view name() const override {
        return "print";
    }
};

} // namespace d2x
