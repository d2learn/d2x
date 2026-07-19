// 打开练习文件。
//
// 这是「策略」而非「机制」：打开编辑器既不是结构也不是显示，而是对学员
// 机器的副作用。d2x 作为通用框架不该替人决定用哪个编辑器，甚至不该决定
// 要不要开 —— 原先硬编码 `code`，等于假定所有人都装了 VS Code。
//
// 所以：
//   - 命令可配置（.d2x.json 的 "editor"，或 D2X_EDITOR 环境变量）
//   - 未配置时按 $VISUAL → $EDITOR → code 依次回退
//   - 显式配成空字符串 = 关闭，一行都不动学员的环境
//   - --emit-events 模式下由调用方跳过：外部前端从事件流里已经拿到
//     exercise 和 verdict，开不开、怎么开是它自己的事，两边都动只会打架
//
// 通用的 hook 机制不必单独造 —— 事件流本身就是。这里只留一个旋钮，
// 对应人们真正想调的那一件事。
export module d2x.editor;

import std;

import d2x.log;
import d2x.config;
import d2x.platform;

namespace d2x {
namespace editor {

// 是否显式关闭（配置里写了空串）。未配置不算关闭，走回退链。
export bool disabled() {
    return Config::editor_is_set() && Config::editor().empty();
}

std::string resolve_command() {
    if (const auto& configured = Config::editor(); !configured.empty()) return configured;

    for (const char* name : {"VISUAL", "EDITOR"}) {
        if (const char* v = std::getenv(name); v && *v) return v;
    }
    return "code";   // 保持既有默认行为
}

export void open(const std::string& file_path) {
    if (disabled()) return;

    auto command_template = resolve_command();
    auto absolute_path = std::filesystem::absolute(file_path).string();

    // 支持 {file} 占位符，方便 `emacsclient -n {file}` 这类需要把路径放
    // 中间的命令；没有占位符就按惯例追加到末尾。
    std::string command;
    if (auto at = command_template.find("{file}"); at != std::string::npos) {
        command = command_template;
        command.replace(at, std::string_view{"{file}"}.size(),
                        std::format("\"{}\"", absolute_path));
    } else {
        command = std::format("{} \"{}\"", command_template, absolute_path);
    }

    if (platform::exec(command) != 0) {
        log::warning("打开编辑器失败: {}", command);
        log::warning("可在 .d2x.json 设置 \"editor\"（留空则不自动打开）");
    }
}

} // namespace editor
} // namespace d2x
