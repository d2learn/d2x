// 学习会话：遍历、推进、完成状态。
//
// 这一层是纯逻辑——不碰构建工具、不碰终端、不碰文件监听。给它一个假
// Provider 和一份内存状态就能测完整流程。旧实现把这些逻辑埋在
// checker::run() 的 100 行循环里，一行都测不了。
export module d2x.session;

import std;

import d2x.domain;
import d2x.json;

namespace d2x::session {

using domain::Exercise;

// 完成状态持久化。
//
// 按 id 存，不按下标——重排或重命名练习不会毁掉学员进度。这是 rustlings
// 的经验：它的 .rustlings-state.txt 同样按名字存，而「每次重编所有练习」
// 的性能投诉正是靠这个缓存解决的，与换构建工具无关。
export class StateStore {
    std::filesystem::path      mPath;
    std::string                mCurrent;
    std::set<std::string>      mCompleted;

public:
    explicit StateStore(std::filesystem::path path) : mPath(std::move(path)) { load(); }

    void load() {
        std::ifstream in(mPath);
        if (!in) return;
        auto parsed = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.is_object()) return;

        mCurrent = parsed.value("current", "");
        if (parsed.contains("completed") && parsed["completed"].is_array()) {
            for (const auto& id : parsed["completed"])
                if (id.is_string()) mCompleted.insert(id.get<std::string>());
        }
    }

    void save() const {
        nlohmann::json out;
        out["current"]   = mCurrent;
        out["completed"] = std::vector<std::string>(mCompleted.begin(), mCompleted.end());

        std::error_code ec;
        std::filesystem::create_directories(mPath.parent_path(), ec);
        std::ofstream file(mPath);
        if (file) file << out.dump(2) << '\n';
    }

    bool is_completed(const std::string& id) const { return mCompleted.contains(id); }
    const std::string& current() const { return mCurrent; }

    void mark_completed(const std::string& id) { mCompleted.insert(id); save(); }
    void set_current(const std::string& id)    { mCurrent = id; save(); }

    // 学员改坏了已完成的练习时用得上
    void unmark(const std::string& id) { mCompleted.erase(id); save(); }
};

// 会话推进逻辑。纯函数式的部分抽在这里，便于单测。
export class Session {
    std::vector<Exercise> mExercises;
    StateStore*           mState;
    std::size_t           mIndex{0};

public:
    Session(std::vector<Exercise> exercises, StateStore* state)
        : mExercises(std::move(exercises)), mState(state) {}

    bool empty() const { return mExercises.empty(); }
    std::size_t total() const { return mExercises.size(); }

    std::size_t completed_count() const {
        if (!mState) return 0;
        return static_cast<std::size_t>(std::ranges::count_if(
            mExercises, [&](const Exercise& e) { return mState->is_completed(e.id); }));
    }

    // 定位起点。优先级：显式指定 > 持久化的 current > 第一个未完成 > 开头。
    // 匹配用子串，方便学员只敲 `d2x checker rvalue`。
    void seek_start(std::string_view wanted) {
        if (!wanted.empty()) {
            auto it = std::ranges::find_if(mExercises, [&](const Exercise& e) {
                return e.id.find(wanted) != std::string::npos;
            });
            if (it != mExercises.end()) {
                mIndex = static_cast<std::size_t>(std::distance(mExercises.begin(), it));
                return;
            }
        }

        if (mState && !mState->current().empty()) {
            auto it = std::ranges::find(mExercises, mState->current(), &Exercise::id);
            if (it != mExercises.end()) {
                mIndex = static_cast<std::size_t>(std::distance(mExercises.begin(), it));
                return;
            }
        }

        auto it = std::ranges::find_if(mExercises, [&](const Exercise& e) {
            return !mState || !mState->is_completed(e.id);
        });
        mIndex = (it != mExercises.end())
               ? static_cast<std::size_t>(std::distance(mExercises.begin(), it))
               : 0;
    }

    bool done() const { return mIndex >= mExercises.size(); }

    const Exercise& current() const { return mExercises[mIndex]; }

    void enter_current() { if (mState && !done()) mState->set_current(current().id); }

    void complete_current() {
        if (done()) return;
        if (mState) mState->mark_completed(current().id);
        ++mIndex;
        enter_current();
    }
};

} // namespace d2x::session
