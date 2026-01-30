export module d2x.platform;

import std;

export import :windows;
export import :linux;

namespace d2x {
namespace platform {

    static std::string gRundir = std::filesystem::current_path().string();

    export using platform_impl::XLINGS_BIN;

    export using platform_impl::run_command_capture;
    export using platform_impl::clear_console;
    export using platform_impl::get_home_dir;
    export using platform_impl::xlings_install;
    export using platform_impl::set_env_variable;

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
} // namespace platform
} // namespace d2x
