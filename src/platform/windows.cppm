module;

#include <cstdio>
#include <cstdlib>

export module d2x.platform:windows;
import std;

#if defined(_WIN32)


namespace d2x {
namespace platform_impl {

    export constexpr std::string_view XLINGS_INSTALL_CMD = "powershell -Command \"irm https://d2learn.org/xlings-install.ps1.txt | iex\"";

    export std::pair<int, std::string> run_command_capture(const std::string& cmd) {
        std::string full = cmd + " 2>&1"; // redirect stderr to stdout (match linux/macos)
        FILE* pipe = _popen(full.c_str(), "r");
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

    // 流式逐行读，见 linux 分区的说明。_pclose 直接给退出码，无需 wait status 解码。
    export int run_command_lines(const std::string& cmd,
                                 const std::function<void(std::string_view)>& on_line) {
        std::string full = cmd + " 2>&1";
        FILE* pipe = _popen(full.c_str(), "r");
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
        if (!line.empty()) on_line(line);

        return _pclose(pipe);
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

    export inline std::string get_xlings_bin() {
        return get_home_dir() + "\\.xlings\\subos\\current\\bin\\xlings.exe";
    }

    export void set_env_variable(const std::string& key, const std::string& value) {
        _putenv_s(key.c_str(), value.c_str());
    }

    // println implementation forwarding to std::println for Windows
    export template<typename... Args>
    void println(std::format_string<Args...> fmt, Args&&... args) {
        std::println(fmt, std::forward<Args>(args)...);
    }

    export inline void println(const std::string& msg) {
        std::println("{}", msg);
    }

} // namespace platform_impl
} // namespace d2x

#endif // defined(_WIN32)