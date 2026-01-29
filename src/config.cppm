export module d2x.config;

import std;

import mcpplibs.llmapi;

import d2x.json;
import d2x.platform;
import d2x.utils;

namespace d2x {

using namespace mcpplibs;

export struct Info;
export struct EnvVars;
export class Config;

struct Info {
    static constexpr std::string_view VERSION = "0.1.1";
    static constexpr std::string_view REPO = "https://github.com/d2learn/d2x";
};

struct EnvVars {
    // UI
    static constexpr std::string_view D2X_UI_BACKEND = "D2X_UI_BACKEND";

    // Language
    static constexpr std::string_view D2X_LANG = "D2X_LANG";

    // BuildTools
    static constexpr std::string_view D2X_BUILDTOOLS = "D2X_BUILDTOOLS";

    // LLM
    static constexpr std::string_view D2X_LLM_API_KEY = "D2X_LLM_API_KEY";
    static constexpr std::string_view D2X_LLM_API_URL = "D2X_LLM_API_URL";
    static constexpr std::string_view D2X_LLM_API_MODEL = "D2X_LLM_API_MODEL";
    static constexpr std::string_view D2X_LLM_SYSTEM_PROMPT = "D2X_LLM_SYSTEM_PROMPT";
};

// Global configuration manager (includes lang, ui, llm)
class Config {
public:
    enum class Scope {
        Global,
        Local
    };

    struct LLMConfig {
        std::string api_key;
        std::string api_url;
        std::string model;
        std::string system_prompt;

        [[nodiscard]] bool is_enabled() const {
            return !api_key.empty() && api_key != "sk-xxx";
        }
    };

    struct ConfigData {
        std::string lang;
        std::string ui_backend;
        std::string buildtools;
        LLMConfig llm;

        // Defaults
        static constexpr std::string_view DEFAULT_UI_BACKEND = "tui";
        static constexpr std::string_view DEFAULT_LANG = "en";
        static constexpr std::string_view DEFAULT_MODEL = "deepseek-chat";
        static constexpr std::string_view DEFAULT_BUILDTOOLS = "xmake d2x-buildtools";
    };

private:
    ConfigData mData;

    Config() {
        // Set defaults first
        mData.ui_backend = std::string{ConfigData::DEFAULT_UI_BACKEND};
        mData.llm.model = std::string{ConfigData::DEFAULT_MODEL};

        // Load local config first
        auto local_path = get_config_path(Scope::Local);
        if (std::filesystem::exists(local_path)) {
            load_from_file(local_path);
        }

        // If not fully configured, try global config
        if (mData.ui_backend.empty() || !mData.llm.is_enabled()) {
            auto global_path = get_config_path(Scope::Global);
            if (std::filesystem::exists(global_path)) {
                merge_from_file(global_path);
            }
        }

        // Fill missing values from environment variables
        if (mData.lang.empty()) mData.lang = utils::get_env_or_default(EnvVars::D2X_LANG);
        if (mData.ui_backend.empty()) mData.ui_backend = utils::get_env_or_default(EnvVars::D2X_UI_BACKEND);
        if (mData.buildtools.empty()) mData.buildtools = utils::get_env_or_default(EnvVars::D2X_BUILDTOOLS);
        if (mData.llm.api_key.empty()) mData.llm.api_key = utils::get_env_or_default(EnvVars::D2X_LLM_API_KEY);
        if (mData.llm.api_url.empty()) mData.llm.api_url = utils::get_env_or_default(EnvVars::D2X_LLM_API_URL);
        if (mData.llm.model.empty()) mData.llm.model = utils::get_env_or_default(EnvVars::D2X_LLM_API_MODEL, "deepseek-chat");
        if (mData.llm.system_prompt.empty()) mData.llm.system_prompt = utils::get_env_or_default(EnvVars::D2X_LLM_SYSTEM_PROMPT);
    }

    void load_from_file(const std::string& path) {
        try {
            auto json = nlohmann::json::parse(std::ifstream(path));
            mData.lang = json.value("lang", "");
            mData.ui_backend = json.value("ui_backend", "");
            mData.buildtools = json.value("buildtools", "");

            // Load LLM config from nested "llm" object (new format) or flat keys (legacy)
            if (json.contains("llm") && json["llm"].is_object()) {
                auto& llm = json["llm"];
                mData.llm.api_key = llm.value("api_key", "");
                mData.llm.api_url = llm.value("api_url", "");
                mData.llm.model = llm.value("model", "");
                mData.llm.system_prompt = llm.value("system_prompt", "");
            } else {
                // Legacy flat format
                mData.llm.api_key = json.value("api_key", "");
                mData.llm.api_url = json.value("api_url", "");
                mData.llm.model = json.value("model", "");
                mData.llm.system_prompt = json.value("system_prompt", "");
            }
        } catch (...) {
            // Ignore parse errors
        }
    }

