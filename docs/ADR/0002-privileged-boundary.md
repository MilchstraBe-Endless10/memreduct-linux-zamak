# ADR-0002: Independent privileged helper

Status: Accepted as prototype direction (2026-10-25)

**Decision**: GUI 普通用户运行，固定枚举动作通过 C bridge → `pkexec` → root-owned `/usr/libexec/memreduct-zamak-helper`；Helper 不提供 shell、任意路径或 PID 操作；自动策略默认关闭。

**Rationale**: 减少特权边界面积、可审计回收逻辑、明确退出状态。

**Unresolved**: Polkit 策略与用户体验、异步异常退出、如何安全执行后台自动任务、安装所有权、Wayland Polkit Agent，必须先验证。
