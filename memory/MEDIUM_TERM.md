# 中期记忆 / Roadmap State

更新时间：2026-10-27；以下是当前 GitHub 状态与已见证据，不将 Draft 分支结果计入 main。

## 当前阶段

**P1 / P2A 测试基础已集成。** PR #2 于 merge commit `86169a1c4c1da0150ff3495290ae165b8c80d49a` 合入 main；PR #4 于 `459415001a7e3538e9496eeb90b8ee75a7378f14` 合入 main，带入 12 项独立 Zamak Canvas 探针测试、Xvfb/XTEST 与 Weston Headless 生命周期工作流。

**P2A 源码规格待集成。** PR #5 `d89d74fde433da1863b4de0223f974c824fb590e` 仍 OPEN / Draft，base 为 `fix/issue-1-zamak-build`。针对新 main 的本地模拟仅解决两个记忆文件冲突；未提交、未推送、未更改 PR Base。

**P2B / P2C 仍在 Draft 分支。** PR #6 `911a3d0b99b05f15bd0109dca475b0b5174e7888` 以 PR #5 为 base；PR #7 `b7ea31e6aea121b0b606866704f52ac0ddfe8be8` 以 PR #6 为 base。P2C-4R1 证据报告控件几何 184/184 PASS、最大分量偏差 1.5 px；整页像素 1:1 为 NOT PASS，Wayland/Hyprland 真实输入为 NOT RUN，托盘及清理确认/结果仍受限。这些结果未集成到 main。

## 已具备的代码

- Zamak 0.2.0 原型与生产 GUI 候选位于未合并的 #6/#7 分支；勿与 main 当前能力混写。
- `/proc/meminfo` 采样、内存策略原型及 C helper 仍受既有安全验收边界约束。
- main 上的独立探针只验证 GTK Canvas 交互和隔离生命周期，不代表生产 GUI 的功能或真实 Wayland 输入。

## 当前阻塞与边界

- PR #5 的本地三方合并解决稿需 ChatGPT 复审；推送、改 Base 与合并均需后续单独授权。
- PR #6/#7 的 base/head 和内容在 #5 集成前不调整；若 #5 后续更新，必须重新检查这两个下游 PR 的比较范围及 Review/CI。
- 12 项简体中文来源差异仍为 `DECISION_REQUIRED`；不得通过规格合并批准具体译文。
- 未完成的内存清理、自动清理、Polkit、Swap/ZRAM、root GUI 与活动 Hyprland 操作继续禁止。

## 阶段表

| 阶段 | 交付 | 状态/依赖 |
|---|---|---|
| P0 | 正式文档、记忆、仓库、安全最小基线 | 已完成 |
| P1 | Zamak 0.2.0 编译与原生构建修复 | PR #2 已合入 main |
| P2A 测试 | 独立 GUI 探针及隔离 CI | PR #4 已合入 main；12/12 探针测试通过，限于探针边界 |
| P2A 规格 | Windows 3.5.2 源码规格与交互矩阵 | PR #5 OPEN / Draft；本轮仅本地模拟解决冲突 |
| P2B | 生产主窗口候选 | PR #6 OPEN / Draft；未计入 main |
| P2C | 五页设置 UI 与视觉校准 | PR #7 OPEN / Draft；1:1、真实 Wayland 输入及部分托盘/清理场景未验收 |
| P3A | 无特权语言/主题偏好持久化 | 设计稿待复审与批准；未开始实现 |
| P3+ | 清理/自动策略/ZRAM/发布 | 未开始；须按安全审查、测试和用户批准推进 |

## 决策与下一步

- 已有 ADR-0001 至 ADR-0003 继续有效；ADR-0004 持久化设计仍为 PROPOSED。
- 下一步先复审 PR #5 本地解决补丁和下游影响；只有所有者另行授权后，才可更新 #5 远端分支或调整 Base。未经单独批准不得合并任何 PR，也不得自动推进至 #6/#7/P3A 实现。
