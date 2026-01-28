module;

#include <cstdlib>

export module d2x.platform;

import std;

#if defined(_WIN32)
import d2x.platform.windows;
#else
import d2x.platform.linux;
#endif

namespace d2x {
namespace platform {

    static std::string gRundir = std::filesystem::current_path().string();

    export using platform_impl::run_command_capture;
    export using platform_impl::clear_console;
    export using platform_impl::get_home_dir;
    export using platform_impl::xlings_install;

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

    // setenv wrapper
    export void set_env_variable(const std::string& key, const std::string& value) {
        setenv(key.c_str(), value.c_str(), 1);
    }
} // namespace platform
} // namespace d2x
