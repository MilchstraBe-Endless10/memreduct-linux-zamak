# 中期记忆 / Roadmap State

更新时间：2026-10-28；顶部是合并后的真实状态，下方保留可追溯的阶段历史；仍不得将 PR #7 Draft 能力计入 main。

## 当前阶段（2026-10-28；PR #6 已合并）

**P0/P1/P2A 与 P2B 主窗口候选均在 main。** GitHub 已确认 `main@c0f616e5b0f3c7511cedcfe7d0b4536e8d54f0f2` 是 PR #6 的 Merge Commit，其按序两个父节点分别为旧 `main@b78785af233dbae7a80085fd7a9f45f30ae42711` 与 PR #6 最终已复审 Head `483ac837cd51d3293baf6c3d8b72309dd02f2ff9`。PR #2、#4、#5、#6 均已合并；#6 7/7 Review threads 已解决。该事实替代下方“#6 仍 Draft/未进入 main”的历史快照。

**验证范围：** PR #6 最终 Head `483ac837…` 的 [main-base Build 40638412007](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40638412007)、[P2A GUI 40638412068](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40638412068) 及 [P2B GUI 40638412022](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40638412022) 全 SUCCESS；合并后的 `main@c0f616e5…` [Push Build 40639545710](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40639545710) 亦 SUCCESS。不能把合并前的 GUI CI 说成新 main Merge SHA 上再次运行。

**P2C 设置页仍独立候选，尚未并入 main。** PR #7 仍 OPEN/Draft，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，Base 分支名指向 PR #6 分支；GitHub API 的 Base SHA 仍为旧 `911a3d0…`，`mergeable=false`。与当前 `main` 的共同祖先为 `911a3d0…`，双方重叠改动 6 路径：`scripts/ci/apt-diagnostics.sh` Blob 完全相同；余下 5 路径内容不同，为两个 workflow（`p2b-production-gui.yml`、`test.yml`）与三份阶段记忆（`MEDIUM_TERM`、`SESSION_LOG`、`SHORT_TERM`）。**这是 Blob 差异审计而非已运行的三方 merge-tree 冲突裁决。** PR #7 还有 8 条未解决、均 outdated 的历史 Review 线程，需集成后按新 HEAD 复核。

**当前未验收：** Mem Reduct 3.5.2 Windows 整页 1:1 像素、真实 Wayland/Hyprland 输入、托盘与清理确认/结果、真实内存清理、Polkit、Swap/ZRAM 和 root GUI。P2C 几何 184/184 及最大分量差 1.5 px 不构成整页像素通过；12 项简中来源差异仍 `DECISION_REQUIRED`。保留 PR #4 独立 12/12 探针、P2B 生产 Xvfb/Weston 与 P2C Windows/Linux 比较的各自验证对象，禁止删除或互相替代。

### 当前阶段表

| 阶段 | 当前主线/候选状态 |
|---|---|
| P0 / P1 | 已进入 main |
| P2A 探针与规格 | PR #4 / #5 已进入 main；独立 Canvas 探针保留 |
| P2B 生产主窗口候选 | PR #6 已于 2026-10-28 通过 Merge Commit 进入 main；CI 按精确 SHA 验证 |
| P2C 设置页、字体/主题/控件 | PR #7 OPEN/Draft；尚未合入 main；三方集成与真实目标验收待办 |
| P3A 偏好持久化 | ADR-0004 仍 PROPOSED；无实现/发布授权 |
| P3+ 实际清理、自动策略、Swap/ZRAM、发布 | 未验收；不得在此次集成中启用 |

### 后续执行门槛

- 首先完成本次两文件状态交接 PR 的 review/CI，并经另行批准后合入；不要直接写 `main`。
- PR #7 的冲突解决须单独授权：以新 `main` 作基线、核对旧祖先、保留两侧真实日志、原版规格与 PR #4 独立探针，不采用旧 #7 整棵 Tree 覆盖 `main`；完成真正三方合并及新 Head 的 Build、P2A/P2B/P2C CI 与独立 Review 后，再单独决定 Ready/merge。
- 本状态同步不包含生产代码更改、PR #7 写操作或特权操作。

## 历史快照（2026-10-28；PR #6 合并前的 main-base 验收）

### 当时的当前阶段

**P1 / P2A 基础已集成。** PR #2、#4、#5 均已合入 `main`; 当前 main 为 `b78785af233dbae7a80085fd7a9f45f30ae42711`。PR #4 的 12 项独立 Canvas 探针和隔离 Xvfb/Weston CI 已在 main。

