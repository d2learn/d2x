// 兼容壳:Provider 传输已析出至协议层(d2x.protocol.transport)。
// 活性超时、shell 安全引用、NDJSON 容错都住在协议层;core 侧只负责把
// 配置(超时时长)与日志(告警回调)接进去——见 checker 的构造点。
export module d2x.provider;

export import d2x.protocol.transport;

export namespace d2x::provider {

using d2x::protocol::Progress;
using d2x::protocol::IExerciseProvider;
using d2x::protocol::ProcessProvider;
using d2x::protocol::TransportOptions;
using d2x::protocol::shell_quote;

} // namespace d2x::provider
