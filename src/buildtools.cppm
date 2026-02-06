export module d2x.buildtools;

import std;

import d2x.config;
import d2x.platform;
import d2x.utils;
import d2x.log;

namespace d2x {
/*
xxx d2x-buildtools [command] [target]

Commands:
    list        List all targets
    build       Build specified target
    run         Run specified target
*/

class BuildTools {
    std::map<std::string, std::vector<std::string>> targets;
    std::string bin = "d2x-buildtools";
public:
    BuildTools(std::string bin) : bin(std::move(bin)) {}
    ~BuildTools() {}

public: // commands
    auto init() const {
        return platform::run_command_capture(bin + " init");
    }

    auto list() const {
        return platform::run_command_capture(bin + " list");
    }

    auto build(const std::string& target) const {
        return platform::run_command_capture(bin + " build " + target);
    }

    auto run(const std::string& target) const {
        return platform::run_command_capture(bin + " run " + target);
    }

public: // get/set
    std::vector<std::string> get_targets() const {
        std::vector<std::string> keys;
        for (const auto& [key, _] : targets) {
            keys.push_back(key);
        }
        return keys;
    }

    std::vector<std::string> get_files_for(const std::string& target) const {
        if (targets.contains(target)) {
            return targets.at(target);
        }
        return {};
    }

public:

    void print_targets() {
        if (targets.empty()) {
            log::warning("No targets found.");
            return;
        }
        for (const auto& [target, files] : targets) {
            std::println("Target: {}", target);
            for (const auto& file : files) {
                std::println("  - {}", file);
            }
        }
    }

public:
    void load_targets() {
        auto [exit_code, output] = list();
        // Parse output to populate targets map
        // { "target1": ["src/main.cpp", "src/util.cpp"], "target2": ["src/app.cpp"]
        // output format assumed to be:
        // target: src/main.cpp, src/util.cpp
        if (exit_code != 0) {
            std::println("Failed to load targets with exit code: {}", exit_code);
            return;
        }

        //std::println("Buildtools output:\n{}", output);

        auto lines = d2x::utils::split_string(output, '\n');
        for (const auto& line : lines) {
            auto parts = d2x::utils::split_string(line, '@');
            if (parts.size() != 2) continue;
            auto target_name = d2x::utils::trim_string(parts[0]);
            auto files_str = d2x::utils::trim_string(parts[1]);
            auto file_list = d2x::utils::split_string(files_str, ',');
            for (const auto& file : file_list) {
                targets[target_name].push_back(d2x::utils::trim_string(file));
            }
        }
    }
}; // class BuildTools

export BuildTools load_buildtools() {
    std::string bin = Config::buildtools();
    if (bin.empty()) {
        bin = "xmake d2x-buildtools";
    }
    BuildTools bt(bin);
    bt.init();
    return bt;
}

} // namespace d2x
