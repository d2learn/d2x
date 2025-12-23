export module d2x.checker;

import std;

import d2x.log;
import d2x.utils;
import d2x.console;
import d2x.buildtools;

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

export void run() {

    btools.load_targets();

    auto targets = btools.get_targets();

    int total_targets = targets.size();
    int built_targets = 0;

    if (total_targets == 0) {
        log::warning("No targets found for checking.");
        return;
    }

    for (const auto& target : targets) {
        log::info("Checking target: {}", target);
        
        bool build_success { false };
        bool status { false };
        auto output = std::string { };

        while (!build_success) {

            std::tie(build_success, output) = build_with_error_handling(target);

            if (build_success) {
                std::tie(build_success, output) = run_with_error_handling(target);
            }

            status = build_success;

            if (!output.empty()) {
                if (output.find("❌") != std::string::npos) {
                    status = false;
                    build_success = false;
                } else if (output.find(D2X_WAIT) != std::string::npos) {
                    build_success = false;
                }
            }
            
            if (build_success) {
                built_targets += 1;
            } else {
                // TODO: open editor...
            }

            auto files = btools.get_files_for(target);

            console::print(
                target, files,
                built_targets, total_targets,
                output, status
            );

            utils::wait_files_changed(files, 10 * 1000);
        }
    }

    log::info("Checker finished.");
}

} // namespace checker
} // namespace d2x
