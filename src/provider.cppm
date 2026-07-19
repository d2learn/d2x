// Provider 抽象：d2x 唯一的向下扩展点。
//
// d2x 不知道 C++、不知道 mcpp、不知道怎么编译任何东西。它只知道怎么问
// 一个 Provider 要练习列表、怎么让它验证一道练习。具体课程各自实现。
//
// 只有三个动词，刻意没有 build/run/test —— 那是编译型语言的形状，焊进
// 通用框架就焊死了适用范围。一次 check 内部编译几次、跑不跑测试、判定
// 看退出码还是看输出，全是课程的事。
export module d2x.provider;

import std;

import d2x.domain;
import d2x.platform;
import d2x.log;
import d2x.json;

namespace d2x::provider {

using domain::Exercise;
using domain::Verdict;
using domain::Outcome;
using domain::Diagnostic;

// 一次 check 过程中的实时进度。d2x 把它转发给前端，让学员在编译时
// 就能看到输出，而不是等到全部结束才黑屏变白屏。
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

// ── NDJSON 事件流解析 ───────────────────────────────────────────────
//
// 每一行都应是一个 JSON 对象；解析不了的行直接忽略。这不是宽容，而是
// 协议设计的一部分：Provider 常常经由启动器（如 `mcpp run`）间接执行，
// 启动器会往 stdout 混入空行乃至编译输出。忽略非 JSON 行让这些噪声
// 天然失效，于是不再需要哨兵前缀或「末行即 JSON」这类隐式约定。
std::optional<nlohmann::json> parse_event(std::string_view line) {
    auto begin = line.find('{');
    if (begin == std::string_view::npos) return std::nullopt;
    auto candidate = line.substr(begin);
    auto parsed = nlohmann::json::parse(candidate, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
    return parsed;
}

// 练习 id 来自课程仓库的文件名，最终会拼进一条 shell 命令。带空格、引号
// 或 `;` 的文件名会让命令断开甚至注入。这里做单引号包裹（POSIX 语义：
// 单引号内除自身外一切字面化），内部单引号用 '\'' 转义。
//
// Windows 的 cmd.exe 不认单引号，但 d2x 走 _popen 时命令实际交给 cmd /c；
// 那里用双引号包裹，且不存在 '\'' 这种拼接技巧 —— 所以退而求其次：拒绝
// 含引号的 id，其余用双引号包裹。这类 id 本就不该出现在课程里。
std::string shell_quote(std::string_view s) {
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

// 以子进程形式驱动 Provider。命令来自配置，参数走 argv。
export class ProcessProvider final : public IExerciseProvider {
    std::string mCommand;

    // 单次调用；逐行回调。返回子进程真实退出码。
    int invoke(std::string_view args,
               const std::function<void(const nlohmann::json&)>& on_event) const {
        return platform::run_command_lines(
            std::format("{} {}", mCommand, args),
            [&](std::string_view line) {
                // 非 JSON 行静默丢弃：启动器噪声是预期内的常态，不是异常。
                if (auto ev = parse_event(line)) on_event(*ev);
            });
    }

public:
    explicit ProcessProvider(std::string command) : mCommand(std::move(command)) {}

    bool describe(std::string& name_out) override {
        bool seen = false;
        int code = invoke("describe", [&](const nlohmann::json& ev) {
            if (ev.value("event", "") != "describe") return;
            name_out = ev.value("name", "unknown");
            seen = true;
        });
        return seen && code == 0;
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

            // 没有 id 的练习无法持久化状态；没有文件的练习会让「打开编辑器」
            // 和「监听变更」都失去目标——旧实现在这里直接 files[0] 崩溃。
            if (ex.id.empty()) {
                log::warning("provider returned an exercise without id, skipped");
                return;
            }
            if (ex.files.empty()) {
                log::warning("exercise '{}' has no files, skipped", ex.id);
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

        invoke(std::format("check {}", quoted), [&](const nlohmann::json& ev) {
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
                if (auto o = domain::outcome_from(ev.value("outcome", ""))) {
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

        // 没收到 verdict 事件 = Provider 中途死了或输出被截断。
        // 绝不能当成通过——把原始输出原样交给学员，让他看见真相。
        if (!got_verdict) {
            verdict.outcome = Outcome::Fail;
            if (verdict.output.empty()) {
                verdict.output = std::format(
                    "provider did not report a verdict for '{}'", ex.id);
            }
        }
        return verdict;
    }
};

} // namespace d2x::provider
