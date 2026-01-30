export module d2x.cmdprocessor;

import std;

import d2x.log;
import d2x.platform;
import d2x.checker;
import d2x.config;
import d2x.xlings;

namespace d2x::cmdprocessor {

struct CommandInfo {
    std::string name;
    std::string description;
    std::string usage;
    std::function<int(int argc, char* argv[])> func;
};

class CommandProcessor {
public:
    CommandProcessor& add(std::string name, std::string description,
                          std::function<int(int argc, char* argv[])> func,
                          std::string usage = "") {
        if (usage.empty()) usage = std::format("d2x {}", name);
        commands_.push_back({std::move(name), std::move(description),
                            std::move(usage), std::move(func)});
        return *this;
    }

    int run(int argc, char* argv[]) {
        if (argc <= 1) return print_help();

        std::string cmd = argv[1];
        if (
            cmd == "help" || cmd == "--help"
            || cmd == "-h" || cmd == "--version"
        ) return print_help();

        for (const auto& c : commands_) {
            if (c.name == cmd) return c.func(argc, argv);
        }

        log::error("Unknown command: {}", cmd);
        std::println("Use 'd2x help' for usage information");
        return 1;
    }

    int print_help() const {
        std::println("d2x version: {}\n", d2x::Info::VERSION);
        std::println("Usage: $ d2x [command] [target] [options]\n");
        std::println("Commands:");
        for (const auto& c : commands_)
            std::println("\t {:10}\t{}", c.name, c.description);
        std::println("\nOptions:");
        std::println("\t --lang <language>          \t set language (zh, en)");
        std::println("\t --log-level <level>        \t set log level (info, debug)");
        std::println("\t --ui <backend>             \t set UI backend (print, tui)");
        std::println("\t --llm-prompt <prompt>      \t set LLM system prompt");
        std::println("\t --llm-api-key <key>        \t set LLM API key");
        std::println("\t --llm-api-url <url>        \t set LLM API URL");
        return 0;
    }

private:
    std::vector<CommandInfo> commands_;
};

int new_project(int argc, char* argv[]);

export CommandProcessor create_processor() {
    return CommandProcessor{}
        .add(
            "new",
            "create new d2x project from template",
            new_project,
            "d2x new <project-name>"
        )
        .add("install", "install d2x package via xlings",
            [](int argc, char* argv[]) {
                if (argc < 3) { std::println("Usage: d2x install <package>"); return 1; }
                d2x::xlings::install(argv[2]); return 0;
            },
            "d2x install <package-name>"
        ).add("book", "open project's book",
            [](int, char**) {
                auto bookdir = std::filesystem::path(d2x::platform::get_rundir()) / "book";
                std::println("Opening book: {}", bookdir.string());
                if (std::filesystem::exists(bookdir)) {
                    platform::run_command_capture("xlings install mdbook -y");
                    std::system(("mdbook serve --open " + bookdir.string()).c_str());
                } else
                    std::println("Error: No book found");
                return 0;
            })
        .add("checker", "run checker for d2x project's exercises",
            [](int, char**) { d2x::checker::run(); return 0; })
        .add("config", "configure d2x (.d2x.json)",
            [](int, char**) { d2x::Config::run_interactive_config(); return 0; })
        .add("list", "list available d2x packages",
            [](int argc, char* argv[]) {
                d2x::xlings::list(argc >= 3 ? argv[2] : ""); return 0;
            }, "d2x list [query]");
}

int new_project(int argc, char* argv[]) {
    std::string project_name = argc >= 3 ? argv[2] : "";

    // if exists project_name directory, error
    if (!project_name.empty() && std::filesystem::exists(project_name)) {
        std::println("目录 '{}' 已存在，无法创建项目", project_name);
        return 1;
    }

    d2x::xlings::ensure_xlings_installed();

    std::string cmd = "xlings install d2x:project-template -y";
    std::println("加载项目模板...");
    auto [status, output] = d2x::platform::run_command_capture(cmd);
    if (status != 0) {
        std::println("项目模板安装失败: {}", output);
        return 1;
    }

    if (project_name.empty()) {
        project_name = "d2x-project";
    }

    if (std::rename("project-template", project_name.c_str()) != 0) {
        std::println("无法重命名项目目录");
        return 1;
    }

    auto d2x_json_file = std::filesystem::path(project_name) / ".d2x.json";
    if (std::filesystem::exists(d2x_json_file)) {
        std::println("项目完整性检查 - ok");
    } else {
        std::println("项目创建失败: 缺少 .d2x.json 文件");
        return 1;
    }

    std::println("项目创建成功: {}", project_name);
    return 0;
};

} // namespace d2x::cmdprocessor
