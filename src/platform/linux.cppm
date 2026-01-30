module;

#include <cstdio>
#include <cstdlib>

export module d2x.platform:linux;

#if defined(__linux__)

import std;

namespace d2x {
namespace platform_impl {

    export constexpr std::string_view XLINGS_BIN = "/home/xlings/.xlings_data/bin/xlings";

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

    export bool xlings_install() {
        std::println("正在安装 xlings...");
        int status = std::system("curl -fsSL https://d2learn.org/xlings-install.sh | bash");
        if (status == 0) {
            std::println("xlings 安装成功！");
            // add xlings to PATH /home/xlings/.xlings_data/bin
            std::string xlings_path = "/home/xlings/.xlings_data/bin";
            char* path_env = std::getenv("PATH");
            if (path_env) {
                std::string new_path = std::string(path_env) + ":" + xlings_path;
                ::setenv("PATH", new_path.c_str(), 1);
            }
            return true;
        }
        std::println("xlings 安装失败");
        return false;
    }

    export void set_env_variable(const std::string& key, const std::string& value) {
        ::setenv(key.c_str(), value.c_str(), 1);
    }
} // namespace platform_impl
}

#endif // defined(__linux__)
