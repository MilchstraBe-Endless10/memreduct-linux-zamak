# 中期记忆 / Roadmap State

更新时间：2026-10-25；仅已见到的证据，未经推测。

## 当前阶段
**P1：Zamak 编译验收** — 本机 Zamak 0.2.0 check/test/build、Arch 原生构建和 Hyprland GUI 基础验收已通过；修复已在 `fix/issue-1-zamak-build` 准备提交。关联 PR 的 GitHub Actions 尚待运行，未合并，不能进入 P2。

## 已具备的代码（来自 v0.2.0 原型）
- `src/main.zm`: GTK3 UI，按钮、缓存类型、状态更新及自动模式原型。
- `src/memory.zm`: `/proc/meminfo` 读取解析。
- `src/policy.zm`: 阈值与 30 分钟冷却原型。
- `native/async_privileged.c`: 异步 pkexec 调用桥。
- `native/memreduct_helper.c`: 三种特权缓存模式白名单。
- C 安全测试 3 项在本轮本机和 GitHub CI 均通过。

## 当前阻塞
- 等待 P1 Draft PR 的 GitHub Actions 结果；如失败，按真实日志继续修复。
- 安装/卸载脚本（涉及 `/usr`）、Polkit 授权端到端和真实缓存清理没有执行；这些不属于本次编译/GUI smoke test，仍需后续隔离环境验收。

## 阶段表
| 阶段 | 交付 | 状态/依赖 |
|---|---|---|
| P0 | 正式文档、记忆、仓库、安全最小基线 | 完成；PUBLIC 仓库已创建，`main` SHA 已核验 |
| P1 | Zamak `check/test/build` 通过，Linux GUI 在目标桌面打开 | 本机验收通过；等待 Draft PR CI |
| P2 | 可信指标显示/日志/只读探测，安全反馈 | 未开始；依赖 P1 |
| P3 | 手动一键缓存清理验收、特权隔离与回滚测试 | 未开始；依赖 P2 |
| P4 | 有条件的自动策略、配置持久化、冷却与可观察性 | 未开始；依赖 P3 安全评审 |
| P5 | ZRAM 只读分析和可选管理，避免同现有 Swap 冲突 | 未开始；需 ADR 和用户批准 |
| P6 | Arch PKGBUILD/Release、真实场景对比基准、公开测试版 | 未开始；依赖各前置条件 |

## 决策和后续
- ADR-0001: 采用 Zamak 原生架构。
- ADR-0002: 独立最小权限 helper + 手动缓存回收优先。
- ADR-0003: GitHub + 三层记忆 + 双 AI 审核制。
- 下一行动：推送 P1 分支、创建关联 Issue #1 的 Draft PR 并核验 CI；不扩展功能、不合并，等待 ChatGPT 复审与用户批准。
