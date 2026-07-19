// d2x 的领域模型。纯数据，零依赖——不认识构建工具，不认识 UI。
//
// 注意用词：这里是 Exercise（练习），不是 target。target 是构建工具的词汇，
// 让它泄漏进领域层正是旧设计的问题所在。d2x 是课程工具，它的领域对象是练习。
export module d2x.domain;

import std;

namespace d2x::domain {

export struct Exercise {
    std::string              id;        // 稳定标识，完成状态按它持久化
    int                      order{};   // 显式顺序
    std::string              title;
    std::string              chapter;
    std::vector<std::string> files;     // 学员编辑的文件，绝对路径
};

// 三态。Blocked 表示「学员代码已经正确，但还有一个显式路障没拆」
// （d2mcpp 里是 D2X_WAIT 宏）——它既不是失败，也不该前进。
// 旧实现把它塞进 build_success=false 而 status 仍为 true，
// UI 上显示成「成功但卡住」，语义是错的。
export enum class Outcome { Pass, Fail, Blocked };

export std::string_view to_string(Outcome o) {
    switch (o) {
        case Outcome::Pass:    return "pass";
        case Outcome::Fail:    return "fail";
        case Outcome::Blocked: return "blocked";
    }
    return "fail";
}

export std::optional<Outcome> outcome_from(std::string_view s) {
    if (s == "pass")    return Outcome::Pass;
    if (s == "fail")    return Outcome::Fail;
    if (s == "blocked") return Outcome::Blocked;
    return std::nullopt;
}

export struct Diagnostic {
    std::string file;
    int         line{};
    int         col{};
    std::string severity;   // "error" | "warning" | "note"
    std::string message;
};

export struct Verdict {
    Outcome                 outcome{Outcome::Fail};
    std::string             stage;        // Provider 自定义："compile" / "run" / "lint"
    std::string             output;       // 给学员看的原始输出，保留 ANSI
    std::vector<Diagnostic> diagnostics;
};

} // namespace d2x::domain
