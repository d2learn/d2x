export module d2x.ui.plugin.tui.ui_thread_helper;

import std;

namespace d2x {

export template<typename StateT>
class UIThread {
public:
    using RenderCallback = std::function<void(const StateT&)>;

    UIThread(RenderCallback render_fn, int fps = 24) 
        : render_fn_(std::move(render_fn))
        , frame_interval_(std::chrono::milliseconds(1000 / fps))
        , running_(false) 
    {}

    void start() {
        if (running_.exchange(true)) return;
        
        render_thread_ = std::jthread([this](std::stop_token stop_token) {
            render_loop(stop_token);
        });
    }

    void stop() {
        running_ = false;
        if (render_thread_.joinable()) {
            render_thread_.request_stop();
        }
    }

    void update_state(StateT new_state) {
        std::lock_guard lock(state_mutex_);
        state_ = std::move(new_state);
    }

    // Update specific field(s) of the state using a callback
    template<typename UpdateFn>
    requires std::invocable<UpdateFn, StateT&>
    void update_field(UpdateFn&& update_fn) {
        std::lock_guard lock(state_mutex_);
        std::forward<UpdateFn>(update_fn)(state_);
    }

    StateT get_state() const {
        std::lock_guard lock(state_mutex_);
        return state_;
    }

    ~UIThread() {
        stop();
    }

private:
    void render_loop(std::stop_token stop_token) {
        using namespace std::chrono;

        while (!stop_token.stop_requested() && running_) {
            const auto frame_start = steady_clock::now();

            StateT current_state;
            {
                std::lock_guard lock(state_mutex_);
                current_state = state_;
            }

            if (render_fn_) {
                render_fn_(current_state);
            }

            const auto frame_end = steady_clock::now();
            const auto elapsed = duration_cast<milliseconds>(frame_end - frame_start);
            
            if (elapsed < frame_interval_) {
                std::this_thread::sleep_for(frame_interval_ - elapsed);
            }
        }
    }

    RenderCallback render_fn_;
    std::chrono::milliseconds frame_interval_;
    std::jthread render_thread_;
    std::atomic<bool> running_;
    mutable std::mutex state_mutex_;
    StateT state_;
};

} // namespace d2x
