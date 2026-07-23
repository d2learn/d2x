// 兼容壳:领域类型已析出至协议层(d2x.protocol.types)——它们是双向协议的
// 载荷形状,由 core 与课程侧共用。本模块保留 d2x::domain 命名空间,core 内
// 既有代码零改动。学习进度(session 状态)不属于协议层,仍归 core。
export module d2x.domain;

export import d2x.protocol.types;

export namespace d2x::domain {

using d2x::protocol::Exercise;
using d2x::protocol::Outcome;
using d2x::protocol::Diagnostic;
using d2x::protocol::Verdict;
using d2x::protocol::to_string;
using d2x::protocol::outcome_from;

} // namespace d2x::domain
