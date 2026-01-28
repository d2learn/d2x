module;

#include <cstdio>

export module d2x.platform.windows;

#if defined(_WIN32)

import std;

namespace d2x {
namespace platform_impl {
    export std::pair<int, std::string> run_command_capture(const std::string& cmd) {
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) {
            return {-1, std::string{}};
        }
        std::string output;
        std::array<char, 256> buffer{};
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            output += buffer.data();
        }
        int code = _pclose(pipe);
        return {code, output};
    }

    export void clear_console() {
        // run by cmd
        std::system("cls");
    }

    export std::string get_home_dir() {
        if (const char* home = std::getenv("USERPROFILE")) return home;
        if (const char* appdata = std::getenv("APPDATA")) return appdata;
        return ".";
    }

    export bool xlings_install() {
        std::println("正在安装 xlings...");
        int status = std::system("powershell -Command \"irm https://d2learn.org/xlings-install.ps1.txt | iex\"");
        if (status == 0) {
            std::println("xlings 安装成功！");
            return true;
        }
        std::println("xlings 安装失败");
        return false;
    }
} // namespace platform_impl
} // namespace d2x

#endif // defined(_WIN32)