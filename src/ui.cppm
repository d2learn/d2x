export module d2x.ui;

import std;

import d2x.log;
import d2x.config;
import d2x.ui.interface;
import d2x.ui.loader;
import d2x.ui.plugin.tui;
import d2x.ui.plugin.print;

namespace d2x {

namespace internal {
    inline std::unique_ptr<IUIBackend> g_ui_backend;
    inline bool g_initialized = false;

    void init() {
        if (g_initialized) return;
        log::info("Initializing UI system...");
        UILoader::register_backend("tui", std::make_unique<TUIBackendFactory>());
        UILoader::register_backend("print", std::make_unique<PrintBackendFactory>());
        g_initialized = true;
    }

    void ensure_started() {
        if (g_ui_backend) return;
        init();
        std::string name = Config::ui_backend();
        log::info("Loading UI backend: {}", name);
        g_ui_backend = UILoader::load(name);
        g_ui_backend->start();
    }

    IUIBackend* backend() {
        ensure_started();
        return g_ui_backend.get();
    }
}

export namespace ui {

void switch_backend(std::string_view name) {
    internal::init();
    if (internal::g_ui_backend) {
        internal::g_ui_backend->stop();
        internal::g_ui_backend.reset();
    }
    internal::g_ui_backend = UILoader::load(name);
    internal::g_ui_backend->start();
}

void update_checker_page(
    std::string target, std::vector<std::string> target_files,
    int built_targets, int total_targets,
    std::string output, bool status, std::string ai_tips = ""
) {
    auto state = ICheckerPageUI::UIState{};
    state.target = std::move(target);
    state.target_files = std::move(target_files);
    state.built_targets = built_targets;
    state.total_targets = total_targets;
    state.output = std::move(output);
    state.status = status;
    state.ai_tips = std::move(ai_tips);
    internal::backend()->update_page<ICheckerPageUI>(PageID::Checker, state);
}

void update_ai_tips(std::string ai_tips) {
    auto state = ICheckerPageUI::UIState{};
    state.ai_tips = std::move(ai_tips);
    state.only_update_ai_tips = true;
    internal::backend()->update_page<ICheckerPageUI>(PageID::Checker, state);
}

void update_help(std::string help_text = "") {
    auto state = IHelpPageUI::UIState{};
    state.help = std::move(help_text);
    internal::backend()->update_page<IHelpPageUI>(PageID::Help, state);
}

std::vector<std::string> available_backends() {
    internal::init();
    return UILoader::available_backends();
}

} // namespace ui
} // namespace d2x
