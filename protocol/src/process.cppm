// d2x.protocol.process — 协议传输的进程运行原语。
//
// 协议层自带最小的「逐行读子进程输出」能力,不依赖 d2x core 的 platform
// 模块——依赖方向必须是 core → protocol,不能反过来。
module;

#include <cstdio>
#include <cstdlib>
#ifndef _WIN32
#  include <sys/wait.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <poll.h>
#  include <signal.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX            // 否则 windows.h 的 min/max 宏会撞上标准库
#  include <windows.h>
#endif

export module d2x.protocol.process;

import std;

namespace d2x::protocol::process {

// 运行结果:exit_code 为平台归一化退出码(信号终止 → 128+sig);
// idle_killed 表示因活性超时被终止。
export struct RunStatus {
    int  exit_code   = 0;
    bool idle_killed = false;
};

#ifndef _WIN32
int normalize(int status) {
    if (status == -1)        return 127;
    if (WIFEXITED(status))   return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return status;
}
#endif

// 逐行运行(无超时)。stderr 并入 stdout;行尾 \n/\r\n 归一剥除;
// 结尾不带换行的最后一行同样回调。
//
// spawn 前清空 LD_LIBRARY_PATH:Provider 链路曾因继承的私有 glibc loader
// 路径在嵌套进程里段错误(mcpp >= 0.0.104 已在其侧根治,这里保留是因为
// 该保护对任意 Provider 启动器成立,不只 mcpp)。
export RunStatus run_lines(const std::string& cmd,
                           const std::function<void(std::string_view)>& on_line) {
#ifndef _WIN32
    ::setenv("LD_LIBRARY_PATH", "", 1);
#endif
    std::string full = cmd + " 2>&1";
#ifdef _WIN32
    FILE* pipe = ::_popen(full.c_str(), "r");
#else
    FILE* pipe = ::popen(full.c_str(), "r");
#endif
    if (!pipe) return {127, false};

    std::string line;
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        line += buffer.data();
        if (line.ends_with('\n')) {
            line.pop_back();
            if (line.ends_with('\r')) line.pop_back();
            on_line(line);
            line.clear();
        }
    }
    if (!line.empty()) on_line(line);

#ifdef _WIN32
    int status = ::_pclose(pipe);
    return {status == -1 ? 127 : status, false};
#else
    return {normalize(::pclose(pipe)), false};
#endif
}

