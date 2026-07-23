module;

#include <cstdio>
#include <cstdlib>
#if defined(__linux__)
#include <sys/wait.h>
#endif

export module d2x.platform:linux;
import std;

#if defined(__linux__)


namespace d2x {
namespace platform_impl {

    export constexpr std::string_view XLINGS_INSTALL_CMD = "curl -fsSL https://d2learn.org/xlings-install.sh | bash";

    export std::pair<int, std::string> run_command_capture(const std::string& cmd) {
        std::string full = cmd + " 2>&1"; // redirect stderr to stdout
        FILE* pipe = ::popen(full.c_str(), "r");
        if (!pipe) {
            std::println("Failed to open pipe for command: {}", cmd);
            return {-1, std::string{}};
        }
        std::string output;
        std::array<char, 256> buffer{};
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            output += buffer.data();
        }
        int status = ::pclose(pipe);

        return {status, output};
    }

    // 流式逐行读。Provider 协议是 NDJSON 事件流，必须边读边处理：
    // 学员在等编译结果，读到 EOF 才显示等于全程黑屏。
    // 返回真实退出码（pclose 给的是 wait status，exit 1 会变成 256）。
    export int run_command_lines(const std::string& cmd,
                                 const std::function<void(std::string_view)>& on_line) {
        std::string full = cmd + " 2>&1";
        FILE* pipe = ::popen(full.c_str(), "r");
        if (!pipe) return -1;

        std::string line;
        std::array<char, 4096> buffer{};
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            line += buffer.data();
            if (line.ends_with('\n')) {
                line.pop_back();
                if (line.ends_with('\r')) line.pop_back();
                on_line(line);
                line.clear();
            }
        }
        if (!line.empty()) on_line(line);   // 最后一行可能没有换行符

        int status = ::pclose(pipe);
        if (status == -1) return 127;
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
        return status;
    }

    export void clear_console() {
        std::system("clear");
    }

    export std::string get_home_dir() {
        if (const char* home = std::getenv("HOME")) return home;
        return ".";
    }

    export inline std::string get_xlings_bin() {
        return get_home_dir() + "/.xlings/subos/current/bin/xlings";
    }

    export void set_env_variable(const std::string& key, const std::string& value) {
        ::setenv(key.c_str(), value.c_str(), 1);
    }

    // println implementation forwarding to std::println for Linux
    export template<typename... Args>
    void println(std::format_string<Args...> fmt, Args&&... args) {
        std::println(fmt, std::forward<Args>(args)...);
    }

    export inline void println(const std::string& msg) {
        std::println("{}", msg);
    }

} // namespace platform_impl
}

#endif // defined(__linux__)
