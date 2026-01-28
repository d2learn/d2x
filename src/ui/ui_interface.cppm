export module d2x.ui.interface;

import std;

namespace d2x {

export struct IUIState {
    virtual ~IUIState() = default;
};

// Page types for different UI views
export enum class PageID {
    Checker = 0,  // Default checker page
    Help,         // Help/usage page
};

export struct IPageUI {
    virtual ~IPageUI() = default;
};

export using Pages = std::map<PageID, std::unique_ptr<IPageUI>>;

// Abstract UI backend interface - plugins implement this
export class IUIBackend {
    Pages mPages;
public:
    virtual ~IUIBackend() = default;

    // Start the UI (may spawn threads, open windows, etc.)
    virtual void start() = 0;

    // Register a page
    template<typename PageType>
    void register_page(PageID pageId, std::unique_ptr<PageType> page) {
        mPages[pageId] = std::move(page);
    }

    // Get a page (for backend implementation to access specific pages)
    IPageUI* get_page(PageID pageId) {
        auto it = mPages.find(pageId);
        return it != mPages.end() ? it->second.get() : nullptr;
    }

    // Update a specific page
    template<typename PageUI, typename State>
    void update_page(PageID pageId, const State& state) {
        auto* page = get_page(pageId);
        if (!page) return;

        if (auto* checkerPage = dynamic_cast<PageUI*>(page)) {
            checkerPage->update(state);
        }
    }

    // Stop the UI and cleanup resources
    virtual void stop() = 0;
};

// Main UI interface with multi-page support
export class ICheckerPageUI : public IPageUI {
public:
    struct UIState : IUIState {
        std::string target;
        std::vector<std::string> target_files;
        int built_targets = 0;
        int total_targets = 0;
        std::string output;
        bool status = false;
        std::string ai_tips;
        bool only_update_ai_tips = false;
    };
public:
    virtual ~ICheckerPageUI() = default;

    // Update the UI state (thread-safe)
    virtual void update(const UIState& state) = 0;
};

export class IHelpPageUI : public IPageUI {
public:
    struct UIState : IUIState {
        std::string help;
    };
public:
    virtual ~IHelpPageUI() = default;

    virtual void update(const UIState& state) = 0;
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
