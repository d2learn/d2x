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
// SIGKILL 进程组并返回 idle_killed=true。固定总时长会误杀 Provider 的
// 冷启动构建(可达分钟级),活性模型只要求「持续有产出」。
//
// Windows:暂无安全的按句柄终止路径,回退为无超时运行(与 mcpp 的
// --timeout 同样的 documented best-effort 语义)。
export RunStatus run_lines_idle(const std::string& cmd,
                                std::chrono::milliseconds idle,
                                const std::function<void(std::string_view)>& on_line) {
    if (idle.count() <= 0) return run_lines(cmd, on_line);
#ifdef _WIN32
    return run_lines(cmd, on_line);
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
