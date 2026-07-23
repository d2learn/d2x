// UI 消息目录:界面文案随 lang 切换(zh/en),终结中英混杂。
//
// 只收录面向学习者的句子;日志(log::*)与协议流不经过这里。
// 键控查表而非散落的三元表达式——新增语言 = 加一列,不是改一片。
export module d2x.msg;

import std;
import d2x.config;

namespace d2x::msg {

export enum class Key {
    StatusChecking,
    StatusPass,
    StatusFail,
    StatusBlocked,
    OutputOmitted,      // {0}=省略行数 {1}=完整输出路径
    WaitingForChange,
    AiDisabled,
    AllDone,
};

struct Entry { std::string_view zh; std::string_view en; };

constexpr Entry table(Key k) {
    switch (k) {
        case Key::StatusChecking:  return {"⏳ 检测中...", "⏳ checking..."};
        case Key::StatusPass:      return {"✅ 通过", "✅ passed"};
        case Key::StatusFail:      return {"❌ 未通过(按下方提示修复后保存即可自动重测)",
                                           "❌ failed (fix per hints below; saving re-checks automatically)"};
        case Key::StatusBlocked:   return {"🚧 检查已全部通过——删除 d2x::wait() 路障即可完成本题",
                                           "🚧 all checks passed — remove the d2x::wait() barrier to finish"};
        case Key::OutputOmitted:   return {"… 中间省略 {} 行,完整输出: {}",
                                           "… {} lines omitted, full output: {}"};
        case Key::WaitingForChange:return {"等待文件修改…", "waiting for file changes…"};
        case Key::AiDisabled:      return {"AI 助手未启用(配置 llm.api_key 后开启)",
                                           "AI assistant disabled (set llm.api_key to enable)"};
        case Key::AllDone:         return {"全部练习已完成 🎉", "all exercises completed 🎉"};
    }
    return {"", ""};
}

export std::string_view text(Key k) {
    auto e = table(k);
    return Config::lang() == "zh" ? e.zh : e.en;
}

} // namespace d2x::msg
