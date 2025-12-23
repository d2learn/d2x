export module d2x.ui.loader;

import std;
import d2x.ui.interface;

namespace d2x {

// UI backend registry and loader
export class UILoader {
public:
    // Register a UI backend factory by name
    static void register_backend(std::string name, std::unique_ptr<IUIBackendFactory> factory) {
        instance().backends_[std::move(name)] = std::move(factory);
    }

    // Load UI backend by name
    static std::unique_ptr<IUIBackend> load(std::string_view name) {
        auto& loader = instance();
        auto it = loader.backends_.find(std::string(name));
        
        if (it == loader.backends_.end()) {
            throw std::runtime_error(std::format("UI backend '{}' not found", name));
        }
        
        return it->second->create();
    }

    // Load UI backend from dynamic library
    static std::unique_ptr<IUIBackend> load_from_lib(std::string_view lib_path) {
        // Note: Full dynamic loading would require dlopen/LoadLibrary
        // For now, we provide the interface for future implementation
        throw std::runtime_error("Dynamic library loading not yet implemented");
    }

    // Get list of available backends
    static std::vector<std::string> available_backends() {
        auto& loader = instance();
        std::vector<std::string> names;
        
        for (const auto& [name, factory] : loader.backends_) {
            names.push_back(name);
        }
        
        return names;
    }

    // Check if a backend is available
    static bool has_backend(std::string_view name) {
        auto& loader = instance();
        return loader.backends_.find(std::string(name)) != loader.backends_.end();
    }

private:
    static UILoader& instance() {
        static UILoader loader;
        return loader;
    }

    std::map<std::string, std::unique_ptr<IUIBackendFactory>> backends_;
};

} // namespace d2x