    void merge_from_file(const std::string& path) {
        try {
            auto json = nlohmann::json::parse(std::ifstream(path));
            if (mData.lang.empty()) mData.lang = json.value("lang", "");
            if (mData.ui_backend.empty()) mData.ui_backend = json.value("ui_backend", "");
            if (mData.buildtools.empty()) mData.buildtools = json.value("buildtools", "");

            if (json.contains("llm") && json["llm"].is_object()) {
                auto& llm = json["llm"];
                if (mData.llm.api_key.empty()) mData.llm.api_key = llm.value("api_key", "");
                if (mData.llm.api_url.empty()) mData.llm.api_url = llm.value("api_url", "");
                if (mData.llm.model.empty()) mData.llm.model = llm.value("model", "");
                if (mData.llm.system_prompt.empty()) mData.llm.system_prompt = llm.value("system_prompt", "");
            } else {
                // Legacy flat format
                if (mData.llm.api_key.empty()) mData.llm.api_key = json.value("api_key", "");
                if (mData.llm.api_url.empty()) mData.llm.api_url = json.value("api_url", "");
                if (mData.llm.model.empty()) mData.llm.model = json.value("model", "");
                if (mData.llm.system_prompt.empty()) mData.llm.system_prompt = json.value("system_prompt", "");
            }
        } catch (...) {
            // Ignore parse errors
        }
    }

    static Config& instance() {
        static Config inst;
        return inst;
    }

public:
    // Lang getters
    [[nodiscard]] static const std::string& lang() { return instance().mData.lang; }

    // UI getters
    [[nodiscard]] static const std::string& ui_backend() { return instance().mData.ui_backend; }

    // BuildTools getter
    [[nodiscard]] static const std::string& buildtools() { return instance().mData.buildtools; }

    // LLM getters
    [[nodiscard]] static const std::string& api_key() { return instance().mData.llm.api_key; }
    [[nodiscard]] static const std::string& api_url() { return instance().mData.llm.api_url; }
    [[nodiscard]] static const std::string& model() { return instance().mData.llm.model; }
    [[nodiscard]] static const std::string& system_prompt() { return instance().mData.llm.system_prompt; }
    [[nodiscard]] static bool is_llm_enabled() { return instance().mData.llm.is_enabled(); }

    static std::string get_config_path(Scope scope) {
        if (scope == Scope::Global) {
            return (std::filesystem::path(platform::get_home_dir()) / ".d2x.json").string();
        }
        return (std::filesystem::path(platform::get_rundir()) / ".d2x.json").string();
    }

    static void backup_config(const std::string& path) {
        try {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t_now);

            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
            std::string timestamp = oss.str();

            auto backup_path = path + ".error." + timestamp;
            std::filesystem::copy_file(path, backup_path);
            std::println("\n\033[93m已备份错误配置文件到: {}\033[0m", backup_path);
        } catch (...) {
            std::println("\n\033[91m警告: 备份配置文件失败\033[0m");
        }
    }

    static std::tuple<bool, ConfigData> load_config_with_error_handling(const std::string& path) {
        ConfigData cfg;
        bool has_error = false;

        if (!std::filesystem::exists(path)) {
            return {false, cfg};
        }

        try {
            auto json = nlohmann::json::parse(std::ifstream(path));
            cfg.lang = json.value("lang", "");
            cfg.ui_backend = json.value("ui_backend", "");
            cfg.buildtools = json.value("buildtools", "");

            // Load LLM config from nested "llm" object
            if (json.contains("llm") && json["llm"].is_object()) {
                auto& llm = json["llm"];
                cfg.llm.api_key = llm.value("api_key", "");
                cfg.llm.api_url = llm.value("api_url", "");
                cfg.llm.model = llm.value("model", "");
                cfg.llm.system_prompt = llm.value("system_prompt", "");
            } else {
                // Legacy flat format
                cfg.llm.api_key = json.value("api_key", "");
                cfg.llm.api_url = json.value("api_url", "");
                cfg.llm.model = json.value("model", "");
                cfg.llm.system_prompt = json.value("system_prompt", "");
            }
        } catch (const nlohmann::json::parse_error& e) {
            std::println("\n\033[91m错误: 配置文件解析失败: {}\033[0m", e.what());
            has_error = true;
        } catch (...) {
            std::println("\n\033[91m错误: 读取配置文件时发生未知错误\033[0m");
            has_error = true;
        }

        if (has_error) {
            backup_config(path);
            if (utils::ask_yes_no("配置文件损坏，是否重新创建?", true)) {
                return {true, ConfigData{}};
            }
        }

        return {false, cfg};
    }

