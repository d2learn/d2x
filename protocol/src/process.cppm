// Child-process transport. stdout is protocol input; stderr is diagnostics.
module;
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#ifndef _WIN32
# include <sys/wait.h>
# include <unistd.h>
# include <fcntl.h>
# include <signal.h>
#else
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <windows.h>
#endif
export module d2x.protocol.process;
import std;

namespace d2x::protocol::process {
export struct RunStatus {
    int exit_code = 0;
    bool idle_killed = false;
    bool deadline_killed = false;
    bool cancelled = false;
};

#ifdef _WIN32
volatile LONG interrupted = 0;
BOOL WINAPI interrupt_handler(DWORD event) {
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
    ::InterlockedExchange(&interrupted, 1);
    return TRUE;
}
#else
volatile sig_atomic_t interrupted = 0;
void interrupt_handler(int) { interrupted = 1; }
int normalize(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
}
#endif

// The absolute deadline is independent of bytes received and host UI ticks.
// Drain only a bounded batch per poll so endless output cannot starve deadlines.
export RunStatus run_streams(const std::string& cmd,
    std::chrono::milliseconds idle, std::chrono::milliseconds deadline,
    const std::function<void(std::string_view)>& on_line,
    const std::function<void(std::string_view, bool)>& on_bytes = {},
    const std::function<void(std::chrono::seconds)>& on_tick = {}) {
    auto started = std::chrono::steady_clock::now();
    auto last_output = started;
    auto next_tick = started;
    std::string line;
    bool oversized = false;
    RunStatus result;
    auto consume = [&](std::string_view bytes, bool stderr_stream) {
        if (on_bytes) on_bytes(bytes, stderr_stream);
        if (stderr_stream) return;
        for (char c : bytes) {
            if (c == '\n') {
                if (!oversized) {
                    if (line.ends_with('\r')) line.pop_back();
                    on_line(line);
                }
                line.clear();
                oversized = false;
            } else if (line.size() < 1024 * 1024) line += c;
            else oversized = true;
        }
    };
    auto should_stop = [&] {
        auto now = std::chrono::steady_clock::now();
        if (on_tick && now >= next_tick) {
            on_tick(std::chrono::duration_cast<std::chrono::seconds>(now - started));
            next_tick = now + std::chrono::seconds(10);
        }
        result.cancelled = interrupted != 0;
        result.deadline_killed = deadline.count() > 0 && now - started >= deadline;
        result.idle_killed = idle.count() > 0 && now - last_output >= idle;
        return result.cancelled || result.deadline_killed || result.idle_killed;
    };
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE rd[2]{}, wr[2]{};
    auto close_pipes = [&] {
        for (int i = 0; i < 2; ++i) {
            if (rd[i]) ::CloseHandle(rd[i]);
            if (wr[i]) ::CloseHandle(wr[i]);
        }
    };
    for (int i = 0; i < 2; ++i) {
        if (!::CreatePipe(&rd[i], &wr[i], &sa, 0)) {
            close_pipes(); return {127};
        }
        ::SetHandleInformation(rd[i], HANDLE_FLAG_INHERIT, 0);
    }
    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        if (job) ::CloseHandle(job);
        close_pipes(); return {127};
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr[0]; si.hStdError = wr[1];
    si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    std::string full = "cmd.exe /D /S /C \"" + cmd + "\"";
    std::vector<char> command(full.begin(), full.end()); command.push_back('\0');
    PROCESS_INFORMATION pi{};
    BOOL ok = ::CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    for (auto& w : wr) { ::CloseHandle(w); w = nullptr; }
    if (!ok) { ::CloseHandle(job); close_pipes(); return {127}; }
    // Never run an uncontained child: assignment failure must fail closed.
    if (!::AssignProcessToJobObject(job, pi.hProcess)) {
        ::TerminateProcess(pi.hProcess, 127);
        ::WaitForSingleObject(pi.hProcess, INFINITE);
        ::CloseHandle(pi.hThread); ::CloseHandle(pi.hProcess);
        ::CloseHandle(job); close_pipes(); return {127};
    }
    ::InterlockedExchange(&interrupted, 0);
    ::SetConsoleCtrlHandler(interrupt_handler, TRUE);
    ::ResumeThread(pi.hThread); ::CloseHandle(pi.hThread);
    auto drain = [&](int channel) {
        std::array<char, 4096> buf{};
        for (int batch = 0; batch < 16; ++batch) {
            DWORD available = 0, n = 0;
            if (!::PeekNamedPipe(rd[channel], nullptr, 0, nullptr, &available, nullptr) || !available) break;
            if (!::ReadFile(rd[channel], buf.data(), std::min<DWORD>(available, buf.size()), &n, nullptr) || !n) break;
            last_output = std::chrono::steady_clock::now();
            consume(std::string_view(buf.data(), n), channel == 1);
        }
    };
    for (;;) {
        drain(0); drain(1);
        // Check deadlines even when the shell has just exited.
        if (should_stop()) {
            ::TerminateJobObject(job, 1);
            ::WaitForSingleObject(pi.hProcess, INFINITE);
            break;
        }
        if (::WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) break;
        ::Sleep(50);
    }
    // Close the job to stop descendants retaining pipes after the shell exits.
    ::TerminateJobObject(job, 1);
    drain(0); drain(1);
    DWORD code = 127; ::GetExitCodeProcess(pi.hProcess, &code);
    result.exit_code = static_cast<int>(code);
    ::CloseHandle(pi.hProcess); ::CloseHandle(job); close_pipes();
    ::SetConsoleCtrlHandler(interrupt_handler, FALSE);
#else
    int fds[2][2]{{-1, -1}, {-1, -1}};
    auto close_pipes = [&] {
        for (auto& pair : fds) for (int fd : pair) if (fd >= 0) ::close(fd);
    };
    if (::pipe(fds[0]) || ::pipe(fds[1])) { close_pipes(); return {127}; }
    struct sigaction action{}, old_int{}, old_term{};
    action.sa_handler = interrupt_handler;
    sigemptyset(&action.sa_mask);
    interrupted = 0;
    ::sigaction(SIGINT, &action, &old_int); ::sigaction(SIGTERM, &action, &old_term);
    pid_t pid = ::fork();
    if (pid == 0) {
        ::setpgid(0, 0);
        ::sigaction(SIGINT, &old_int, nullptr); ::sigaction(SIGTERM, &old_term, nullptr);
        ::dup2(fds[0][1], STDOUT_FILENO); ::dup2(fds[1][1], STDERR_FILENO);
        close_pipes();
        ::unsetenv("LD_LIBRARY_PATH");
        ::execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
        ::_exit(127);
    }
    if (pid < 0) {
        close_pipes();
        ::sigaction(SIGINT, &old_int, nullptr); ::sigaction(SIGTERM, &old_term, nullptr);
        return {127};
    }
    ::setpgid(pid, pid); // close the parent/child process-group race
    for (auto& pair : fds) {
        ::close(pair[1]); pair[1] = -1;
        ::fcntl(pair[0], F_SETFL, ::fcntl(pair[0], F_GETFL) | O_NONBLOCK);
    }
    auto drain = [&](int channel) {
        std::array<char, 4096> buf{};
        for (int batch = 0; batch < 16; ++batch) {
            auto n = ::read(fds[channel][0], buf.data(), buf.size());
            if (n <= 0) break;
            last_output = std::chrono::steady_clock::now();
            consume(std::string_view(buf.data(), static_cast<std::size_t>(n)), channel == 1);
        }
    };
    int status = 0;
    for (;;) {
        drain(0); drain(1);
        if (should_stop()) {
            ::kill(-pid, SIGKILL);
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            break;
        }
        auto waited = ::waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0 && errno != EINTR) { status = -1; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ::kill(-pid, SIGKILL);
    drain(0); drain(1); close_pipes();
    ::sigaction(SIGINT, &old_int, nullptr); ::sigaction(SIGTERM, &old_term, nullptr);
    result.exit_code = status == -1 ? 127 : normalize(status);
#endif
    if (!line.empty() && !oversized) on_line(line);
    if (result.cancelled) result.exit_code = 130;
    else if (result.deadline_killed || result.idle_killed) result.exit_code = 124;
    return result;
}

export RunStatus run_lines_idle(const std::string& cmd, std::chrono::milliseconds idle,
    const std::function<void(std::string_view)>& on_line) {
    return run_streams(cmd, idle, {}, on_line);
}
export RunStatus run_lines(const std::string& cmd,
    const std::function<void(std::string_view)>& on_line) {
    return run_streams(cmd, {}, {}, on_line);
}
} // namespace d2x::protocol::process
