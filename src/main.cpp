import std;

import d2x.cmdprocessor;
import d2x.config;
import d2x.platform;

/*
d2x command [options] --xxx-xxx

Environment variables (can also be set via command line options):
export D2X_LANG=zh or en
export D2X_LOG_LEVEL=info or debug
export D2X_UI_BACKEND=print or tui
export D2X_LLM_SYSTEM_PROMPT="You are a helpful programming assistant."
export LLM_API_KEY="sk-xxxxxx"
export LLM_API_URL="https://xxx.xxx.com/v1"
*/

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
        d2x::platform::set_env_variable(std::string(d2x::EnvVars::D2X_LANG), *opts.lang);
    }
    if (opts.log_level) {
        d2x::platform::set_env_variable("D2X_LOG_LEVEL", *opts.log_level);
    }
    if (opts.ui_backend) {
        d2x::platform::set_env_variable(std::string(d2x::EnvVars::D2X_UI_BACKEND), *opts.ui_backend);
    }
    if (opts.llm_prompt) {
        d2x::platform::set_env_variable(std::string(d2x::EnvVars::D2X_LLM_SYSTEM_PROMPT), *opts.llm_prompt);
    }
    if (opts.llm_api_key) {
        d2x::platform::set_env_variable(std::string(d2x::EnvVars::D2X_LLM_API_KEY), *opts.llm_api_key);
    }
    if (opts.llm_api_url) {
        d2x::platform::set_env_variable(std::string(d2x::EnvVars::D2X_LLM_API_URL), *opts.llm_api_url);
    }
}

int main(int argc, char* argv[]) {
    auto options = parse_options(argc, argv, 1);
    apply_options(options);

    auto processor = d2x::cmdprocessor::create_processor();
    return processor.run(argc, argv);
}
