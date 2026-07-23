// d2x.protocol.transport — Provider 传输:以子进程形式驱动课程 Provider。
//
// d2x 唯一的向下扩展点。只有三个动词(describe / exercises / check),刻意
// 没有 build/run/test——那是编译型语言的形状,焊进通用框架就焊死了适用范围。
//
// 活性超时住在这里:自上次输出起超过 idle 时长即判定 Provider 挂死并终止,
// verdict 缺失走「无 verdict = fail + 原样呈现已收输出」的既有路径。
export module d2x.protocol.transport;

import std;
import d2x.json;
import d2x.protocol.types;
import d2x.protocol.codec;
import d2x.protocol.process;

namespace d2x::protocol {

// 一次 check 过程中的实时进度。宿主把它转发给前端,让学习者在编译时
// 就能看到输出,而不是等到全部结束才黑屏变白屏。
export struct Progress {
    std::function<void(std::string_view /*stage*/)> on_stage;
    std::function<void(std::string_view /*chunk*/)> on_output;
};

export class IExerciseProvider {
public:
    virtual ~IExerciseProvider() = default;

    // 自我描述。失败即致命——没有 Provider 就没有课程。
    virtual bool describe(std::string& name_out) = 0;

    virtual std::vector<Exercise> exercises() = 0;

    virtual Verdict check(const Exercise& ex, const Progress& progress) = 0;
};

// 练习 id 来自课程仓库的文件名,最终会拼进一条 shell 命令。带空格、引号
// 或 `;` 的文件名会让命令断开甚至注入。这里做单引号包裹(POSIX 语义:
// 单引号内除自身外一切字面化),内部单引号用 '\'' 转义。
//
// Windows 的 cmd.exe 不认单引号;那里用双引号包裹,且拒绝含引号的 id——
// 这类 id 本就不该出现在课程里。
export std::string shell_quote(std::string_view s) {
#ifdef _WIN32
    if (s.find('"') != std::string_view::npos) return {};   // 调用方视作非法
    return std::format("\"{}\"", s);
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += "'";
    return out;
#endif
}

export struct TransportOptions {
    // 活性超时:自上次输出起的最大静默时长;<=0 关闭。
    std::chrono::milliseconds idle_timeout{std::chrono::seconds(120)};
    // 协议层不依赖宿主日志——异常路径经回调上报(跳过的练习、超时终止等)。
    std::function<void(std::string)> on_warning;
};

// 以子进程形式驱动 Provider。命令来自宿主配置,参数走 argv。
export class ProcessProvider final : public IExerciseProvider {
    std::string      mCommand;
    TransportOptions mOpts;

    void warn(std::string msg) const {
        if (mOpts.on_warning) mOpts.on_warning(std::move(msg));
    }

    // 单次调用;逐行回调。返回运行状态(退出码 + 是否被活性超时终止)。
    process::RunStatus invoke(std::string_view args,
        const std::function<void(const nlohmann::json&)>& on_event) const {
        auto status = process::run_lines_idle(
            std::format("{} {}", mCommand, args), mOpts.idle_timeout,
            [&](std::string_view line) {
                // 非 JSON 行静默丢弃:启动器噪声是预期内的常态,不是异常。
                if (auto ev = codec::parse_event(line)) on_event(*ev);
            });
        if (status.idle_killed) {
            warn(std::format(
                "provider idle-timeout: no output for {}s while running '{} {}' — killed",
                std::chrono::duration_cast<std::chrono::seconds>(mOpts.idle_timeout).count(),
                mCommand, args));
        }
        return status;
    }

public:
    explicit ProcessProvider(std::string command, TransportOptions opts = {})
        : mCommand(std::move(command)), mOpts(std::move(opts)) {}

    bool describe(std::string& name_out) override {
        bool seen = false;
        auto status = invoke("describe", [&](const nlohmann::json& ev) {
            if (ev.value("event", "") != "describe") return;
            name_out = ev.value("name", "unknown");
            seen = true;
        });
        return seen && status.exit_code == 0;
    }

    std::vector<Exercise> exercises() override {
        std::vector<Exercise> found;
        invoke("exercises", [&](const nlohmann::json& ev) {
            if (ev.value("event", "") != "exercise") return;

            Exercise ex;
            ex.id      = ev.value("id", "");
            ex.order   = ev.value("order", 0);
            ex.title   = ev.value("title", ex.id);
            ex.chapter = ev.value("chapter", "");
            if (ev.contains("files") && ev["files"].is_array()) {
                for (const auto& f : ev["files"]) {
                    if (f.is_string()) ex.files.push_back(f.get<std::string>());
                }
            }

            // 没有 id 的练习无法持久化状态;没有文件的练习会让「打开编辑器」
            // 和「监听变更」都失去目标。
            if (ex.id.empty()) {
                warn("provider returned an exercise without id, skipped");
                return;
            }
            if (ex.files.empty()) {
                warn(std::format("exercise '{}' has no files, skipped", ex.id));
                return;
            }
            found.push_back(std::move(ex));
        });

        std::ranges::sort(found, {}, &Exercise::order);
        return found;
    }

    Verdict check(const Exercise& ex, const Progress& progress) override {
        Verdict verdict;
        bool got_verdict = false;
        std::string collected;

        auto quoted = shell_quote(ex.id);
        if (quoted.empty()) {
            verdict.outcome = Outcome::Fail;
            verdict.output  = std::format(
                "exercise id '{}' contains characters that cannot be passed safely to the provider",
                ex.id);
            return verdict;
        }

        auto status = invoke(std::format("check {}", quoted),
                             [&](const nlohmann::json& ev) {
            auto kind = ev.value("event", "");

            if (kind == "stage") {
                auto name = ev.value("name", "");
                verdict.stage = name;
                if (progress.on_stage) progress.on_stage(name);

            } else if (kind == "output") {
                auto chunk = ev.value("chunk", "");
                collected += chunk;
                if (progress.on_output) progress.on_output(chunk);

            } else if (kind == "verdict") {
                if (auto o = outcome_from(ev.value("outcome", ""))) {
                    verdict.outcome = *o;
                    got_verdict = true;
                }
                verdict.stage = ev.value("stage", verdict.stage);
                if (ev.contains("diagnostics") && ev["diagnostics"].is_array()) {
                    for (const auto& d : ev["diagnostics"]) {
                        verdict.diagnostics.push_back(Diagnostic{
                            .file     = d.value("file", ""),
                            .line     = d.value("line", 0),
                            .col      = d.value("col", 0),
                            .severity = d.value("severity", "error"),
                            .message  = d.value("message", ""),
                        });
                    }
                }

            } else if (kind == "error") {
                collected += ev.value("message", "");
                collected += '\n';
            }
        });

        verdict.output = std::move(collected);

        // 没收到 verdict 事件 = Provider 中途死了、输出被截断、或被活性
        // 超时终止。绝不能当成通过——原始输出原样交给学习者,让他看见真相。
        if (!got_verdict) {
            verdict.outcome = Outcome::Fail;
            if (status.idle_killed) {
                verdict.output += std::format(
                    "\n[d2x] provider produced no output for {}s and was terminated",
                    std::chrono::duration_cast<std::chrono::seconds>(mOpts.idle_timeout).count());
            } else {
                // 无论是否已有部分输出都要附上原因——只给「真相的前半段」
                // 会让 fail 看起来毫无来由。
                if (!verdict.output.empty()) verdict.output += '\n';
                verdict.output += std::format(
                    "[d2x] provider did not report a verdict for '{}'", ex.id);
            }
        }
        return verdict;
    }
};

} // namespace d2x::protocol
