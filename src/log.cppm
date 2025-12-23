export module d2x.log;

import std;

template<typename... Args>
void log_print(const std::string& level, const std::string& color, std::format_string<Args...> fmt, Args&&... args) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);

    std::println("{}{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}\t[D2X:{}] | {}", 
        color,
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        level, //"\033[0m",
        std::format(fmt, std::forward<Args>(args)...));
}

namespace d2x {
export namespace log {

// C++23 std::println + 变参模板

template<typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    log_print("DEBUG", "\033[36m", fmt, std::forward<Args>(args)...);  // 青色
}

template<typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    log_print("INFO", "\033[32m", fmt, std::forward<Args>(args)...);  // 绿色
}

template<typename... Args>
void warning(std::format_string<Args...> fmt, Args&&... args) {
    log_print("WARNING", "\033[33m", fmt, std::forward<Args>(args)...);  // 黄色
}

template<typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    log_print("ERROR", "\033[31m", fmt, std::forward<Args>(args)...);  // 红色
}

} // namespace log
} // namespace d2x
