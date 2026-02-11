module;

#include <cstdio>
#include <cstdlib>

export module d2x.platform:macos;

#if defined(__APPLE__)

import std;

namespace d2x {
namespace platform_impl {

    export constexpr std::string_view XLINGS_BIN = "/Users/xlings/.xlings_data/bin/xlings";
    export constexpr std::string_view XLINGS_INSTALL_CMD = "curl -fsSL https://d2learn.org/xlings-install.sh | bash";

    export std::pair<int, std::string> run_command_capture(const std::string& cmd) {
        std::string full = cmd + " 2>&1"; // redirect stderr to stdout
        FILE* pipe = ::popen(full.c_str(), "r");
        if (!pipe) {
            std::cout << std::format("Failed to open pipe for command: {}\n", cmd);
            std::cout.flush();
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

    export void clear_console() {
        std::system("clear");
    }

    export std::string get_home_dir() {
        if (const char* home = std::getenv("HOME")) return home;
        return ".";
    }

    export void set_env_variable(const std::string& key, const std::string& value) {
        ::setenv(key.c_str(), value.c_str(), 1);
    }

    // println implementation using std::cout for macOS
    export template<typename... Args>
    void println(std::format_string<Args...> fmt, Args&&... args) {
        std::cout << std::format(fmt, std::forward<Args>(args)...) << '\n';
        std::cout.flush();
    }

    export inline void println(const std::string& msg) {
        std::cout << msg << '\n';
        std::cout.flush();
    }

} // namespace platform_impl
}

#endif // defined(__APPLE__)
