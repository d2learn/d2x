module;

#include <cstdio>
#include <cstdlib>

export module d2x.platform:windows;

#if defined(_WIN32)

import std;

namespace d2x {
namespace platform_impl {

    export constexpr std::string_view XLINGS_BIN = "C:\\Users\\Public\\xlings\\.xlings_data\\bin\\xlings";

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
            // add xlings to PATH C:\Users\Public\xlings\.xlings_data\bin
            std::string xlings_path = "C:\\Users\\Public\\xlings\\.xlings_data\\bin";
            char* path_env = std::getenv("PATH");
            if (path_env) {
                std::string new_path = std::string(path_env) + ";" + xlings_path;
                _putenv_s("PATH", new_path.c_str());
            }
            return true;
        }
        std::println("xlings 安装失败");
        return false;
    }

    export void set_env_variable(const std::string& key, const std::string& value) {
        _putenv_s(key.c_str(), value.c_str());
    }
} // namespace platform_impl
} // namespace d2x

#endif // defined(_WIN32)