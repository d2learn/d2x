// d2x.protocol.codec — 双向协议的 NDJSON 编解码。
//
// 解码端(d2x core 消费):每行一个 JSON 对象,解析不了的行忽略——这不是
// 宽容,是协议设计的一部分:Provider 常经由启动器(如 `mcpp run`)间接执行,
// 启动器会往 stdout 混入空行乃至编译输出,忽略非 JSON 行让噪声天然失效。
//
// 编码端(课程侧 Provider 可选复用):下行事件的标准序列化。d2x 的 parse 端
// 与课程侧的 emit 端此前是两份手写实现,靠文档同步——双真相源。接入本模块
// 后归一;不接入的 Provider(任意语言)以协议文档 + conformance 套件为准。
export module d2x.protocol.codec;

import std;
import d2x.json;
import d2x.protocol.types;

namespace d2x::protocol::codec {

// ── 解码 ────────────────────────────────────────────────────────────
export std::optional<nlohmann::json> parse_event(std::string_view line) {
    auto begin = line.find('{');
    if (begin == std::string_view::npos) return std::nullopt;
    auto candidate = line.substr(begin);
    auto parsed = nlohmann::json::parse(candidate, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
    return parsed;
}

// ── 编码(下行事件,一行一个,调用方负责逐行输出并 flush)───────────────
export std::string describe_event(std::string_view name, int protocol_version) {
    nlohmann::json ev{{"event", "describe"}, {"protocol", protocol_version},
                      {"name", name}};
    return ev.dump();
}

export std::string exercise_event(const Exercise& ex) {
    nlohmann::json ev{{"event", "exercise"}, {"id", ex.id}, {"order", ex.order},
                      {"title", ex.title}, {"chapter", ex.chapter},
                      {"files", ex.files}};
    return ev.dump();
}

export std::string stage_event(std::string_view name) {
    return nlohmann::json{{"event", "stage"}, {"name", name}}.dump();
}

export std::string output_event(std::string_view chunk) {
    return nlohmann::json{{"event", "output"}, {"chunk", chunk}}.dump();
}

export std::string error_event(std::string_view message) {
    return nlohmann::json{{"event", "error"}, {"message", message}}.dump();
}

export std::string verdict_event(const Verdict& v, int exit_code) {
    nlohmann::json diags = nlohmann::json::array();
    for (const auto& d : v.diagnostics) {
        diags.push_back({{"file", d.file}, {"line", d.line}, {"col", d.col},
                         {"severity", d.severity}, {"message", d.message}});
    }
    return nlohmann::json{{"event", "verdict"}, {"outcome", to_string(v.outcome)},
                          {"stage", v.stage}, {"exit_code", exit_code},
                          {"diagnostics", diags}}.dump();
}

} // namespace d2x::protocol::codec
