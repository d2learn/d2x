export module d2x.xlings;

import std;

import d2x.platform;
import d2x.utils;

namespace d2x {
namespace xlings {

export [[nodiscard]] bool has_xlings() {
    auto [status, _] = d2x::platform::run_command_capture("xlings");
    return status == 0;
}

export bool install(const std::string& pkgname) {

    std::println("开始安装 -> {}", pkgname);

    // Check if xlings is installed
    if (!has_xlings()) {
        std::print("xlings 未安装，是否现在安装? [Y/n]: ");
        std::cout.flush();
        std::string input;
        if (!std::getline(std::cin, input)) return false;
        if (!input.empty() && input[0] != 'y' && input[0] != 'Y') {
            std::println("已取消安装");
            return false;
        }

        std::println("正在安装 xlings...");
        if (!d2x::platform::xlings_install()) {
            std::println("xlings 安装失败");
            return false;
        }
    }

    // Install the package
    std::string command = "xlings install d2x:" + pkgname;
    std::println("正在执行: {}", command);
    int status = std::system(command.c_str());

    if (status == 0) {
        return true;
    }

    std::println("安装失败，命令返回状态码: {}", status);
    return false;
}

export void list(const std::string& query = "") {
    std::string command = "xim -s d2x:" + query;
    auto [status, output] = d2x::platform::run_command_capture(command);

    if (status != 0) {
        std::println("查询失败: {}", output);
        return;
    }

    // Strip ANSI escape codes and print from first '{'
    //auto clean_output = d2x::utils::strip_ansi(output);
    //auto pos = clean_output.find('{');
    //if (pos != std::string::npos) {
    //    clean_output = clean_output.substr(pos);
    //}
    //std::println("{}", clean_output);

    std::println("{}", output);
}

} // namespace xlings
} // namespace d2x
