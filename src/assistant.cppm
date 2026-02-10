export module d2x.assistant;

import std;
import mcpplibs.llmapi;

import d2x.ui;
import d2x.log;
import d2x.platform;
import d2x.config;

namespace d2x {

using namespace mcpplibs;

// 系统提示模板，使用constexpr string_view避免运行时字符串构造
std::string system_prompt = R"(
背景: 你是一个现代C++代码和教育专家

任务:
通过对比学生修改后的代码和原始练习代码(故意有错误),
再结合编译器报错和运行信息, 用提示的方式帮助学生修复代码并完成练习(符合原始代码意图)

输出要求:
- 0.如果学生逃避删除代码或运行时检测/断言, 请用幽默的方式提醒他
- 2.用引导式的方式帮助学生发现并修复代码中的错误, 而不是直接给出答案
- 3.要极大程度的给予学生情绪支持、鼓励和正向反馈
- 4.每次回答控制在20到100字之间
- 5.给相同的代码多次提问时, 尽量给出不同引导思路, 但始终不要直接给出答案
- 6.回答中不要包含代码片段, 也不要包含请求多少次等信息
)";


constexpr std::string_view system_prompt_template = R"(
回答风格:
- 风格1(默认使用): 用时而可爱、时而傲娇、时而生气的方式回答
- 风格2: 夸张嘲讽风格，但仍要给可执行提示; 不得歧视或人身攻击

原始练习代码:
{}

用{}语言进行回答
)";

constexpr std::string_view user_prompt_template = R"(
当前练习代码:
{}

编译器报错和运行信息:
{}
)";



export class Assistant {
    std::optional<llmapi::Client> mLLMClient;

    // 流式请求状态
    mutable std::mutex mAnswerMutex;
    std::string mCurrentAnswer;
    std::string mCurrentQuestion;
    std::atomic<bool> mIsProcessing;
    std::jthread mRequestThread;
    std::string mLanguage;
    int mNeedHelpCount;
    bool mEnable;

public:
    explicit Assistant() : mCurrentAnswer(),
        mCurrentQuestion("NULL"),
        mIsProcessing(false),
        mRequestThread(),
        mLanguage("en"),
        mNeedHelpCount(0),
        mEnable(Config::is_llm_enabled())
    {
        if (mEnable) {
            mLLMClient.emplace(Config::api_key(), Config::api_url());
            log::info("Initialized Assistant with API URL: {}", Config::api_url());

            mLLMClient->model(Config::model());
            log::info("Using model: {}", Config::model());
        } else {
            log::info("Assistant is disabled (no LLM API key configured).");
        }

        mLanguage = platform::get_system_language();
        std::string config_lang = Config::lang();
        if (!config_lang.empty() && config_lang != "auto") {
            log::info("Using language from config: {}", config_lang);
            mLanguage = config_lang;
        }
        if (mLanguage.contains("zh")) {
            mLanguage = "中文";
        } else {
            mLanguage = "English(英语)";
        }
        log::info("System language detected: {}", mLanguage);

        // Override system prompt from config/env
        if (!Config::system_prompt().empty()) {
            system_prompt = Config::system_prompt();
            log::info("Overriding system prompt from configuration.");
        }

        log::info("Assistant is {}enabled.", mEnable ? "" : "not ");

        // sleep 1 second show info for user
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    Assistant(const Assistant&) = delete;
    Assistant& operator=(const Assistant&) = delete;
    Assistant(Assistant&&) noexcept = delete;
    Assistant& operator=(Assistant&&) noexcept = delete;

    ~Assistant() {
        stop_current_request();
    }

public:
    void set_original_code(std::string_view original_code) {
        if (!mEnable || !mLLMClient.has_value()) return;

        mLLMClient->clear();

        //log::info("Setting original code context for Assistant.");

        // system prompt: base + formatted template
        std::string formatted_template = std::vformat(
            system_prompt_template,
            std::make_format_args(original_code, mLanguage)
        );

        std::string full_prompt = system_prompt + formatted_template;

        mLLMClient->system(std::move(full_prompt));
    }

    std::string ask(std::string ecode, std::string output) {
        if (!mEnable || !mLLMClient.has_value()) return "AI Disabled / 未启用 - https://github.com/d2learn/d2x";

        std::string question = std::vformat(
            user_prompt_template,
            std::make_format_args(ecode, output)
        );

        if (mCurrentQuestion != question) {
            mCurrentAnswer.clear();
            //log::info("Assistant received new question, starting request.");
            start_request(std::move(question), true);
            mNeedHelpCount = 0;
            return "..."; // optimize for immediate return
        } else if (!is_processing() && mNeedHelpCount < 4) {
            /*
            mNeedHelpCount++;
            if (mNeedHelpCount >= 2) {
                //start_request("使用风格1: 进行鼓励一下", false);
            } else {
                //start_request("使用风格2: 进行疯狂嘲讽", false);
            }
            //std::println("Assistant is re-asking for help, attempt {}", mNeedHelpCount);
            //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            */
        }
        return mCurrentAnswer;
    }

    [[nodiscard]] std::string get_current_answer() const {
        std::lock_guard lock(mAnswerMutex);
        return mCurrentAnswer;
    }

private:
    void start_request(std::string question, bool update_q = false) {
        stop_current_request();
        mCurrentAnswer.clear();
        mIsProcessing.store(true);

        if (update_q) mCurrentQuestion = std::move(question);
        
        mRequestThread = std::jthread([this](std::stop_token stop_token) {
            try {
                auto capture_stream = [this, &stop_token](std::string_view chunk) {
                    if (stop_token.stop_requested()) {
                        return;
                    }
                    std::lock_guard lock(mAnswerMutex);
                    mCurrentAnswer += chunk;
                    // TODO: optimize ?
                    ui::update_ai_tips(mCurrentAnswer);
                };
 
                mLLMClient->user(mCurrentQuestion)
                    .request(capture_stream);
            } catch (...) {
                // 忽略异常，保留已接收的部分答案
                log::warning("Assistant request encountered an error.");
            }
            mIsProcessing.store(false);
        });
    }

    [[nodiscard]] bool is_processing() const {
        return mIsProcessing.load();
    }

    void stop_current_request() {
        if (mRequestThread.joinable()) {
            mRequestThread.request_stop();
            mRequestThread.join();
        }
        mIsProcessing.store(false);
    }
}; // class Assistant

} // namespace d2x