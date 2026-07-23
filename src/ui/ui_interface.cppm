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
    // 词汇与领域层一致:exercise,不是 target(旧字段名把构建工具词汇
    // 固化进了接口,页面因此一直打 "Target:")。
    struct UIState : IUIState {
        std::string exercise;                   // 练习 id
        std::string chapter;
        std::vector<std::string> files;
        int completed = 0;
        int total = 0;
        std::string outcome;                    // "" 检测中 | pass | fail | blocked
        std::vector<std::string> checks;        // 结构化诊断行(file:line message),置顶展示
        std::string output;                     // 原始输出(页面自行截断)
        std::string output_log_path;            // 全量输出文件(截断提示引用)
        std::string hint;                       // AI 提示
        bool only_update_hint = false;
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
