export module d2x.platform;

#if defined(_WIN32)
import d2x.platform.windows;
#else
import d2x.platform.linux;
#endif

namespace d2x {
namespace platform {
    export using platform_impl::run_command_capture;
} // namespace platform
} // namespace d2x
