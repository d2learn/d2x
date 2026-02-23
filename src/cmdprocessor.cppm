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

    xlings::ensure_xlings_installed();

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

export int run(int argc, char* argv[]) {
    auto app = cmdline::App("d2x")
        .version(std::string(Info::VERSION))
        .description("d2x command line tool")
        .option("lang").takes_value().global().help("set language (zh, en)")
        .option("log-level").takes_value().global().help("set log level (info, debug)")
        .option("ui").takes_value().global().help("set UI backend (print, tui)")
        .option("llm-prompt").takes_value().global().help("set LLM system prompt")
        .option("llm-api-key").takes_value().global().help("set LLM API key")
        .option("llm-api-url").takes_value().global().help("set LLM API URL")
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
            .action([](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                xlings::install(std::string(a.positional(0)));
            })
        .subcommand("book")
            .description("open project's book")
            .action([](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                auto bookdir = std::filesystem::path(platform::get_rundir()) / "book";
                if (Config::lang() == "en") bookdir /= "en";
                std::println("Opening book: {}", bookdir.string());
                if (std::filesystem::exists(bookdir)) {
                    platform::run_command_capture("xlings install mdbook -y");
                    platform::exec(("mdbook serve --open " + bookdir.string()).c_str());
                } else
                    std::println("Error: No book found");
            })
        .subcommand("checker")
            .description("run checker for d2x project's exercises")
            .arg("target").help("target name")
            .action([](const cmdline::ParsedArgs& a) {
                apply_global_options(a);
                checker::run(std::string(a.positional_or(0, "")));
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

    return app.run(argc, argv);
}

} // namespace d2x::cmdprocessor
