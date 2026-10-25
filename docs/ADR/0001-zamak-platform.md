# ADR-0001: Zamak + GTK3 原生应用

Status: Accepted by project direction (2026-10-25)

**Context**: Python/PySide6 原型适配，但用户指定 Zamak 重构并要求 Linux 桌面低负载、原生部署。

**Decision**: Zamak 0.2.x 负责 GUI、业务策略与只读内存采集；本地 C 层仅承担必要 FFI/原生辅助，Linux GTK3/libui-ng。工具链锁至 `@veyralux/zamak@0.2.0`，验证通过后再考虑升级。

**Consequences**: 没有 Python GUI 运行时；可能遇到早期语言编译、GUI 控件及 Wayland 适配限制。不得假定 Zamak 完整功能已可在用户机器编译。

**Alternatives**: PySide6/Tauri/Rust/Qt，暂不选；如 Zamak 持续受限，提出新 ADR + 用户批准，而非偷偷迁移。
