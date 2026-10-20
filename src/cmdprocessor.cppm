module;
#include <cstdio>

export module d2x.cmdprocessor;

import std;
import mcpplibs.cmdline;

import d2x.log;
import d2x.platform;
import d2x.checker;
import d2x.config;
import d2x.xlings;

namespace d2x::cmdprocessor {

using namespace mcpplibs;

void apply_global_options(const cmdline::ParsedArgs& args) {
    if (auto v = args.value("lang"))
        platform::set_env_variable(std::string(EnvVars::D2X_LANG), *v);
    if (auto v = args.value("log-level"))
        platform::set_env_variable("D2X_LOG_LEVEL", *v);
    if (auto v = args.value("ui"))
        platform::set_env_variable(std::string(EnvVars::D2X_UI_BACKEND), *v);
    if (auto v = args.value("llm-prompt"))
        platform::set_env_variable(std::string(EnvVars::D2X_LLM_SYSTEM_PROMPT), *v);
    if (auto v = args.value("llm-api-key"))
        platform::set_env_variable(std::string(EnvVars::D2X_LLM_API_KEY), *v);
    if (auto v = args.value("llm-api-url"))
        platform::set_env_variable(std::string(EnvVars::D2X_LLM_API_URL), *v);
}

void new_project(const cmdline::ParsedArgs& args) {
    std::string project_name = args.positional_or(0, "d2x-project");

    if (std::filesystem::exists(project_name)) {
        std::println("目录 '{}' 已存在，无法创建项目", project_name);
        return;
    }

    if (!xlings::require_xlings()) return;

    std::string cmd = "xlings install d2x:project-template -y";
    std::println("加载项目模板...");
    int status = platform::exec(cmd);
    if (status != 0) {
        std::println("项目模板安装失败");
        return;
    }

    if (std::rename("project-template", project_name.c_str()) != 0) {
        std::println("无法重命名项目目录");
        return;
    }

    auto d2x_json_file = std::filesystem::path(project_name) / ".d2x.json";
    if (std::filesystem::exists(d2x_json_file)) {
        std::println("项目完整性检查 - ok");
    } else {
        std::println("项目创建失败: 缺少 .d2x.json 文件");
        return;
    }

    std::println("项目创建成功: {}", project_name);
}

// Policy stays local to book; platform only probes the requested endpoint.
int run_book(const cmdline::ParsedArgs& args) {
    try {
        auto configured = Config::book_port(args.value("port"));
        auto bookdir = std::filesystem::path(platform::get_rundir()) / "book";
        const auto& lang = Config::lang();
        if (!lang.empty() && lang != "auto") {
            auto langdir = bookdir / lang;
            if (std::filesystem::is_directory(langdir)) bookdir = langdir;
        }
        if (!std::filesystem::is_directory(bookdir)) {
            std::println(stderr, "Error: No book found at {}", bookdir.string());
            return 1;
        }
        auto quoted = platform::quote_argument(bookdir.string());
        if (!quoted) {
            std::println(stderr, "Error: Book path contains unsupported shell characters: {}", bookdir.string());
            return 1;
        }
        if (!xlings::require_xlings()) return 1;
        int status = platform::normalize_exit_status(platform::exec("xlings install mdbook -y"));
        if (status != 0) {
            std::println(stderr, "Error: mdBook installation failed (exit {})", status);
            return status;
        }
        int first = configured ? configured->value : 3000;
        int last = configured ? first : 3099;
        for (int port = first; port <= last; ++port) {
            auto available = platform::probe_tcp_port(port);
            if (!available) {
                std::println(stderr, "Error: Cannot check localhost:{}: {}", port, available.error());
                return 1;
            }
            if (!*available) {
                if (configured) {
                    std::println(stderr, "Error: Book port {} from {} is in use; choose another with d2x book --port <N>",
                        port, configured->source);
                    return 1;
                }
                continue;
            }
            if (port > first)
                std::println("Book ports {}-{} are in use; trying http://localhost:{}", first, port - 1, port);
            std::println("Opening book: {}", bookdir.string());
            std::cout.flush();
            status = platform::normalize_exit_status(platform::exec(
                std::format("mdbook serve --open --port {} {}", port, *quoted)));
            // Ctrl+C/termination is a normal way to stop this foreground server.
            if (status != 0 && status != 130 && status != 143)
                std::println(stderr, "Error: mdBook exited with {}; retry or choose another port with d2x book --port <N>", status);
            return status;
        }
        std::println(stderr, "Error: Book ports 3000-3099 are in use; choose another with d2x book --port <N>");
    } catch (const std::exception& e) {
        std::println(stderr, "Error: {}", e.what());
    }
    return 1;
}

export int run(int argc, char* argv[]) {
    int command_exit_code = 0;
    // An option builder's action() returns its immediate App. Build this
    // subcommand separately so subsequent commands stay attached to d2x.
    auto book_command = cmdline::App("book");
    book_command.description("open project's book")
        .option("port").short_name('p').takes_value().value_name("PORT")
            .help("book port (1-65535); default: first available in 3000-3099")
        .action([&command_exit_code](const cmdline::ParsedArgs& a) {
            apply_global_options(a);
            command_exit_code = run_book(a);
        });
    auto app = cmdline::App("d2x")
        .version(std::string(Info::VERSION))
        .description("d2x command line tool")
        .option("lang").takes_value().global().help("set language (zh, en)")
        .option("log-level").takes_value().global().help("set log level (info, debug)")
        .option("ui").takes_value().global().help("set UI backend (print, tui)")
        .option("llm-prompt").takes_value().global().help("set LLM system prompt")
        .option("llm-api-key").takes_value().global().help("set LLM API key")
        .option("llm-api-url").takes_value().global().help("set LLM API URL")
        // 让外部前端（VSCode 插件 / Web / CI）直接消费上行事件流，
        // 不必链接 d2x，也不必解析 TUI 的转义序列。
        .option("emit-events").global().help("emit the frontend protocol as NDJSON on stdout")
        .subcommand("new")
            .description("create new d2x project from template")
            .arg("project-name").help("project name")
            .action([](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                new_project(a);
            })
        .subcommand("install")
            .description("install d2x package via xlings")
            .arg("package").required().help("package name")
            .action([&command_exit_code](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                command_exit_code = xlings::install(std::string(a.positional(0))) ? 0 : 1;
            })
        .subcommand(std::move(book_command))
        .subcommand("checker")
            .description("run checker for d2x project's exercises")
            .arg("target").help("exercise name (substring match)")
            .action([&command_exit_code](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                command_exit_code = checker::run(std::string(a.positional_or(0, "")), a.is_flag_set("emit-events"));
            })
        .subcommand("status")
            .description("show exercise progress overview (read-only)")
            .action([](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                checker::status(a.is_flag_set("emit-events"));
            })
        .subcommand("config")
            .description("configure d2x (.d2x.json)")
            .action([](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                Config::run_interactive_config();
            })
        .subcommand("list")
            .description("list available d2x packages")
            .arg("query").help("search query")
            .action([](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                xlings::list(std::string(a.positional_or(0, "")));
            });

    int parse_status = app.run(argc, argv);
    return parse_status != 0 ? parse_status : command_exit_code;
}

} // namespace d2x::cmdprocessor
