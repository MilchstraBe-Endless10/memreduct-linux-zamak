# 中期记忆 / Roadmap State

更新时间：2026-10-25；仅已见到的证据，未经推测。

## 当前阶段
**P1：Zamak 编译验收** — 本机 Zamak 0.2.0 check/test/build、Arch 原生构建、Hyprland GUI 基础验收及 Draft PR Actions 均通过。PR #2 仍 OPEN / Draft，等待 ChatGPT 复审和用户合并决定。

**P2A：源码规格与隔离 UI 测试** — 源码规格已提交到独立 Draft PR #5（base 为 P1 分支），不要求先合并 PR #2；CI 测试与规格分在 PR #4/#5。PR #4 R6 Xvfb/XTEST、Weston Headless/Pixman 与 build-and-test Push/PR Actions 全部通过（代码 head `e2d3c5e4481e9494472913f41636580369038cd8`）；保留原 9 项事件路由测试，新增 3 项探针测试，12/12 PASS。X11 WM close 使用隔离 Openbox + Alt+F4，不能用 `xdotool windowclose`（它执行 `XDestroyWindow`）。Windows 原生截图、真实 DLU→物理像素转换、Hyprland 输入和 2 px 对照仍 BLOCKED / NOT RUN。生产窗口须按官方 3.5.2 紧凑 DLU 资源和真实 DPI 换算，禁止放大；336×348 仅是 CI 测试窗口。

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
| P2A | 固定 Windows 3.5.2 UI/UX 规格、交互矩阵、Linux 能力映射及隔离测试 | 源码规格 PR #5 OPEN / Draft，PR #4 隔离 X11/Wayland CI PASS。Windows 源图缺失，像素验收 BLOCKED |
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
- 下一行动：等待 ChatGPT 复审 PR #4 和 PR #5；检查 PR #5 的 build-and-test Actions。PR #2/#4/#5 均保持 Draft，不合并；真实 Windows 像素验收仍未完成；不进入 P2B。
