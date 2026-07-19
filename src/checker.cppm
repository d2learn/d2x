// 学习循环的编排。
//
// 这里只做编排：从 Provider 拿练习、驱动 Session 推进、往 EventSink 发事件、
// 失败时等文件变更再重试。
//
// 它不知道怎么编译、不知道怎么判定通过（那是 Provider 的事），
// 也不知道页面长什么样（那是 EventSink 背后的前端的事）。
export module d2x.checker;

import std;

import d2x.log;
import d2x.utils;
import d2x.ui;
import d2x.config;
import d2x.domain;
import d2x.provider;
import d2x.session;
import d2x.emit;
import d2x.assistant;
import d2x.editor;
import d2x.platform;
import d2x.watch;

namespace d2x {
namespace checker {

using domain::Outcome;

// 学员多久没动文件就重新跑一轮（毫秒）。即使没有编辑也定期重试，
// 这样外部因素（依赖装好了、磁盘腾出空间了）不至于让人卡死。
constexpr int kIdlePollMs = 20 * 1000;

std::filesystem::path state_path() {
    return std::filesystem::path(platform::get_rundir()) / ".d2x" / "state.json";
}

// 读取学员正在编辑的源码。练习可能挂多个文件，AI 助手看第一个即可 ——
// 但读不到不该让整个会话崩掉（旧实现在这里无保护地抛异常）。
std::string read_source(const std::vector<std::string>& files) {
    if (files.empty()) return {};
    try {
        return utils::read_file_to_string(files.front());
    } catch (const std::exception& e) {
        log::warning("读取练习文件失败: {}", e.what());
        return {};
    }
}

export void run(const std::string& start_target = "", bool emit_events = false) {

    auto command = Config::buildtools();
    if (command.empty()) {
        log::error("未配置 buildtools —— 请在 .d2x.json 里指定 Provider 命令");
        log::error("例如: \"buildtools\": \"mcpp run -q -p d2x/buildtools/mcpp --\"");
        return;
    }

    // 内置前端走内存通道，外部客户端走管道，二者消费同一套事件类型。
    std::unique_ptr<emit::IEventSink> sink;
    if (emit_events) {
        // stdout 交给协议流，日志改道 stderr —— 外部客户端就能直接
        // 逐行 JSON.parse，不必先过滤噪声。
        log::to_stderr(true);
        sink = std::make_unique<emit::StdoutSink>();
    } else {
        sink = std::make_unique<emit::UiSink>();
    }

    auto provider = provider::ProcessProvider(command);

    // Provider 起不来是致命错误，且必须说清楚是「Provider 挂了」而不是
    // 「没有练习」。旧实现先打 "Failed to load targets with exit code"
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

        sink->session(static_cast<int>(session.total()),
                      static_cast<int>(session.completed_count()),
                      exercise.id);
        sink->exercise(exercise);

        bool opened_editor = false;
        auto watcher = watch::FileWatcher(exercise.files);

        for (;;) {
            // 每次重试都重读源码：学员刚改过，AI 助手要看到最新版本
            auto source = read_source(exercise.files);
            assistant.set_original_code(source);

            // 边跑边发。旧实现读到 EOF 才刷新，编译期间全程黑屏。
            auto progress = provider::Progress{
                .on_stage  = [&](std::string_view s) { sink->stage(s); },
                .on_output = [&](std::string_view c) { sink->output(c); },
            };

            auto verdict = provider.check(exercise, progress);
            sink->verdict(verdict);

            if (verdict.outcome == Outcome::Pass) break;

            // Blocked 与 Fail 都不推进、都等学员改文件；区别只在前端怎么呈现，
            // 而那已经由 verdict 事件里的 outcome 表达了。
            if (!opened_editor) {
                for (const auto& file : exercise.files) editor::open(file);
                opened_editor = true;
            }

            sink->hint(assistant.ask(source, verdict.output));
            sink->waiting("file-change");

            // 构建刚跑完，先把当前文件状态当作基线 —— 否则构建期间的任何
            // 落盘都会被当成学员的编辑（自触发保护）。去抖在 FileWatcher
            // 内部完成，调用方不再需要连着调两次。
            watcher.resync();
            watcher.wait_for_change(std::chrono::milliseconds{kIdlePollMs});
        }

        session.complete_current();
    }

    sink->session(static_cast<int>(session.total()),
                  static_cast<int>(session.completed_count()), "");
    sink->done();
    log::info("全部练习已完成 🎉");
}

} // namespace checker
} // namespace d2x
