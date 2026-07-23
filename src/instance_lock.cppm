// 单实例锁:同一仓库同时只允许一个 checker。
//
// 依据(2026-07-24 设计文档 S2):一个泄漏的 checker 实例在下游 e2e 覆盖
// 答案时并发触发重建,与外部构建竞争产物目录,测试二进制被替换瞬间报
// exit 127——两个监听循环互踩的破坏是静默且难归因的。
//
// 机制:.d2x/checker.lock 写入本进程 pid。启动时若锁存在:
//   pid 仍存活 → 拒绝启动(明确报出对方 pid);
//   pid 已死   → 陈旧锁(上一个实例被 kill -9/断电),自动接管。
// 正常退出走 RAII;SIGINT/SIGTERM 走信号处理器删锁后退出。
module;

#include <csignal>
#include <cstdlib>
#ifndef _WIN32
#  include <unistd.h>
#  include <signal.h>
#else
#  include <windows.h>
#endif

export module d2x.instance_lock;

import std;

namespace d2x::instance_lock {

namespace {
    // 信号处理器只能访问无锁的全局状态;路径在 acquire 时固化。
    std::filesystem::path g_lock_path;

    bool pid_alive(long pid) {
#ifndef _WIN32
        return ::kill(static_cast<pid_t>(pid), 0) == 0;
#else
        HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 static_cast<DWORD>(pid));
        if (!h) return false;
        DWORD code = 0;
        bool alive = ::GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
        ::CloseHandle(h);
        return alive;
#endif
    }

    extern "C" void on_signal(int sig) {
        std::error_code ec;
        if (!g_lock_path.empty()) std::filesystem::remove(g_lock_path, ec);
        std::_Exit(128 + sig);
    }
}

// 尝试获取锁。失败时经 on_conflict 报出持有者 pid,返回 false。
export bool acquire(const std::filesystem::path& lock_path,
                    const std::function<void(long /*holder_pid*/)>& on_conflict) {
    std::error_code ec;
    std::filesystem::create_directories(lock_path.parent_path(), ec);

    if (std::filesystem::exists(lock_path)) {
        long holder = 0;
        {
            std::ifstream in(lock_path);
            in >> holder;
        }
        if (holder > 0 && pid_alive(holder)) {
            if (on_conflict) on_conflict(holder);
            return false;
        }
        // 陈旧锁:持有者已死,接管。
        std::filesystem::remove(lock_path, ec);
    }

    {
        std::ofstream out(lock_path, std::ios::trunc);
#ifndef _WIN32
        out << ::getpid() << '\n';
#else
        out << ::GetCurrentProcessId() << '\n';
#endif
    }
    g_lock_path = lock_path;

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);
    return true;
}

// 正常退出路径的释放(信号路径由 on_signal 兜底)。
export void release() {
    std::error_code ec;
    if (!g_lock_path.empty()) {
        std::filesystem::remove(g_lock_path, ec);
        g_lock_path.clear();
    }
}

} // namespace d2x::instance_lock
