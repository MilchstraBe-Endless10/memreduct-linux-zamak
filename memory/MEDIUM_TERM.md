# 中期记忆 / Roadmap State

更新时间：2026-10-25；仅已见到的证据，未经推测。

## 当前阶段
**P0：仓库治理和基础迁移** — 文档/规则/CI 骨架已整理；**尚未完成 GitHub 创建与远端提交**。

## 已具备的代码（来自 v0.2.0 原型）
- `src/main.zm`: GTK3 UI，按钮、缓存类型、状态更新及自动模式原型。
- `src/memory.zm`: `/proc/meminfo` 读取解析。
- `src/policy.zm`: 阈值与 30 分钟冷却原型。
- `native/async_privileged.c`: 异步 pkexec 调用桥。
- `native/memreduct_helper.c`: 三种特权缓存模式白名单。
- C 安全测试 3 项此前以及本轮运行通过。

## 当前阻塞
- 没有 `zam` 编译器，npm 注册表 DNS 失败，**Zamak 编译/GUI 未验证**。
- 无 GitHub 新建仓库权限接口，浏览器建仓失败；目前 PUBLIC 仓库并未创建，源码仍仅本地。
- Hyprland GUI、Polkit Agent 和 `libui-ng` 未做真机验收。

## 阶段表
| 阶段 | 交付 | 状态/依赖 |
|---|---|---|
| P0 | 正式文档、记忆、仓库、安全最小基线 | 进行中；仓库未创建 |
| P1 | Zamak `check/test/build` 通过，Linux GUI 在目标桌面打开 | 未开始；先有 Arch 实机环境 |
| P2 | 可信指标显示/日志/只读探测，安全反馈 | 未开始；依赖 P1 |
| P3 | 手动一键缓存清理验收、特权隔离与回滚测试 | 未开始；依赖 P2 |
| P4 | 有条件的自动策略、配置持久化、冷却与可观察性 | 未开始；依赖 P3 安全评审 |
| P5 | ZRAM 只读分析和可选管理，避免同现有 Swap 冲突 | 未开始；需 ADR 和用户批准 |
| P6 | Arch PKGBUILD/Release、真实场景对比基准、公开测试版 | 未开始；依赖各前置条件 |

## 决策和后续
- ADR-0001: 采用 Zamak 原生架构。
- ADR-0002: 独立最小权限 helper + 手动缓存回收优先。
- ADR-0003: GitHub + 三层记忆 + 双 AI 审核制。
- 下一行动：按 `prompts/CODEX-P1.md` 在 Codex Desktop 补齐工具链并拿到可验证日志。
