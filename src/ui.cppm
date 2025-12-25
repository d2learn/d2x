export module d2x.ui;

import std;

import d2x.log;
import d2x.ui.interface;
import d2x.ui.loader;
import d2x.ui.plugin.tui.simple_tui;
import d2x.ui.plugin.print.simple_print;

namespace d2x {

namespace internal {
    inline std::unique_ptr<IUIBackend> g_ui_backend;
    inline bool g_initialized = false;
}

// Initialize UI system with available backends
void init_ui_system() {
    if (internal::g_initialized) return;

    log::info("Initializing UI system...");

    // Register built-in backends
    UILoader::register_backend("tui", std::make_unique<TUIBackendFactory>());
    UILoader::register_backend("simple_print", std::make_unique<SimplePrintBackendFactory>());
    
    internal::g_initialized = true;

    log::info("UI system initialized.");
}

export namespace ui {

// Start UI with the currently loaded backend
void start() {
    if (!internal::g_ui_backend) {
        init_ui_system();
        // Default to simple_print backend
        // read ENV variable D2X_UI_BACKEND for backend choice in future
        const char* backend_env = std::getenv("D2X_UI_BACKEND");
        const std::string backend = backend_env ? backend_env : "simple_print";
        log::info("Starting UI with backend: {}", backend);
        internal::g_ui_backend = UILoader::load(backend);
        log::info("UI backend '{}' loaded.", backend);
    }
    internal::g_ui_backend->start();
}

// Stop the UI backend
void stop() {
    if (internal::g_ui_backend) {
        internal::g_ui_backend->stop();
        internal::g_ui_backend.reset();
    }
}

// Switch to a different UI backend by name
void switch_backend(std::string_view backend_name) {
    if (!internal::g_initialized) {
        init_ui_system();
    }
    
    // Stop current backend if running
    if (internal::g_ui_backend) {
        internal::g_ui_backend->stop();
    }
    
    // Load new backend
    internal::g_ui_backend = UILoader::load(backend_name);
    internal::g_ui_backend->start();
}

// Update AI tips
void update_ai_tips(std::string ai_tips) {
    if (!internal::g_ui_backend) start();
    internal::g_ui_backend->update_ai_tips(std::move(ai_tips));
}

// Update UI state
void update(
    std::string target, std::vector<std::string> target_files,
    int built_targets, int total_targets,
    std::string output, bool status, std::string ai_tips = ""
) {
    if (!internal::g_ui_backend) {
        start();
    }

    UIState state;
    state.target = std::move(target);
    state.target_files = std::move(target_files);
    state.built_targets = built_targets;
    state.total_targets = total_targets;
    state.output = std::move(output);
    state.status = status;
    state.ai_tips = std::move(ai_tips);

    internal::g_ui_backend->update(std::move(state));
}

// Get list of available backends
std::vector<std::string> get_available_backends() {
    if (!internal::g_initialized) {
        init_ui_system();
    }
    return UILoader::available_backends();
}

} // namespace ui
} // namespace d2x
