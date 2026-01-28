export module d2x.config;

import std;

namespace d2x {
// export
export struct D2XInfo;

// impl
struct D2XInfo {
    static constexpr std::string_view VERSION = "0.1.1";
    static constexpr std::string_view REPO = "https://github.com/d2learn/d2x";
};
} // namespace d2x