**P2B 生产窗口仍在 Draft PR #6，尚未合入 main。** 本状态快照核验时，Base 为 `main@b78785af233dbae7a80085fd7a9f45f30ae42711`，Head `36d548d3dedcefffc37dceab13be887a1473187f`（父提交 `3b73868287c9ed13d3e8fc25f3b9d6fbe06cdc5a`）；GitHub 报告 `mergeable=true`。该 Head 的 main-base [Build 40632783618](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783618)、[P2A GUI 40632783623](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783623) 和 [P2B GUI 40632783657](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783657) 均 PASS，build job 与两套 GUI 的 Xvfb/Weston jobs 均完成。**这些检查对应记忆修正前的 `36d548d…`；本次文档提交后的新 Head 必须独立复验，不能沿用这些 PASS。** 2026-10-28 的 ChatGPT Review 指出 `SHORT_TERM.md`、`MEDIUM_TERM.md` 顶部仍记录旧 Base 与“main-base CI 未运行”；本次修改将旧状态保留为带日期的历史快照，并回应两条状态评论。PR 仍 Draft，尚未合并。

**P2C 仍在 Draft PR #7。** Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，Base 分支指向 PR #6，独立集成冲突尚未解决；本轮未修改 #7。GitHub API 报告 `mergeable=false`，返回的 Base SHA 仍为旧 #6 Head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，需后续核实状态刷新。P2C-4R1 几何 184/184 PASS、最大分量偏差 1.5 px，但整页 Windows 1:1 NOT PASS，真实 Wayland/Hyprland 输入 NOT RUN，托盘及清理确认/结果仍受限。

### 历史快照（2026-10-28；PR #6 Base 改为 main 前）

> 以下保留此前“尚未改 Base”的准确状态、旧 Base CI 证据和当时的冲突判断；仅描述该时点，当前状态以上方为准。

- PR #6 尚未改 Base；Base 为 `docs/issue-3-p2a-ui-spec`，Head 为双亲集成提交 `3b73868287c9ed13d3e8fc25f3b9d6fbe06cdc5a`。该时点只验证了旧 Base 上的 GUI/Build Push 与 PR CI，main-base PR CI 尚未运行。
- 当时 PR #7 以旧 PR #6 Head 为 Base，GitHub 报告 `mergeable=false/dirty`；相对前移后的 PR #6 的差异及冲突需重新核验。以上为历史判断，不作为当前冲突数量的结论。
- 该阶段旧 Base Head `3b738682…` 的 GUI Push/PR `40627628308` / `40627633632`、Build Push/PR `40627628264` / `40627633638` 均 SUCCESS；Xvfb/XTEST、Weston 生命周期、APT 诊断、主项目 13/13 与独立探针 12/12 通过。这些旧 Base/旧 Head 证据保留为历史结果，不替代 `36d548d…` 的 main-base 检查，也不替代本次文档提交的新 Head 检查。

### 已具备的代码

- Zamak 0.2.0 原型与生产 GUI 候选位于未合并的 #6/#7 分支；勿与 main 当前能力混写。
- `/proc/meminfo` 采样、内存策略原型及 C helper 仍受既有安全验收边界约束。
- main 上的独立探针只验证 GTK Canvas 交互和隔离生命周期，不代表生产 GUI 的功能或真实 Wayland 输入。

### 当前阻塞与边界

- PR #6 的四个文档冲突已在双亲集成提交中解决，净差异为 16 文件、零删除；PR #4 独立探针保留。PR #6 仍 OPEN/Draft、未合并。两条记忆状态 Review 由本次文档修改回应，推送后仍待 ChatGPT 独立复审；修改后的新 HEAD 必须另行完成 CI 验证。
- PR #7 仍以 PR #6 分支为 Base，存在独立集成冲突，且本轮没有修改授权。不能采用 #7 旧 Tree 覆盖当前基线；后续需独立核对祖先、三方合并、PR #4 探针与 P2B CI 保留情况，并重新跑 CI。
- Windows Mem Reduct 3.5.2 整页 1:1 像素一致性尚未通过；真实 Wayland/Hyprland 鼠标与键盘输入未验收。托盘与清理确认/结果受限；真实内存清理、Polkit、Swap/ZRAM 和 root GUI 均不属于已验收能力。
- 12 项简体中文来源差异仍为 `DECISION_REQUIRED`；不得通过规格合并批准具体译文。
- 未完成的内存清理、自动清理、Polkit、Swap/ZRAM、root GUI 与活动 Hyprland 操作继续禁止。

### 阶段表

