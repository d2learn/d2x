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
} // namespace platform_impl
} // namespace d2x

#endif // defined(_WIN32)