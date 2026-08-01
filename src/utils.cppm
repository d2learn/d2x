export module d2x.utils;

import std;

namespace d2x {
export namespace utils {

// 把绝对路径压成相对 cwd 的短路径,用于练习页展示;不在 cwd 之下(或压不出
// 相对关系)时原样返回。
//
// 不再手写「前缀匹配 + 掐掉一个 '/'」:那种写法只认 '/',Windows 上分隔符是
// '\',前导分隔符掐不掉,练习页会显示成 "\src\intro\tests\hello-mcpp.cpp"
// 这种带前导分隔符的怪路径(Win10/Win11 CI 实测)。交给 lexically_relative
// 处理,分隔符与 ".." 情形都由标准库负责。
//
// 输出统一用 generic_string()(正斜杠):课程与文档都以正斜杠书写路径,两个
// 平台显示一致也便于 CI 断言。
std::string normalize_path(std::string path) {
    if (path.empty()) return "N/A";

    std::error_code ec;
    const auto current = std::filesystem::current_path(ec);
    if (ec) return path;

    // 纯词法运算,不碰文件系统——路径可能来自 Provider,未必存在于本机。
    const auto rel = std::filesystem::path(path).lexically_relative(current);
    if (rel.empty()) return path;                       // 压不出相对关系(如本就是相对路径)
    if (rel.begin()->string() == "..") return path;     // 在 cwd 之外,相对形式反而更难读

    return rel.generic_string();
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    // use std::views and std::ranges to split string
    std::vector<std::string> result;
    auto view = str | std::views::split(delimiter);
    for (const auto& part : view) {
        result.push_back(std::string(part.begin(), part.end()));
    }
    return result;
}

// TODO: Optimize
std::string trim_string(const std::string& str) {
    // Remove ANSI escape sequences manually without regex
    std::string cleaned;
    cleaned.reserve(str.size());
    
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\x1b' && i + 1 < str.size() && str[i + 1] == '[') {
            // Skip ANSI escape sequence
            i += 2;
            while (i < str.size() && str[i] != 'm') {
                ++i;
            }
        } else {
            cleaned.push_back(str[i]);
        }
    }
    
    // Trim whitespace using standard algorithms
    auto start = std::ranges::find_if_not(cleaned, 
        [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::ranges::find_if_not(cleaned | std::views::reverse,
        [](unsigned char ch) { return std::isspace(ch); }).base();
    
    return std::string(start, end);
}

bool wait_files_changed(const std::vector<std::string>& files, int out_ms) {
    bool changed = false;
    int check_interval = 500; // milliseconds

    auto last_mod_times = [&]() {
        std::int64_t mod_times = 0;
        for (const auto& file : files) {
            //std::println("Debug: Checking file {}", file);
            // TODO: file need is absolute by d2x-buildtools
            if (std::filesystem::exists(file)) {
                auto ftime = std::filesystem::last_write_time(file);
                mod_times += ftime.time_since_epoch().count();
            }
        }
        return mod_times;
    };

    auto previous_mod_times = last_mod_times();

    while (!changed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval));
        out_ms -= check_interval;
        if (out_ms <= 0) { break; }

        auto current_mod_times = last_mod_times();
        //std::println("Debug: previous_mod_times = {}, current_mod_times = {}", previous_mod_times, current_mod_times);
        if (current_mod_times != previous_mod_times) {
            changed = true;
        }
    }

    return changed;
}

std::string read_file_to_string(const std::string& filepath) {
    std::ifstream file_stream(filepath);
    if (!file_stream.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file_stream.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::string strip_ansi(const std::string& str) {
    std::string cleaned;
    cleaned.reserve(str.size());

    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\x1b' && i + 1 < str.size() && str[i + 1] == '[') {
            // Skip ANSI escape sequence (ESC[...m)
            i += 2;
            while (i < str.size() && str[i] != 'm') {
                ++i;
            }
        } else {
            cleaned.push_back(str[i]);
        }
    }

    return cleaned;
}

[[nodiscard]] std::string get_env_or_default(std::string_view name, std::string_view default_value = "") {
    if (const char* value = std::getenv(name.data()); value != nullptr) {
        return value;
    }
    return std::string{default_value};
}

bool ask_yes_no(const std::string& question, bool default_yes = false) {
    std::string prompt = default_yes ? "[Y/n] " : "[y/N] ";
    std::print("{}{}", question, prompt);
    std::cout.flush();

    std::string input;
    if (!std::getline(std::cin, input)) return default_yes;

    if (input.empty()) return default_yes;

    return input[0] == 'y' || input[0] == 'Y';
}

std::string ask_input(const std::string& prompt, const std::string& default_value = "") {
    std::string display_default = default_value.empty() ? "(空)" : default_value;
    std::print("{} [{}]: ", prompt, display_default);
    std::cout.flush();

    std::string input;
    if (!std::getline(std::cin, input)) return default_value;

    return input.empty() ? default_value : input;
}

void print_separator(const std::string& title) {
    std::println("\n=== {} ===", title);
}

} // namespace utils
} // namespace d2x