// 逐行运行 + 活性超时:自上次收到任何输出起超过 idle 时长即判定挂死,
// 终止整棵进程树并返回 idle_killed=true。固定总时长会误杀 Provider 的
// 冷启动构建(可达分钟级),活性模型只要求「持续有产出」。
//
// 两个平台的做法不同但语义一致 ——「杀掉整棵进程树,而不只是直接子进程」:
// POSIX 用独立进程组 + kill(-pid),Windows 用 Job Object。Provider 往往还有
// 孙进程(mcpp → 编译器),只杀直接子进程会留下孤儿继续占用产物目录。
export RunStatus run_lines_idle(const std::string& cmd,
                                std::chrono::milliseconds idle,
                                const std::function<void(std::string_view)>& on_line) {
    if (idle.count() <= 0) return run_lines(cmd, on_line);
#ifdef _WIN32
    // 不用 _popen:它拿不到进程句柄,超时了无从终止。改为 CreateProcess 起
    // cmd.exe,并把进程纳入 Job Object —— Job Object 是 Windows 上终止整棵
    // 进程树的正规手段,对应 POSIX 分支的 kill(-pid)。
    //
    // 命令行走 ANSI 版 API,与本文件 run_lines 里的 _popen 保持一致(两者都
    // 按当前代码页解释);统一改 UTF-16 是另一件事,不在这里顺手做。
    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE rd = nullptr, wr = nullptr;
    if (!::CreatePipe(&rd, &wr, &sa, 0)) return {127, false};
    // 读端不让子进程继承:否则管道多出一个写者,子进程退出也读不到 EOF。
    ::SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = wr;          // stderr 并入 stdout,与 run_lines 的 2>&1 一致
    si.hStdInput  = ::GetStdHandle(STD_INPUT_HANDLE);

    // CreateProcess 会就地改写命令行缓冲区,必须传可写副本。
    std::string full = "cmd.exe /C " + cmd;
    std::vector<char> cmdline(full.begin(), full.end());
    cmdline.push_back('\0');

    PROCESS_INFORMATION pi{};
    // CREATE_SUSPENDED:先挂起,纳入 Job 之后再放行 —— 否则子进程可能在被
    // 纳管之前就派生出逃逸在 Job 之外的孙进程。
    BOOL ok = ::CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                               CREATE_SUSPENDED | CREATE_NO_WINDOW,
                               nullptr, nullptr, &si, &pi);
    ::CloseHandle(wr);           // 父进程手里这份写端必须关,否则永远读不到 EOF
    if (!ok) { ::CloseHandle(rd); return {127, false}; }

    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jl{};
        jl.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jl, sizeof(jl));
        if (!::AssignProcessToJobObject(job, pi.hProcess)) {
            // 纳管失败(例如已处在不可嵌套的 Job 里):退化成只杀直接子进程。
            // 比完全不超时好 —— 至少 d2x 自己不会跟着挂死。
            ::CloseHandle(job);
            job = nullptr;
        }
    }
    ::ResumeThread(pi.hThread);
    ::CloseHandle(pi.hThread);

    std::string line;
    std::array<char, 4096> buffer{};
    auto last_output = std::chrono::steady_clock::now();
    bool killed    = false;
    bool escalated = false;
    std::chrono::steady_clock::time_point kill_at{};

    auto emit = [&](DWORD n) {
        for (DWORD i = 0; i < n; ++i) {
            char c = buffer[static_cast<std::size_t>(i)];
            if (c == '\n') {
                if (line.ends_with('\r')) line.pop_back();
                on_line(line);
                line.clear();
            } else {
                line += c;
            }
        }
    };

    // 全程 PeekNamedPipe 探量再读,绝不裸调 ReadFile —— 管道读是阻塞的,
    // 挂死的 Provider 会把这里一起拖住,活性超时就永远轮不到判定。
    auto drain = [&]() -> bool {   // 返回是否读到了任何数据
        bool got = false;
        for (;;) {
            DWORD avail = 0;
            if (!::PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr)) break;
            if (avail == 0) break;
            DWORD want = avail < buffer.size() ? avail : static_cast<DWORD>(buffer.size());
            DWORD n = 0;
            if (!::ReadFile(rd, buffer.data(), want, &n, nullptr) || n == 0) break;
            got = true;
            emit(n);
        }
        return got;
    };

    for (;;) {
        if (drain()) last_output = std::chrono::steady_clock::now();

        if (::WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
            drain();                       // 收尾:进程已退,管道里可能还有残留
            if (!line.empty()) on_line(line);
            DWORD code = 0;
            ::GetExitCodeProcess(pi.hProcess, &code);
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(rd);
            if (job) ::CloseHandle(job);
            return {static_cast<int>(code), killed};
        }

        auto now = std::chrono::steady_clock::now();

        if (!killed && now - last_output > idle) {
            if (job) ::TerminateJobObject(job, 1);      // 整棵进程树
            else     ::TerminateProcess(pi.hProcess, 1);
            killed = true;
            kill_at = now;
        }

        // 终止之后进程仍不退出,就不能一直等下去 —— 这个循环存在的意义就是
        // 防止挂死,它自己挂死是最糟的结局。分两级兜底:先补一次直杀(Job
        // 路径被拒时还有救),再到点就放弃等待、如实返回 idle_killed,由上层
        // 报「Provider 被终止」。退出码沿用 POSIX 分支被 KILL 时的 128+9。
        if (killed) {
            auto since_kill = now - kill_at;
            if (!escalated && since_kill > std::chrono::seconds(5)) {
                ::TerminateProcess(pi.hProcess, 1);
                escalated = true;
            }
            if (since_kill > std::chrono::seconds(15)) {
                ::CloseHandle(pi.hProcess);
                ::CloseHandle(rd);
                if (job) ::CloseHandle(job);   // KILL_ON_JOB_CLOSE 再补一刀
                return {137, true};
            }
        }
        ::Sleep(50);
    }
#else
    ::setenv("LD_LIBRARY_PATH", "", 1);
    // popen 无法拿到 pid,这里手工 fork + exec sh -c,子进程自成进程组,
    // 超时可以杀掉整组(Provider 往往还有孙进程,只杀直接子进程会留孤儿)。
    int fds[2];
    if (::pipe(fds) != 0) return {127, false};

    pid_t pid = ::fork();
    if (pid < 0) { ::close(fds[0]); ::close(fds[1]); return {127, false}; }
    if (pid == 0) {
        ::setpgid(0, 0);
        ::dup2(fds[1], 1);
        ::dup2(fds[1], 2);
        ::close(fds[0]);
        ::close(fds[1]);
        ::execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
        ::_exit(127);
    }
    ::close(fds[1]);
    ::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL) | O_NONBLOCK);

    std::string line;
    std::array<char, 4096> buffer{};
    auto last_output = std::chrono::steady_clock::now();
    bool killed = false;

    auto drain = [&]() -> bool {   // 返回是否读到了任何数据
        bool got = false;
        for (;;) {
            ssize_t n = ::read(fds[0], buffer.data(), buffer.size());
            if (n <= 0) break;
            got = true;
            for (ssize_t i = 0; i < n; ++i) {
                char c = buffer[static_cast<std::size_t>(i)];
                if (c == '\n') {
                    if (line.ends_with('\r')) line.pop_back();
                    on_line(line);
                    line.clear();
                } else {
                    line += c;
                }
            }
        }
        return got;
    };

    int status = 0;
    for (;;) {
        struct pollfd pfd{fds[0], POLLIN, 0};
        ::poll(&pfd, 1, 200);
        if (drain()) last_output = std::chrono::steady_clock::now();

        pid_t r = ::waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            drain();
            if (!line.empty()) on_line(line);
            ::close(fds[0]);
            return {normalize(status), killed};
        }
        if (!killed && std::chrono::steady_clock::now() - last_output > idle) {
            ::kill(-pid, SIGKILL);   // 整个进程组
            killed = true;
        }
    }
#endif
}

} // namespace d2x::protocol::process
