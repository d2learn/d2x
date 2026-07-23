// 学习会话：遍历、推进、完成状态。
//
// 这一层是纯逻辑——不碰构建工具、不碰终端、不碰文件监听。给它一个假
// Provider 和一份内存状态就能测完整流程。旧实现把这些逻辑埋在
// checker::run() 的 100 行循环里，一行都测不了。
module;

// stderr 是宏,import std 不提供
#include <cstdio>

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
        if (parsed.is_discarded() || !parsed.is_object()) {
            // 损坏的进度文件不能静默清零(学习者会无感知地丢档):改名备份、
            // 明确告知,再从空白继续。备份带时间戳,反复损坏也不互相覆盖。
            in.close();
            auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            auto backup = mPath;
            backup += std::format(".corrupt-{}", ts);
            std::error_code ec;
            std::filesystem::rename(mPath, backup, ec);
            std::println(stderr,
                "warning: 进度文件损坏,已备份为 {} 并从空白进度继续",
                backup.string());
            return;
        }

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
        // 平铺循环,同 seek_start 的 ICE 规避说明
        std::size_t n = 0;
        for (const auto& e : mExercises)
            if (mState->is_completed(e.id)) ++n;
        return n;
    }

    // 定位起点。优先级：显式指定 > 持久化的 current > 第一个未完成 > 开头。
    // 匹配用子串，方便学员只敲 `d2x checker rvalue`。
    // 平铺循环而非 ranges 投影:clang 20(MSVC 目标)对跨模块类型
    // (Exercise 现居 d2x.protocol.types)的 ranges 投影在 PCM 代码生成
    // 阶段 ICE(Windows CI 实测,Stack dump 指向本函数)。行为等价。
    void seek_start(std::string_view wanted) {
        if (!wanted.empty()) {
            for (std::size_t i = 0; i < mExercises.size(); ++i) {
                if (mExercises[i].id.find(wanted) != std::string::npos) {
                    mIndex = i;
                    return;
                }
            }
        }

        if (mState && !mState->current().empty()) {
            for (std::size_t i = 0; i < mExercises.size(); ++i) {
                if (mExercises[i].id == mState->current()) {
                    mIndex = i;
                    return;
                }
            }
        }

        for (std::size_t i = 0; i < mExercises.size(); ++i) {
            if (!mState || !mState->is_completed(mExercises[i].id)) {
                mIndex = i;
                return;
            }
        }
        mIndex = 0;
    }

    bool done() const { return mIndex >= mExercises.size(); }

    const Exercise& current() const { return mExercises[mIndex]; }

    void enter_current() { if (mState && !done()) mState->set_current(current().id); }

    void complete_current() {
        if (done()) return;
        if (mState) mState->mark_completed(current().id);
        advance_to_next_incomplete();
        enter_current();
    }

private:
    bool incomplete(const Exercise& e) const {
        return !mState || !mState->is_completed(e.id);
    }

    // 推进到下一道未完成的练习：先向后找，找不到再从头绕一圈。
    //
    // 为什么要绕回去：起点优先级是「持久化 current > 第一个未完成」，
    // 这是对的 —— 学员主动跳级后重启不该被硬拉回开头。但副作用是，
    // 课程作者在学员当前位置之前插入新练习时，那道题会被静默跳过。
    // 绕一圈保证「学员不会被打断，也不会丢内容」。
    //
    // 向后找时跳过已完成的：学员重玩时不必把做过的题再验一遍，
    // 这与 seek_start 的行为一致。
    void advance_to_next_incomplete() {
        for (std::size_t i = mIndex + 1; i < mExercises.size(); ++i) {
            if (incomplete(mExercises[i])) { mIndex = i; return; }
        }
        for (std::size_t i = 0; i <= mIndex && i < mExercises.size(); ++i) {
            if (incomplete(mExercises[i])) { mIndex = i; return; }
        }
        mIndex = mExercises.size();   // 全部完成
    }

public:
};

} // namespace d2x::session
