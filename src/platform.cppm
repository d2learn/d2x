export module d2x.platform;

import std;

export import :windows;
export import :linux;
export import :macos;

namespace d2x {
namespace platform {

    static std::string gRundir = std::filesystem::current_path().string();

    export using platform_impl::XLINGS_BIN;
    export using platform_impl::XLINGS_INSTALL_CMD;

    export using platform_impl::clear_console;
    export using platform_impl::get_home_dir;
    //export using platform_impl::xlings_install;
    export using platform_impl::set_env_variable;
    export using platform_impl::println;

    export [[nodiscard]] std::string get_rundir() {
        return gRundir;
    }

    export [[nodiscard]] std::string get_system_language() {
        try {
            // Get the system's default locale
            auto loc = std::locale("");
            auto name = loc.name();

            // Extract language code from locale name
            // Format examples: "zh_CN.UTF-8", "en_US.UTF-8", "C", "POSIX"
            if (name.empty() || name == "C" || name == "POSIX") {
                return "en";
            }
            
            // Find first delimiter and extract language code
            if (auto pos = name.find_first_of("_-.@"); pos != std::string::npos) {
                return name.substr(0, pos);
            }
            
            return name;
        } catch (const std::runtime_error&) {
            // Locale initialization failed, fallback to English
            return "en";
        }
    }

    export std::pair<int, std::string> run_command_capture(const std::string& cmd) {
        set_env_variable("LD_LIBRARY_PATH", "");
        return platform_impl::run_command_capture(cmd);
    }

    export int exec(const std::string& cmd) {
        // TODO: fix return 139 issue (on linux)
        // workaround by clear LD_LIBRARY_PATH
        set_env_variable("LD_LIBRARY_PATH", "");
        return std::system(cmd.c_str());
    }

    export bool xlings_install() {
        std::println("正在安装 xlings...");
        int status = platform::exec(std::string(XLINGS_INSTALL_CMD));
        if (status == 0) {
            std::println("xlings 安装成功！");
            std::string xlings_path { std::filesystem::path(XLINGS_BIN).parent_path().string() };
            char* path_env = std::getenv("PATH");
            if (path_env) {
                std::string new_path = std::string(path_env) + ";" + xlings_path;
                set_env_variable("PATH", new_path.c_str());
            }
            return true;
        }
        std::println("xlings 安装失败");
        return false;
    }
} // namespace platform
} // namespace d2x
