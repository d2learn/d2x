// xlings 集成:依赖策略是「直接用,缺失即报错」。
//
// d2x 不代装 xlings(2026-07-24 设计文档 D3 决策):代装是安装器里再跑
// 安装器,失败面大、责任边界混乱。缺失时输出分平台官方安装命令后退出,
// 由使用者自行安装——一条明确的报错比「帮你装」更可预期。
module;

// stderr 是宏,import std 不提供
#include <cstdio>

export module d2x.xlings;

import std;

import d2x.platform;
import d2x.utils;

namespace d2x {
namespace xlings {

[[nodiscard]] bool has_xlings() {
    // 先看二进制路径,再退化到 --version 探测。
    if (std::filesystem::exists(platform::get_xlings_bin())) {
        return true;
    }

    auto [status, output] = d2x::platform::run_command_capture("xlings --version");
    auto clean_output = d2x::utils::strip_ansi(output);
    // regex_search 而非 regex_match:输出里将来多一行 banner 不该被误判为
    // 「未安装」(误判会把使用者引向不必要的重装)。
    return status == 0
        && std::regex_search(clean_output, std::regex("xlings (\\d+\\.\\d+\\.\\d+)"));
}

// 缺失即报错并给出安装命令;返回是否可用。所有依赖 xlings 的子命令
// (install/new/book/list)共用这一个入口。
export bool require_xlings() {
    if (has_xlings()) return true;

    std::println(stderr, "error: 未检测到 xlings(d2x 的包管理依赖)");
    std::println(stderr, "安装后重试本命令:");
    std::println(stderr, "  Linux/macOS:  curl -fsSL https://d2learn.org/xlings-install.sh | bash");
    std::println(stderr, "  Windows:      irm https://d2learn.org/xlings-install.ps1.txt | iex");
    return false;
}

// 包名最终拼进 shell 命令——与练习 id 同一纪律:白名单校验,拒绝而非转义。
[[nodiscard]] bool valid_pkgname(std::string_view name) {
    if (name.empty()) return false;
    return std::ranges::all_of(name, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    });
}

export bool install(const std::string& pkgname) {
    if (!valid_pkgname(pkgname)) {
        std::println(stderr, "error: 非法包名 '{}'(仅允许 [A-Za-z0-9._-])", pkgname);
        return false;
    }
    if (!require_xlings()) return false;

    if (std::filesystem::exists(pkgname)) {
        std::println(stderr, "error: 目录 '{}' 已存在——不做静默覆盖。", pkgname);
        std::println(stderr, "如需更新,进入该目录使用课程自带的更新方式(如 d2x update / git pull)。");
        return false;
    }

    std::string command = "xlings install d2x:" + pkgname + " -y";
    std::println("正在执行: {}", command);
    int status = platform::exec(command);

    if (status != 0) {
        std::println(stderr, "error: 安装失败(退出码 {})。排查建议:", status);
        std::println(stderr, "  1. 刷新索引:  xlings update");
        std::println(stderr, "  2. 网络受限:  xlings config --mirror CN");
        std::println(stderr, "  3. 确认包名:  d2x list {}", pkgname);
        return false;
    }

    // 校验结果:课程项目的标志是 .d2x.json。
    auto marker = std::filesystem::path(pkgname) / ".d2x.json";
    if (!std::filesystem::exists(marker)) {
        std::println(stderr, "warning: 安装命令成功但未找到 {}——课程可能安装到了其他目录,以 xlings 输出为准。",
                     marker.string());
        return true;
    }

    std::println("");
    std::println("安装完成。开始练习:");
    std::println("  cd {} && d2x checker", pkgname);
    return true;
}

export void list(const std::string& query = "") {
    if (!require_xlings()) return;

    std::string command = "xim -s d2x:" + (valid_pkgname(query) || query.empty() ? query : "");
    auto [status, output] = d2x::platform::run_command_capture(command);

    if (status != 0) {
        std::println(stderr, "查询失败: {}", output);
        return;
    }
    std::println("{}", output);
}

} // namespace xlings
} // namespace d2x