    static void save_config(const std::string& path, const ConfigData& cfg) {
        nlohmann::json json;
        json["lang"] = cfg.lang;
        json["ui_backend"] = cfg.ui_backend;
        json["buildtools"] = cfg.buildtools;

        // Save LLM config to nested "llm" object
        nlohmann::json llm_json;
        llm_json["api_key"] = cfg.llm.api_key;
        llm_json["api_url"] = cfg.llm.api_url;
        llm_json["model"] = cfg.llm.model.empty() ? "deepseek-chat" : cfg.llm.model;
        llm_json["system_prompt"] = cfg.llm.system_prompt;
        json["llm"] = llm_json;

        std::ofstream file(path);
        if (file.is_open()) {
            file << json.dump(4);
            std::println("\n\033[92m配置已保存到: {}\033[0m", path);
        } else {
            std::println("\n\033[91m错误: 无法保存配置文件\033[0m");
        }
    }

    static void run_interactive_config() {
        utils::print_separator("d2x 配置向导");

        // Choose scope
        std::println("\n请选择配置范围:");
        std::println("  1. 全局配置 (用户目录: {})", platform::get_home_dir());
        std::println("  2. 本地配置 (当前目录: {})", platform::get_rundir());

        int choice = 0;
        while (choice != 1 && choice != 2) {
            std::print("请输入 (1/2): ");
            std::cout.flush();
            std::string input;
            if (std::getline(std::cin, input)) {
                try {
                    choice = std::stoi(input);
                } catch (...) {
                    choice = 0;
                }
            }
        }

        Scope scope = choice == 1 ? Scope::Global : Scope::Local;
        std::string config_path = get_config_path(scope);
        std::string scope_name = scope == Scope::Global ? "全局" : "本地";

        utils::print_separator(scope_name + "配置");

        // Load existing config with error handling
        auto [has_error, cfg] = load_config_with_error_handling(config_path);
        bool exists = std::filesystem::exists(config_path);

        if (exists && !has_error) {
            std::println("找到现有配置文件: {}", config_path);
            print_config(cfg);
            if (!utils::ask_yes_no("\n是否修改配置?", false)) {
                std::println("操作已取消。");
                return;
            }
        }

        // Collect new config
        ConfigData new_cfg;
        new_cfg.lang = utils::ask_input("Language (zh/en/auto)", cfg.lang.empty() ? std::string{ConfigData::DEFAULT_LANG} : cfg.lang);
        new_cfg.ui_backend = utils::ask_input("UI Backend", cfg.ui_backend.empty() ? std::string{ConfigData::DEFAULT_UI_BACKEND} : cfg.ui_backend);
        new_cfg.buildtools = utils::ask_input("BuildTools", cfg.buildtools.empty() ? std::string{ConfigData::DEFAULT_BUILDTOOLS} : cfg.buildtools);
        new_cfg.llm.api_key = utils::ask_input("LLM API Key", cfg.llm.api_key);
        // TODO: msvc cannot access llmapi::URL::DeepSeek?
        //new_cfg.llm.api_url = utils::ask_input("LLM API URL", cfg.llm.api_url.empty() ? std::string{llmapi::URL::DeepSeek} : cfg.llm.api_url);
        new_cfg.llm.api_url = utils::ask_input("LLM API URL", cfg.llm.api_url.empty() ? std::string{"https://api.deepseek.com/v1"} : cfg.llm.api_url);
        new_cfg.llm.model = utils::ask_input("LLM Model", cfg.llm.model.empty() ? std::string{ConfigData::DEFAULT_MODEL} : cfg.llm.model);
        new_cfg.llm.system_prompt = utils::ask_input("LLM System Prompt", cfg.llm.system_prompt);

        utils::print_separator("配置预览");
        print_config(new_cfg);

        if (utils::ask_yes_no("\n确认保存配置?", true)) {
            save_config(config_path, new_cfg);
        } else {
            std::println("操作已取消。");
        }
    }

    static void print_config(const ConfigData& cfg) {
        std::println("\n当前配置:");
        std::println("  lang:         {}", cfg.lang.empty() ? "(自动检测)" : cfg.lang);
        std::println("  ui_backend:   {}", cfg.ui_backend.empty() ? std::string{ConfigData::DEFAULT_UI_BACKEND} : cfg.ui_backend);
        std::println("  buildtools:   {}", cfg.buildtools.empty() ? std::string{ConfigData::DEFAULT_BUILDTOOLS} : cfg.buildtools);
        std::println("  llm.api_key:      {}", cfg.llm.api_key.empty() ? "(未设置)" : cfg.llm.api_key.substr(0, 10) + "...");
        std::println("  llm.api_url:      {}", cfg.llm.api_url.empty() ? "(未设置)" : cfg.llm.api_url);
        std::println("  llm.model:        {}", cfg.llm.model.empty() ? std::string{ConfigData::DEFAULT_MODEL} : cfg.llm.model);
        std::println("  llm.system_prompt: {}", cfg.llm.system_prompt.empty() ? "(未设置)" : cfg.llm.system_prompt.substr(0, 30) + "...");
    }
};

} // namespace d2x
