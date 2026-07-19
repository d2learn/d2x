// 学习循环的编排。
//
// 这里只做编排：从 Provider 拿练习、驱动 Session 推进、把事件转给 UI、
// 失败时等文件变更再重试。它不知道怎么编译、不知道怎么判定通过——
// 那些是 Provider 的事（见 d2x.provider）。
export module d2x.checker;

import std;

import d2x.log;
import d2x.utils;
import d2x.ui;
import d2x.config;
import d2x.domain;
import d2x.provider;
import d2x.session;
import d2x.assistant;
import d2x.editor;
import d2x.platform;

namespace d2x {
namespace checker {

using domain::Outcome;

// 学员多久没动文件就重新轮询一次（毫秒）
constexpr int kIdlePollMs  = 20 * 1000;
// 连续变更的合并窗口，避免编辑器保存一次触发多轮重建
constexpr int kSettleMs    = 1 * 1000;

std::filesystem::path state_path() {
    return std::filesystem::path(platform::get_rundir()) / ".d2x" / "state.json";
}

export void run(const std::string& start_target = "") {

    auto command = Config::buildtools();
    if (command.empty()) {
        log::error("未配置 buildtools —— 请在 .d2x.json 里指定 Provider 命令");
        return;
    }

    auto provider = provider::ProcessProvider(command);

    // Provider 起不来是致命错误，且必须说清楚是「Provider 挂了」而不是
    // 「没有练习」。旧实现在这里先打 "Failed to load targets with exit code"
    // 再打 "No targets found for checking."，两层都在误导学员。
    std::string provider_name;
    if (!provider.describe(provider_name)) {
        log::error("Provider 无响应: {}", command);
        log::error("请确认该命令可执行，且实现了 d2x Provider 协议（describe/exercises/check）");
        return;
    }
    log::info("Provider: {}", provider_name);

    auto exercises = provider.exercises();
    if (exercises.empty()) {
        log::warning("Provider '{}' 没有返回任何练习", provider_name);
        return;
    }

    auto state   = session::StateStore(state_path());
    auto session = session::Session(exercises, &state);
    session.seek_start(start_target);
    session.enter_current();

    auto assistant = d2x::Assistant();

    while (!session.done()) {
        const auto& exercise = session.current();

        bool opened_editor = false;

        for (;;) {
            // 每次重试都重读源码：学员刚改过，AI 助手要看到最新版本
            auto source = utils::read_file_to_string(exercise.files.front());
            assistant.set_original_code(source);

            // 边跑边显示。旧实现读到 EOF 才刷新，编译期间全程黑屏。
            std::string streamed;
            auto progress = provider::Progress{
                .on_stage  = [&](std::string_view stage) {
                    ui::update_checker_page(
                        exercise.id, exercise.files,
                        static_cast<int>(session.completed_count()),
                        static_cast<int>(session.total()),
                        std::format("[{}]\n{}", stage, streamed),
                        false, "");
                },
                .on_output = [&](std::string_view chunk) {
                    streamed += chunk;
                },
            };

            auto verdict = provider.check(exercise, progress);

            if (verdict.outcome == Outcome::Pass) {
                ui::update_checker_page(
                    exercise.id, exercise.files,
                    static_cast<int>(session.completed_count()) + 1,
                    static_cast<int>(session.total()),
                    verdict.output, true, "");
                break;
            }

            // Blocked 与 Fail 的区别只在提示语：前者代码已对、只差拆路障，
            // 两者都不推进，都等学员改文件。
            if (!opened_editor) {
                for (const auto& file : exercise.files) editor::open(file);
                opened_editor = true;
            }

            auto tips = assistant.ask(source, verdict.output);

            ui::update_checker_page(
                exercise.id, exercise.files,
                static_cast<int>(session.completed_count()),
                static_cast<int>(session.total()),
                verdict.output,
                verdict.outcome == Outcome::Blocked,
                tips);

            utils::wait_files_changed(exercise.files, kIdlePollMs);
            // 合并连续保存，避免编辑器一次保存触发多轮重建
            while (utils::wait_files_changed(exercise.files, kSettleMs));
        }

        session.complete_current();
    }

    log::info("全部练习已完成 🎉");
}

} // namespace checker
} // namespace d2x
