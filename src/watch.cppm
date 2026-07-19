// 文件监听：去抖 + 自触发保护。
//
// 替换掉原先「把所有文件 mtime 相加再比总和」的做法 —— 那有三个问题：
//
//   1. 求和会抵消。两个文件的 mtime 一增一减，总和不变，改动被漏掉。
//   2. 没有去抖。编辑器保存常常是多次写入（或写临时文件再 rename），
//      第一次写入就触发重建，可能读到半截文件。
//   3. 去抖逻辑手写在调用方（连续调两次 wait_files_changed），
//      语义藏在调用点，没法单独验证。
export module d2x.watch;

import std;

namespace d2x::watch {

// 每个文件单独记 mtime 与大小。大小是廉价的第二维度：某些文件系统
// mtime 粒度是秒级，一秒内的改动只靠 mtime 看不出来。
struct Stamp {
    std::int64_t mtime{};
    std::uintmax_t size{};
    bool exists{};

    bool operator==(const Stamp&) const = default;
};

Stamp stamp_of(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return {};

    Stamp s;
    s.exists = true;
    auto t = std::filesystem::last_write_time(p, ec);
    if (!ec) s.mtime = t.time_since_epoch().count();
    s.size = std::filesystem::file_size(p, ec);
    if (ec) s.size = 0;
    return s;
}

export class FileWatcher {
    std::vector<std::string>  mFiles;
    std::vector<Stamp>        mSnapshot;

    std::vector<Stamp> sample() const {
        std::vector<Stamp> out;
        out.reserve(mFiles.size());
        for (const auto& f : mFiles) out.push_back(stamp_of(f));
        return out;
    }

public:
    explicit FileWatcher(std::vector<std::string> files)
        : mFiles(std::move(files)) { resync(); }

    // 重新采样，把当前状态当作基线。
    //
    // 这是自触发保护：构建过程本身可能碰到被监听的文件（生成、格式化、
    // 或仅仅是被读取时更新 atime 的边缘情况）。在开始等待之前重新采样，
    // 就不会把自己造成的变化误当作学员的编辑。
    void resync() { mSnapshot = sample(); }

    // 等待学员改文件。
    //
    // 检测到第一处改动后不立即返回，而是进入「安静期」：持续采样直到
    // settle 时长内没有新变化，才认为这一轮编辑结束。这样编辑器的多次
    // 写入只会触发一次重建，也避免读到写了一半的文件。
    //
    // 返回 true 表示发生了改动，false 表示超时（学员没动）。
    bool wait_for_change(std::chrono::milliseconds timeout,
                         std::chrono::milliseconds settle = std::chrono::milliseconds{300},
                         std::chrono::milliseconds poll   = std::chrono::milliseconds{150}) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        // 阶段一：等第一处改动
        bool changed = false;
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(poll);
            auto now = sample();
            if (now != mSnapshot) {
                mSnapshot = std::move(now);
                changed = true;
                break;
            }
        }
        if (!changed) return false;   // 整段时间都没动静

        // 阶段二：安静期。有新变化就重新计时，直到 settle 内无变化。
        auto quiet_until = std::chrono::steady_clock::now() + settle;
        while (std::chrono::steady_clock::now() < quiet_until) {
            std::this_thread::sleep_for(poll);
            auto now = sample();
            if (now != mSnapshot) {
                mSnapshot = std::move(now);
                quiet_until = std::chrono::steady_clock::now() + settle;
            }
        }
        return true;
    }
};

} // namespace d2x::watch
