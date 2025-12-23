export module d2x.ui.interface;

import std;

namespace d2x {

// Common state for console/checker UI
export struct ConsoleState {
    std::string target;
    std::vector<std::string> target_files;
    int built_targets = 0;
    int total_targets = 0;
    std::string output;
    bool status = false;
};

// Abstract UI backend interface - plugins implement this
export class IUIBackend {
public:
    virtual ~IUIBackend() = default;
    
    // Start the UI (may spawn threads, open windows, etc.)
    virtual void start() = 0;
    
    // Stop the UI and cleanup resources
    virtual void stop() = 0;
    
    // Update the UI state (thread-safe)
    virtual void update(ConsoleState state) = 0;
};

// Factory interface for creating UI backends
export class IUIBackendFactory {
public:
    virtual ~IUIBackendFactory() = default;
    
    // Create a new UI backend instance
    virtual std::unique_ptr<IUIBackend> create() = 0;
    
    // Get the backend name/identifier
    virtual std::string_view name() const = 0;
};

} // namespace d2x
