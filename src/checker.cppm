export module d2x.checker;

import std;

import d2x.log;
import d2x.utils;
import d2x.ui;
import d2x.buildtools;
import d2x.assistant;
import d2x.editor;

namespace d2x {
namespace checker {

// detect flag
constexpr std::string D2X_WAIT = "D2X_WAIT";
static auto btools = d2x::load_buildtools();

std::pair<bool, std::string> build_with_error_handling(const std::string &target) {
    auto [exit_code, output] = btools.build(target);
    return std::make_pair(exit_code == 0, output);
}

std::pair<bool, std::string> run_with_error_handling(const std::string &target) {
    auto [exit_code, output] = btools.run(target);
    return std::make_pair(exit_code == 0, output);
}

export void run(const std::string& start_target = "") {

    btools.load_targets();

    auto targets = btools.get_targets();

    int total_targets = targets.size();
    int built_targets = 0;

    if (total_targets == 0) {
        log::warning("No targets found for checking.");
        return;
    }

    // 如果指定了起始target，找到第一个匹配的位置
    std::size_t start_idx = 0;
    if (!start_target.empty()) {
        bool found = false;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            if (targets[i].find(start_target) != std::string::npos) {
                start_idx = i;
                found = true;
                log::info("Starting from target: {}", targets[i]);
                break;
            }
            built_targets += 1; // skip targets before the matched one
        }
        if (!found) {
            built_targets = 0; // reset if not found
            log::warning("Target '{}' not found. Starting from beginning.", start_target);
        }
    }

    auto assistant = d2x::Assistant();

    for (std::size_t idx = start_idx; idx < targets.size(); ++idx) {
        const auto& target = targets[idx];
        //log::info("Checking target: {}", target);
        
        bool build_success { false };
        bool status { false };
        auto output = std::string { };
        bool open_target_file { false };

        auto files = btools.get_files_for(target);
        // read original code from files[0]
        auto original_code = utils::read_file_to_string(files[0]);

        assistant.set_original_code(original_code);

        while (!build_success) {

            std::tie(build_success, output) = build_with_error_handling(target);

            if (build_success) {
                std::tie(build_success, output) = run_with_error_handling(target);
            }

            status = build_success;

            if (!output.empty()) {
                if (
                    output.find("❌") != std::string::npos
                    || output.find("error") != std::string::npos
                ) {
                    status = false;
                    build_success = false;
                } else if (output.find(D2X_WAIT) != std::string::npos) {
                    build_success = false;
                }
            }

            if (build_success) {
                built_targets += 1;
            } else if (!open_target_file) {
                // Open file in editor on first failure
                for (const auto& file : files) {
                    editor::open(file);
                }
                open_target_file = true;
            }

            // ask ai assistant for tips
            // ask ai assistant for tips
            auto ecode = utils::read_file_to_string(files[0]);
            //auto ai_tips = assistant.ask(ecode, output);
            auto ai_tips = assistant.ask(ecode, output);

            ui::update_checker_page(
                target, files,
                built_targets, total_targets,
                output, status,
                ai_tips
            );

            if (!build_success) {
                utils::wait_files_changed(files, 20 * 1000);
                // wait user action to change files to avoid shaking
                while (utils::wait_files_changed(files, 1 * 1000));
            }
        }
    }

    log::info("Checker finished.");
}

} // namespace checker
} // namespace d2x