| 阶段 | 交付 | 状态/依赖 |
|---|---|---|
| P0 | 正式文档、记忆、仓库、安全最小基线 | 已完成 |
| P1 | Zamak 0.2.0 编译与原生构建修复 | PR #2 已合入 main |
| P2A 测试 | 独立 GUI 探针及隔离 CI | PR #4 已合入 main；12/12 探针测试通过，限于探针边界 |
| P2A 规格 | Windows 3.5.2 源码规格与交互矩阵 | PR #5 已合入 main |
| P2B | 生产主窗口候选 | PR #6 OPEN / Draft；未计入 main |
| P2C | 五页设置 UI 与视觉校准 | PR #7 OPEN / Draft；1:1、真实 Wayland 输入及部分托盘/清理场景未验收 |
| P3A | 无特权语言/主题偏好持久化 | 设计稿待复审与批准；未开始实现 |
| P3+ | 清理/自动策略/ZRAM/发布 | 未开始；须按安全审查、测试和用户批准推进 |

### 决策与下一步

- 已有 ADR-0001 至 ADR-0003 继续有效；ADR-0004 持久化设计仍为 PROPOSED。
- PR #6 的 main-base CI 已在 `36d548d…` 通过；本轮仅修正两份状态记忆，推送后须确认新 Head 的三条 main-base workflow 全部完成，再交 ChatGPT 复审两条评论。PR #6 标记 Ready、合并，及处理 PR #7 均须另行明确授权；不得自动推进至 PR #7 或 P3A 生产实现。

## 历史快照：PR #6 P2B 阶段记忆（原记录日期 2026-10-25）

> 下列内容保留 PR #6 分支的当时状态与证据；其中 Draft/阻塞状态不代表当前 GitHub 状态。

## 当前阶段
**P1：Zamak 编译验收** — 本机 Zamak 0.2.0 check/test/build、Arch 原生构建、Hyprland GUI 基础验收及 Draft PR Actions 均通过。PR #2 仍 OPEN / Draft，等待 ChatGPT 复审和用户合并决定。

**P2A：源码规格与隔离 UI 测试** — 源码规格位于独立 Draft PR #5；CI 测试位于 PR #4。PR #5 R7 修正 head `d89d74fde433da1863b4de0223f974c824fb590e`，checks 2/2 PASS；R6 几何说明、最终 CI 证据与候选实现门槛已修正。PR #4 最终 head `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1` 的 GUI PR run `40192531809`、build PR run `40192531948` 均通过，push runs 分别为 `40192528601`、`40192528611`；保留原 9 项事件路由测试并新增 3 项探针测试，12/12 PASS。Windows 原生截图、真实 DLU→物理像素转换、Hyprland 输入和 2 px 对照仍 BLOCKED / NOT RUN。`IDD_MAIN=224×214 DLU` 是主窗模板；每个 `IDD_SETTINGS_* =260×168 DLU` 仅是设置子页模板，不是含导航和底部按钮的完整外框。

**P2B-1：生产主窗口候选** — 独立 Draft PR #6（base 为 PR #5 修正规格分支），代码 tested head `a6df90389a69fc9cad12272c2daa2f273c834385`。PR GUI run `40199653180`、build-and-test `40199653440` 和对应 push runs `40199649320` / `40199649284` 全通过。生产 Xvfb/XTEST 截图、菜单、双语、内存采样、close 与私有 Weston 生命周期已有证据；控件状态见 P2B-1 report。仍是集中参数化的未校准估算窗口，不是像素验收。

## 已具备的代码
- `src/main.zm`: Zamak Canvas 生产主窗口候选、四菜单、双语、禁用清理状态和真实 `/proc/meminfo` 定时采样；不等于完整功能集。
- `src/ui_geometry.zm`: 集中管理从官方 DLU 模板换算的候选尺寸参数，等待 Windows 字体/DPI 校准。
- `src/memory.zm`: `/proc/meminfo` 读取解析。
- `src/policy.zm`: 阈值与 30 分钟冷却原型。
- `native/async_privileged.c`: 异步 pkexec 调用桥。
- `native/memreduct_helper.c`: 三种特权缓存模式白名单。
- C 安全测试 3 项在本轮本机和 GitHub CI 均通过。

## 当前阻塞
- 等待 ChatGPT 复审 PR #6 的生产 UI、实际截图和诊断；所有 PR 均保持 Draft，不自动合并。
- Windows v3.5.2 原生截图、实测 DLU→像素换算、2 px 对照和 Hyprland 实际输入仍未完成；Weston 日志中的 GTK seat critical / SIGTERM cleanup BUG 保留为待解释诊断。
- 设置页、托盘、Polkit 授权端到端、安装/卸载、真实缓存清理、Swap/ZRAM 写入均未执行或实现。
