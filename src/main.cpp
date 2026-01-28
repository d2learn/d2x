import std;

import d2x.log;
import d2x.checker;
import d2x.platform;
import d2x.config;

/*
d2x command [options] --xxx-xxx

Environment variables (can also be set via command line options):
export D2X_LANG=zh or en
export D2X_LOG_LEVEL=info or debug
export D2X_UI_BACKEND=simple_print or tui
export D2X_LLM_SYSTEM_PROMPT="You are a helpful programming assistant."
export LLM_API_KEY="sk-xxxxxx"
export LLM_API_URL="https://xxx.xxx.com/v1"
*/

void print_help() {
    std::println("d2x version: {}\n", d2x::Info::VERSION);
    std::println("Usage: $ d2x [command] [target] [options]\n");
    std::println("Commands:");
    std::println("\t new       \t create new d2x project");
    std::println("\t book      \t open project's book");
    std::println("\t run       \t run sourcecode file");
    std::println("\t checker   \t run checker for d2x project's exercises");
    std::println("\t config    \t configure d2x settings");
    std::println("\t help      \t help info\n");
    std::println("Options:");
    std::println("\t --lang <language>          \t set language (zh, en)");
    std::println("\t --log-level <level>        \t set log level (info, debug)");
    std::println("\t --ui <backend>             \t set UI backend (simple_print, tui)");
    std::println("\t --llm-prompt <prompt>      \t set LLM system prompt");
    std::println("\t --llm-api-key <key>        \t set LLM API key");
    std::println("\t --llm-api-url <url>        \t set LLM API URL");
}

struct CommandOptions {
    std::optional<std::string> lang;
    std::optional<std::string> log_level;
    std::optional<std::string> ui_backend;
    std::optional<std::string> llm_prompt;
    std::optional<std::string> llm_api_key;
    std::optional<std::string> llm_api_url;
};

CommandOptions parse_options(int argc, char* argv[], int start_idx) {
    CommandOptions opts;
    
    for (int i = start_idx; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--lang" && i + 1 < argc) {
            opts.lang = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            opts.log_level = argv[++i];
        } else if (arg == "--ui" && i + 1 < argc) {
            opts.ui_backend = argv[++i];
        } else if (arg == "--llm-prompt" && i + 1 < argc) {
            opts.llm_prompt = argv[++i];
        } else if (arg == "--llm-api-key" && i + 1 < argc) {
            opts.llm_api_key = argv[++i];
        } else if (arg == "--llm-api-url" && i + 1 < argc) {
            opts.llm_api_url = argv[++i];
        }
    }
    
    return opts;
}

void apply_options(const CommandOptions& opts) {
    if (opts.lang) {
        d2x::platform::set_env_variable(std::string{d2x::EnvVars::D2X_LANG}, *opts.lang);
    }
    if (opts.log_level) {
        d2x::platform::set_env_variable("D2X_LOG_LEVEL", *opts.log_level);
    }
    if (opts.ui_backend) {
        d2x::platform::set_env_variable(std::string{d2x::EnvVars::D2X_UI_BACKEND}, *opts.ui_backend);
    }
    if (opts.llm_prompt) {
        d2x::platform::set_env_variable(std::string{d2x::EnvVars::D2X_LLM_SYSTEM_PROMPT}, *opts.llm_prompt);
    }
    if (opts.llm_api_key) {
        d2x::platform::set_env_variable(std::string{d2x::EnvVars::D2X_LLM_API_KEY}, *opts.llm_api_key);
    }
    if (opts.llm_api_url) {
        d2x::platform::set_env_variable(std::string{d2x::EnvVars::D2X_LLM_API_URL}, *opts.llm_api_url);
    }
}

int main(int argc, char* argv[]) {

    if (argc == 1 || (argc >= 2 && std::string(argv[1]) == "help")) {
        print_help();
        return 0;
    }

    std::string command = argv[1];
    
    // Parse and apply command-line options
    auto options = parse_options(argc, argv, 2);
    apply_options(options);

    if (command == "new") {
        std::println("TODO: Creating new d2x project...");
    } else if (command == "book") {
        // if book exists, open it by mdbook
        auto rundir = d2x::platform::get_rundir();
        auto bookdir = std::filesystem::path(rundir) / "book";
        std::println("Opening book in directory: {}", bookdir.string());
        if (std::filesystem::exists(bookdir)) {
            std::system(("mdbook serve --open " + bookdir.string()).c_str());
        } else {
            std::println("Error: No book found in directory: {}", bookdir.string());
        }
    } else if (command == "run") {
        std::println("TODO: Running sourcecode file...");
    } else if (command == "checker") {
            d2x::checker::run();
    } else if (command == "config") {
        d2x::Config::run_interactive_config();
    } else {
        std::println("Unknown command: {}", command);
        std::println("Use 'd2x help' for usage information");
        return 1;
    }

    return 0;
}