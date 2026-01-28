module;

#include <cstdio>

export module d2x.platform.linux;

#if defined(__linux__)

import std;

namespace d2x {
namespace platform_impl {
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

    export void clear_console() {
        std::system("clear");
    }

    export std::string get_home_dir() {
        if (const char* home = std::getenv("HOME")) return home;
        return ".";
    }
} // namespace platform_impl
}

#endif // defined(__linux__)
