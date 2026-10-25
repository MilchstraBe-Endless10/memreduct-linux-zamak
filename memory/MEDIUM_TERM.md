# 中期记忆 / Roadmap State

更新时间：2026-10-25；仅已见到的证据，未经推测。

## 当前阶段
**P1：Zamak 编译验收** — 本机 Zamak 0.2.0 check/test/build、Arch 原生构建、Hyprland GUI 基础验收及 Draft PR Actions 均通过。PR #2 仍 OPEN / Draft，等待 ChatGPT 复审和用户合并决定。

**P2A：源码规格与隔离 UI 测试** — 以独立 Draft PR 从 P1 基线审阅规格；不要求先合并 PR #2。UI 规格文件已整理，分支仍在本机待完整提交。PR #4 的 R6 运行中 Wayland 与 build jobs 通过，X11 真实 close 场景因 Xvfb 未启动 WM 而失败，正在修复测试环境；不修改 main/P1 功能分支，不进入 P2B。Windows 原生截图与 2 px 对照仍 BLOCKED / NOT RUN。产品窗口尺寸须按 3.5.2 资源 DLU 和实测 DPI 换算，禁止把 CI 探针的大于原版估值用作生产尺寸。

## 已具备的代码（来自 v0.2.0 原型）
- `src/main.zm`: GTK3 UI，按钮、缓存类型、状态更新及自动模式原型。
- `src/memory.zm`: `/proc/meminfo` 读取解析。
- `src/policy.zm`: 阈值与 30 分钟冷却原型。
- `native/async_privileged.c`: 异步 pkexec 调用桥。
- `native/memreduct_helper.c`: 三种特权缓存模式白名单。
- C 安全测试 3 项在本轮本机和 GitHub CI 均通过。

## 当前阻塞
- 等待 Draft PR #2 复审及用户决定是否合并；不自动合并或进入 P2。
- 安装/卸载脚本（涉及 `/usr`）、Polkit 授权端到端和真实缓存清理没有执行；后续需在适当隔离环境验收。

## 阶段表
| 阶段 | 交付 | 状态/依赖 |
|---|---|---|
| P0 | 正式文档、记忆、仓库、安全最小基线 | 完成；PUBLIC 仓库已创建，`main` SHA 已核验 |
| P1 | Zamak `check/test/build` 通过，Linux GUI 在目标桌面打开 | 本机、GUI 与 CI 验收通过；Draft PR 等待复审 |
| P2A | 固定 Windows 3.5.2 UI/UX 规格、交互矩阵、Linux 能力映射及隔离测试 | 规格分支 `docs/issue-3-p2a-ui-spec` 正在验证并准备 Draft PR；CI 分支 `test/issue-3-isolated-gui` 的 R6 X11 close 需修复后复跑。Windows 源图缺失，像素验收 BLOCKED |
| P2B | 主窗口 UI 高保真实现 | 未开始；等 P2A 规格审查，不在本轮启动 |
| P2 | 可信指标显示/日志/只读探测，安全反馈 | 未开始；依赖 P1 与阶段审查 |
| P3 | 手动一键缓存清理验收、特权隔离与回滚测试 | 未开始；依赖 P2 |
| P4 | 有条件的自动策略、配置持久化、冷却与可观察性 | 未开始；依赖 P3 安全评审 |
| P5 | ZRAM 只读分析和可选管理，避免同现有 Swap 冲突 | 未开始；需 ADR 和用户批准 |
| P6 | Arch PKGBUILD/Release、真实场景对比基准、公开测试版 | 未开始；依赖各前置条件 |

## 决策和后续
- ADR-0001: 采用 Zamak 原生架构。
- ADR-0002: 独立最小权限 helper + 手动缓存回收优先。
- ADR-0003: GitHub + 三层记忆 + 双 AI 审核制。
- 下一行动：完成独立 P2A 规格 Draft PR 和 PR #4 CI 复审证据；等待 ChatGPT 复审。PR #2/#4/#5 均不得自动合并；不进入 P2B。
