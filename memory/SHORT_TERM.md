# 短期记忆 / Latest Handoff

## 当前真实状态（2026-10-28；PR #6 已合并至 main）

- **远端已核实：** `main@c0f616e5b0f3c7511cedcfe7d0b4536e8d54f0f2` 是 [PR #6](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6) 的真实 Merge Commit（2026-10-28 05:42:24 UTC），按序父提交是旧 `main@b78785af233dbae7a80085fd7a9f45f30ae42711` 和已复审 PR Head `483ac837cd51d3293baf6c3d8b72309dd02f2ff9`；Tree `17c1bff4f7a9bdc07089b1954f23b250cd258338`。PR #6 为 CLOSED/Merged，7/7 Review threads 已解决；不再称 OPEN/Draft。
- **精确 HEAD 测试：** 合并前 `483ac837…` 的 main-base PR [Build 40638412007](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40638412007)、[P2A GUI 40638412068](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40638412068)、[P2B GUI 40638412022](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40638412022) 均 SUCCESS（合计五个 job）；合并后 `main@c0f616e5…` 的 [Push Build 40639545710](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40639545710) 已完成并 SUCCESS。合并后新运行的是 Build，不冒称 P2A/P2B GUI 也在新 merge SHA 上重跑。
- **当前交付边界：** P1、P2A 独立 Canvas 探针、P2A 源规格和 P2B 紧凑生产主窗口候选已在 `main`；Xvfb/XTEST 生产输入与 Weston 隔离生命周期有 CI 证据，但 Windows 3.5.2 **整页 1:1 像素验收 NOT PASS**，真实 Wayland/Hyprland 键鼠输入 **NOT RUN**。清理、Polkit、Swap/ZRAM、root GUI、托盘与清理确认/结果等未获功能验收，继续遵守安全边界。
- **PR #7 当前状态：** [PR #7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 仍 OPEN/Draft，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，Base 分支名仍为 `feat/issue-3-p2b-main-window`；GitHub API 仍返回旧 Base SHA `911a3d0b99b05f15bd0109dca475b0b5174e7888` 和 `mergeable=false`。本轮不改 PR #7 的 Base、HEAD、代码或 Review。
- **PR #7 只读三树审计：** 相对当前 `main@c0f616e5…` 与 PR #7 Head，其共同祖先 `911a3d0…`；两侧 Git Blob SHA 比较有 6 个重叠路径，`scripts/ci/apt-diagnostics.sh` 内容完全相同；其余 5 个不同：`.github/workflows/p2b-production-gui.yml`、`.github/workflows/test.yml`、`memory/MEDIUM_TERM.md`、`memory/SESSION_LOG.md`、`memory/SHORT_TERM.md`。这五个是**待三方合并验证的差异路径，不能宣称 5 个已证实的文本冲突**。主线独立 P2A 探针目录及其 workflow 不得由 PR #7 旧树覆盖删除。PR #7 八条旧审查线程均未解决且标记 outdated，须在真实新集成 Head 上逐条复核。
- **本轮界限：** 本轮仅更新两份状态记忆并在独立文档 PR 交接；不会将此分支变动写入 `main`、不会触碰 PR #7 或做特权操作。后续 #7 需隔离三方合并、独立 CI/截图复核及另外的明确合并授权；12 项简中资源差异仍 `DECISION_REQUIRED`。

## 历史快照（2026-10-28；PR #6 合并前的 main-base 验收）

- `main` 为 `b78785af233dbae7a80085fd7a9f45f30ae42711`。PR #6 仍 OPEN / Draft、未合并，Base 为 `main`，Head 为 `36d548d3dedcefffc37dceab13be887a1473187f`；当前 GitHub 报告 `mergeable=true`。Head `36d548d…` 的父提交是此前双亲集成提交 `3b73868287c9ed13d3e8fc25f3b9d6fbe06cdc5a`。
- 记忆修正前 Head `36d548d…` 的 main-base PR 检查通过：[Build 40632783618](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783618)、[P2A GUI 40632783623](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783623)、[P2B GUI 40632783657](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783657)。Build 的 Linux job、两条 GUI workflow 的 Xvfb/XTEST 与 Weston Headless/Pixman jobs 均成功；PR 事件针对 `main` 与该 Head。**这些 PASS 只属于 `36d548d…`；记忆修正产生的新 Head 必须独立复验，不能沿用旧结果。** P2A artifacts：X11 `12792291897` SHA-256 `7393d70168a11587a382bfece61a1bb1233c4a14c67e7c0b8c3850988c9ea242`，Weston `12792560638` SHA-256 `57d030b5ad64ebb89c9722e5b431e45caa8b34d32e45ad9c17b521ff30c53119`。P2B artifacts：X11 `12792660966` SHA-256 `8b2a9d099b8992c8901788eeb62ed294619472ef3e536b90dfae43e1684f20d2`，Weston `12791754327` SHA-256 `92063912e303491206bee5ccbb2a672d32578ec401a9868f4371a5cd31059815`。
- 本次复核发现两条尚未解决的记忆状态 Review：[`SHORT_TERM.md` #4396911245](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6#discussion_r4396911245) 与 [`MEDIUM_TERM.md` #4396911255](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6#discussion_r4396911255)。本次只修正状态记录并保留旧阶段历史；线程状态留待独立复审，不在这里代替审查者解决。
- PR #7 仍 OPEN / Draft，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，Base 分支仍指向 PR #6。该 PR 有独立集成冲突且未获本轮修改授权；本轮未改 PR #7。GitHub API 此次仍报告 `mergeable=false`，并返回旧 Base SHA `911a3d0b99b05f15bd0109dca475b0b5174e7888`，需后续单独核实其基线刷新与冲突解决。
- Windows Mem Reduct 3.5.2 整页 1:1 像素验收未通过；真实 Wayland/Hyprland 键鼠输入未验收。实际内存清理、Polkit、Swap/ZRAM 操作不属于本轮验收；相关功能继续保持未验收状态。

## 历史快照（2026-10-28；PR #6 Base 改为 main 前）

> 以下保留本轮授权改 Base 之前的完整状态与测试记录。“尚未改 Base”及 main-base CI 未运行仅描述该时点，不代表当前 GitHub 状态。

- `main` 为 `b78785af233dbae7a80085fd7a9f45f30ae42711`，PR #2、#4、#5 已合并；PR #6 的生产窗口仍未合入 `main`。
- PR #6 仍 OPEN / Draft，Base 仍为 `docs/issue-3-p2a-ui-spec`（该分支 HEAD `a19474ca3f79de65a18e76f6852ab503ce11b173`）。PR #6 已普通快进推送双亲集成 Merge Commit `3b73868287c9ed13d3e8fc25f3b9d6fbe06cdc5a`，Tree `22022b50f6233c2e729b02cf678b426e12af0d21`；父提交按序为原 PR #6 HEAD `911a3d0b99b05f15bd0109dca475b0b5174e7888`、当时 `main` `b78785af233dbae7a80085fd7a9f45f30ae42711`。GitHub 报告 `mergeable=true` **仅针对当前旧 Base**，不能据此视为新 main-base 验收。
- 该集成 Head 的 [GUI Push 40627628308](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627628308)、[GUI PR 40627633632](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627633632)、[Build Push 40627628264](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627628264)、[Build PR 40627633638](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627633638) 全部 SUCCESS；Xvfb/XTEST、Weston Headless 各两条 GUI lane 通过，APT update/install 诊断 PASS，主项目 Zamak 13/13。PR GUI 事件 Base 仍为旧 P2A 分支；**PR #6 新 main-base CI NOT RUN**。本次文档状态修正的后续 HEAD 与 CI 应另以 GitHub 远端结果核验。
- PR #6 生产 UI 的三条原始缺陷 Review 已有对应证据并解决；2026-10-28 新增的两条记忆状态审查需要在本状态修正提交推送后复核。本次状态修正不改生产 GUI、清理权限或测试逻辑。
- PR #7 OPEN / Draft，HEAD `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，Base 分支指向 PR #6，但旧 HEAD 尚未继承本轮集成历史，GitHub 当前 `mergeable=false/dirty`。新 #6 与旧 #7 两点 Tree 比较有 46 个路径差异（含 10 个表观删除），三点 PR 比较为 30 个 P2C 变更路径；这些不是未来真实三方合并的删除结论。#7 未修改，后续必须独立冲突解决，保护 PR #4 探针与 PR #6 生产 GUI CI。
- P2C-4R1 几何 184/184 PASS、最大分量偏差 1.5 px，**整页 Windows/GTK 1:1 未通过、真实 Wayland/Hyprland 输入 NOT RUN**；托盘、清理确认/结果及特权清理未验收。12 项简中原版来源差异仍为 `DECISION_REQUIRED`；未经另行授权不得改 Base、标记 Ready、合并或修改 PR #7。

## 历史快照（2026-10-28；PR #6 本地集成审计、推送前）

> 以下记录属于当时仅完成本地冲突模拟、尚未推送的阶段；旧 HEAD、旧合并状态与旧 Review 线程信息不代表当前 GitHub 状态，当前状态以上方记录为准。

- `main` 当前为 `b78785af233dbae7a80085fd7a9f45f30ae42711`；PR #2、#4、#5 已合并。PR #5 的原分支 Head 为 `a19474ca3f79de65a18e76f6852ab503ce11b173`，其集成 Merge Commit 已进入 main。
- PR #6 OPEN / Draft，Head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，base 分支 `docs/issue-3-p2a-ui-spec`；远端该分支 Head 为 `a19474ca3f79de65a18e76f6852ab503ce11b173`。GitHub PR 状态报告 `mergeable=false / DIRTY`。本地 merge-tree 与合并模拟在 `docs/UI_FIDELITY_TEST_PLAN.md`、`memory/MEDIUM_TERM.md`、`memory/SHORT_TERM.md`、`memory/SESSION_LOG.md` 发现冲突；只在独立 worktree 形成解决草稿。
- PR #7 OPEN / Draft，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，base 为 PR #6 Head，GitHub 当前报 CLEAN。任何 #6 Head 更新后必须重新审计 #7 的二点/三点差异。
- PR #6 的最终旧-base GUI/build Actions 为 [40208944364](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208944364) / [40208944120](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208944120)，均 PASS；这不构成 main-base CI。采样失败后清空旧值、重绘及禁用清理的证据见 P2B-2 Review、`src/main.zm` 与 Xvfb/XTEST 脚本；两条结构/菜单旧 Review 仍未解决。
- PR #6 生产 GUI CI 草稿已在独立 worktree 准备：保留 push/旧 base，加入 main 与 `opened/synchronize/reopened/edited`；APT update/install 复用 `apt-diagnostics.sh` 并分步限时，Runner job 60 分钟，Action 固定到 Node 24 版本，artifact 缺失时失败且并发不取消证据作业。尚未提交或推送。
- 本地合并候选验证：治理 29 项、C 安全单测 3 项、Shell 语法、Zamak 0.2.0 check/test/build（13/13）、Arch 原生构建、生产 GUI 构建、Canvas 探针（12/12）、PyYAML 工作流语法和 `git diff --check` 均 PASS。当前环境没有 Xvfb、Openbox、Weston/wayland-info、actionlint；生产 XTEST/Weston 图形运行和 actionlint 校验 NOT RUN，未安装依赖，也未操作活动桌面。以上是本地候选，不是新 GitHub main-base CI。
- P2B/P2C 功能仍不属于 main。P2C-4R1 几何 184/184 PASS（最大偏差 1.5 px），整页 1:1 NOT PASS，真实 Wayland/Hyprland 输入 NOT RUN，托盘/清理确认与结果仍受限；12 项简中来源差异为 `DECISION_REQUIRED`。

## 历史快照（2026-10-28；PR #5 集成前状态）

- main 为 `459415001a7e3538e9496eeb90b8ee75a7378f14`；PR #2、#4 已合并。PR #5 仍 OPEN / Draft，当前旧 Base 为 `fix/issue-1-zamak-build`（`11b0b9444cd94f0026b855abe3849a06106426f2`）。其 P2A 集成 Merge Commit `4996c83103c72484f20a435118711cddc0aea244` 已推送到 `docs/issue-3-p2a-ui-spec`，Tree 为 `3afdf06d4dd3e1375ba7864d78f9572229950cca`；按序双亲是原 PR #5 Head `d89d74fde433da1863b4de0223f974c824fb590e`、main `459415001a7e3538e9496eeb90b8ee75a7378f14`。
- 此快照记录的是 PR #5 改 Base 前状态，不能当作改 Base 后的当前目标。PR #6 仍 OPEN / Draft，Head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，以 PR #5 分支为 Base；GitHub 当前报告 `mergeable=false`，需单独解决与复审。PR #7 仍 OPEN / Draft，Base 为 PR #6，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`。#5/#6/#7 均未合并。
- P2B/P2C 生产 UI、Windows 原生采集与视觉比较仍来自未合并的 #6/#7 分支，不代表当前 main；P2C-4R1 几何为 184/184 PASS，最大偏差 1.5 px；整页 1:1 NOT PASS，Wayland/Hyprland 真实输入 NOT RUN，托盘及清理确认/结果受限。
- 12 项简中来源差异仍为 `DECISION_REQUIRED`；未批准文案改动。
- 本地模拟集成的原始历史和来源 SHA 保留在下方及 `memory/SESSION_LOG.md`；该历史条目描述的是当时的本地模拟，不再代表 PR #5 当前远端状态。
- 已推送集成树的本地验证记录：治理 29 项、C/Python 安全测试 3 项、Shell 语法、根项目 Zamak 0.2.0 check/test（5/5）/build、独立探针 check/test（12/12）/build、`git diff --check` 与敏感内容审计均 PASS。探针源码 SHA-256 为 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。这些结果来自改 Base 前的 `4996c831…`。


## 历史验收记录：PR #4 main Base 迁移（2026-10-27）
> 本记录描述 #4 合并前的 Base/CI 验收；当前合并状态以文件顶部快照为准。

- 所有者授权后，仅将 PR #4 base 从 `fix/issue-1-zamak-build` 调整为 `main`；没有改 main、rebase、merge、标记 Ready 或处理 PR #5/#6/#7。PR 保持 OPEN / Draft。
- 为保证 Base 编辑触发检查，提交 `ada1d82ef57ab76277dd6b088999e577e4f3a834`，在 `.github/workflows/p2a-r4-isolated-gui.yml` 显式加入 `pull_request.edited`；发现 `test.yml` 仍使用默认活动类型后，提交 `0067b97c6a9a3763af62e1a6990db413bc198820` 为 build-and-test 也加入 `edited`，并借真实 `synchronize` 事件取得最终 head 的完整 PR 检查。
- 在 `0067b97c6a9a3763af62e1a6990db413bc198820` 上，PR Base 为 `main` SHA `86169a1c4c1da0150ff3495290ae165b8c80d49a`；GitHub 为 PR 合成 checkout `3d8927eac872b791ac70e08e2ae182876c56dfca`。main-base GUI run [40501262596](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40501262596) 的 Weston 与 Xvfb job 均 PASS；build run [40501262352](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40501262352) PASS。治理、shell、C 安全、Zamak 5 项单测和 Arch 原生构建均执行通过；探针 check/test/build 通过，12/12 测试通过。
- Weston、Xvfb APT update/install 均有诊断记录且 `apt_exit_code=0`。Xvfb APT install 约 17 分钟后成功，尚无证据确认更深层原因；步骤设置 40 分钟时限。Alt+F4 触发 `CI_WINDOW_CLOSE` 与 `CI_APP_RETURNED`，两客户端退出码均为 0；Xvfb hover/press 与 press/release ROI 差各 8004 像素，语言切换差 4697，返回英文后差 0。
- 本次 Wayland artifact ID `12733767416`，GitHub digest/独立 ZIP SHA-256 `cf6b5fcd1740df07996fa3756f431788d5f086241d3ba6f95c7e155f6c21dab9`；X11 artifact ID `12734377907`，GitHub digest/独立 ZIP SHA-256 `d9aa083317660cba7c29212fb6ebd1d62ac8199f1d1197bc54d7743fd3bb1b7f`。两包中的探针二进制 SHA-256 均为 `9f7cb6db085fc3b247a62203b2e7f60d994620639c7dc5d51997c6edd9204ce6`，源文件哈希核验通过。
- 当前 PR #4 改动仍是隔离探针、报告/记忆及两个 CI 工作流；没有生产 UI 或特权功能改动。一个 inline review thread 仍未解决，内容是确保 Base 编辑触发检查；本轮已通过显式 `edited` 和 main-base PR CI 提供证据，留待 ChatGPT 正式复审。
- 本记录对应已完整验收的 `0067b97…`。短期记忆随后单独提交时，必须对新 HEAD 重新运行 CI；不得将上述成功结果冒称为该后续 HEAD 的检查。


## 历史快照：P2A-R7 规格修正及 P2B-1 开发（2026-10-25）
> 以下内容是 2026-10-25 的规格/开发阶段记录；其中 PR 状态与截图缺口不代表当前状态。

### 开工核验与纠正
- PR #2、PR #4、PR #5 均为 OPEN / Draft；PR #5 head `6b3862b0e6d105f42dbf8d2d6c679cdfb182ad16`，base `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #4 最终 head `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1`。没有合并任何 PR，未修改 main、P1 或 PR #4。
- PR #4 最终 GUI PR Actions `40192531809`、build PR Actions `40192531948` 已在规格文档中刷新；同 SHA push runs 为 `40192528601`、`40192528611`。
- 尺寸约束更正：`IDD_MAIN=224×214 DLU` 是主窗模板；每个 `IDD_SETTINGS_* =260×168 DLU` 是设置子页面模板，不能当作含导航、底部按钮和边框的完整设置窗口尺寸。P2B 可开发参数集中、明确未校准的紧凑候选，真实 Windows 像素验收独立保持 BLOCKED。

### 本轮开发状态
- 在规格 PR 分支更新了最终 CI 证据、DLU 页面/外框说明及 P2B 候选与最终像素验收的门槛区分。
- P2B-1 将从修正规格分支新建独立功能分支，实现生产 `src/main.zm` UI；PR #4 的 12 项探针测试保持原样。最终构建、隔离生产 GUI CI、截图哈希和各控件状态待实施后记录。
- 不在主机启动 GUI，不连接活动 Hyprland IPC，不执行清理、Polkit、Swap 或 ZRAM 操作。Windows 原版真实截图、DLU 实测换算及 2 px 对照仍 BLOCKED / NOT RUN。

## 历史记录：P2B-2 原版主窗口结构与数据可靠性修复（2026-10-25）

- 按用户任务核对 PR #6 最新 ChatGPT Review 与 3 条行内评论。PR #6 保持 OPEN / Draft；base `docs/issue-3-p2a-ui-spec` SHA `d89d74fde433da1863b4de0223f974c824fb590e`。修复后代码 HEAD `e4fccb6293c698015c8f4105947669bfdc4693d6` 与远端一致；未修改或合并 main、PR #2/#4/#5。
- 生产主窗口改回三个纵向分组，每组各自显示 Usage、Available、Total available 标签和值；简体中文 Pagefile/Swap 标题为“交换空间”。System working set 保持 `—`，说明放在状态区。DLU/控件/弹窗几何集中于 `src/ui_geometry.zm`，仍为 `PROVISIONAL`，不是 Windows 实测像素目标。
- 菜单弹窗矩形按 Canvas 视口翻转/约束，根菜单、英语/简中语言子菜单和清理菜单均有 `fits=true` 日志。必需 meminfo 字段缺失、非法、负数或关系不一致时采样失败；真实 SwapTotal=0 有效；失败会清空旧值并请求重绘。保留/新增 Zamak parser 与几何测试，13/13。
- 精确 HEAD `e4fccb6...` 的 GitHub Actions：生产 GUI run [40208121223](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121223) 两项作业 Xvfb/XTEST、Weston Headless/Pixman 均 PASS；build/test run [40208121224](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121224) PASS。X11 artifact `12604402691`，Wayland artifact `12604102361`。源码、二进制、截图 SHA-256 和精确映射记录在 `docs/P2B-1_PRODUCTION_UI_REPORT.md`。
- Xvfb 中实际生产窗口为 337×369，Canvas 313×345，scale 1；英语、简中三组布局及各菜单通过，边界都在 Canvas 内；鼠标、键盘、Esc 焦点恢复、WM close 通过，进程退出码 0。采样失败注入使旧值失效并显示 em dash；清理按钮和选项记录 `action=none`，未触发特权调用。
- Weston 私有 Headless/Pixman socket 下生产客户端连接、绘制、读取真实 `/proc/meminfo`、从 `app.run` 返回并以 0 退出。仍保留 1 条 `gdk_seat_get_keyboard` critical、无 `wl_seat` 广播、受控清理时 compositor layer BUG；因果未确定。未操作活动 Hyprland/桌面，未在本机启动 GUI。
- 本轮既有本机验证结果：项目检查 29 项、Python/C 单测 3 项、shell 语法、Zamak 0.2.0 check、test 13/13、build、Arch build、`git diff --check` PASS。没有执行清理/Polkit/内核写入、Swap/ZRAM 修改或 root GUI。
- Windows 3.5.2 原生截图、真实字体/DPI DLU 换算和 2 px overlay 继续 `BLOCKED / NOT RUN`。设置页、托盘等不在本轮实现。最新 Linux 英文/中文实际生产截图与来源 SHA 记录在测试报告；尚无原生 Windows 截图，不能宣称像素级或 1:1 验收。

## 历史记录：P2B-1 生产主窗口候选与 CI 复测（2026-10-25）

- PR #5 的 R7 规格修正 head `d89d74fde433da1863b4de0223f974c824fb590e` 已核对，仍 OPEN / Draft，CI 2/2 PASS；正式 `IDD_MAIN=224×214 DLU` 为主窗口模板，每个 `IDD_SETTINGS_* =260×168 DLU` 是子页模板。R6 Actions `40192531809` / `40192531948` 与对应 push runs `40192528601` / `40192528611` 已记录。没有合并 PR #2/#4/#5。
- P2B 独立分支 `feat/issue-3-p2b-main-window` 从 PR #5 修正规格分支派生；PR #6 OPEN / Draft，base 为 `docs/issue-3-p2a-ui-spec`。生产代码 head `a6df90389a69fc9cad12272c2daa2f273c834385` 通过 PR GUI run `40199653180`、build-and-test run `40199653440`；push GUI/build runs `40199649320` / `40199649284` 也通过。远端分支 SHA 已核对。
- 生产 `src/main.zm` 以真实 `/proc/meminfo` 显示 RAM/Swap，System working set 显示不可用；英文/简中独立切换；清理按钮和八项 Windows 清理选择全部禁用，不触发 Polkit 或内核写操作。DLU 转换参数集中在 `src/ui_geometry.zm`，仍为未校准估算。
- Xvfb/XTEST 实测正式窗口：top-level 337×369、Canvas 313×345，scale 1；点击语言菜单、split button、禁用区域、Tab/方向键/Enter/Escape 与 Alt+F4 close 均通过，退出码 0。实际截图及 SHA-256 在 `docs/P2B-1_PRODUCTION_UI_REPORT.md` 和 artifact `12601156413`。
- Weston 13.0.0 Headless/Pixman 私有 Wayland socket 下正式客户端连接、绘制、读 meminfo、退出码 0；不测试 Wayland 输入。诊断保留：一次 `gdk_seat_get_keyboard` critical、`wl_seat` 未公告；Weston SIGTERM 清理日志含 `BUG: finalizing a layer with views still on it.`，尚未确认为应用故障，不隐藏日志。
- 本机 `verify_project.py` 29 项、C 安全单测 3 项、shell syntax、Zamak 0.2.0 check/test/build（7/7）、`scripts/build-arch.sh`、`git diff --check` 均 PASS。没有在主机启动 GUI、访问活动 Hyprland IPC、操作用户桌面或执行特权清理。
- Windows 原生 v3.5.2 截图、真实字体/DPI DLU 校准、配对 overlay/2 px 验收仍 BLOCKED / NOT RUN；Hyprland 鼠标/键盘、HiDPI、设置页与托盘未实现。PR #4 保留独立 12/12 探针测试，未并入 PR #6。

## 历史记录：P2A-R6 规格入库与隔离 GUI CI（2026-10-25）

## 本轮基线与结果
- 文档分支 `docs/issue-3-p2a-ui-spec` 独立基于 P1 `11b0b9444cd94f0026b855abe3849a06106426f2`，不包含 PR #4 测试代码；已创建 [Draft PR #5](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/5)，base `fix/issue-1-zamak-build`，引用 Issue #3。PR 创建时 head `b94731b9a9bc52a4834358f9f71fcf86a2033925`；本次最后的记忆同步提交会推进 head，最终 SHA 以 PR 远端核验为准。
- PR #2 仍 OPEN / Draft，head `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #4 仍 OPEN / Draft，base `fix/issue-1-zamak-build`，最终本轮 head `e2d3c5e4481e9494472913f41636580369038cd8`，远端 SHA 已核对一致。
- R6 GUI Actions push [40191454446](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454446) 与 PR [40191456330](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456330) 均 PASS；build-and-test push [40191454361](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454361) 与 PR [40191456339](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456339) 均 PASS。Artifacts 已下载核验；含 X11/Wayland 日志、PNG、协议/几何状态和 SHA 清单。
- 9 项既有 Zamak 事件路由测试函数原样保留，新增 3 项紧凑几何/双语用例；Zamak 0.2.0 共 12/12 PASS。探针源码 SHA-256：`d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。
- Xvfb + Openbox 下实际 XTEST 验证 hover/pressed/release、按钮及箭头命中、菜单、Tab/Enter/Escape、双语和 resize；同按钮 ROI hover→pressed、pressed→release 差异均为 8004 像素。WM close 通过 Alt+F4 传递 WM_DELETE，记录 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`、退出码 0 且窗口消失。`xdotool windowclose` 实际用 `XDestroyWindow` 销毁 X 窗口，不作为 WM close 证据。
- Weston Headless/Pixman Wayland socket、312×324 Canvas 绘制、受控退出及进程退出码 0 均 PASS。每个运行记录一次 `gdk_seat_get_keyboard` critical，未公告 `wl_seat`；两者相符但因果未证实，未隐藏 GTK 诊断。

## 文档与尺寸约束
- 已整理 P2A 源码规格、互动矩阵、Linux 能力映射、版本差异、可行性及真实 Windows 截图采集清单；官方基准固定 `v.3.5.2` / `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 新增明确尺寸规则：主对话框 224×214 DLU、五个设置页模板各 260×168 DLU；生产 GUI 必须保持官方紧凑比例，不得放大。R5/R6 336×348 顶层、312×324 Canvas 仅为 96-DPI 估算的 CI 探针尺寸，不是 Windows 测量或产品目标。
- 官方 Windows 原生截图及真实 DLU→物理像素换算仍 BLOCKED；overlay/diff/2px 结果 NOT RUN。336×348 top-level / 312×324 Canvas 仅为 CI 探针尺寸。没有新增或公开私人截图/路径。

## 本机只读/非 GUI 验证
- `python3 scripts/verify_project.py` PASS（29 项）；`python3 -m unittest discover -s tests -v` PASS（3 项 C 安全测试）。
- 所有 `scripts/*.sh` 语法检查 PASS；Zamak `0.2.0`、`zam check .`、`zam test .`（5 passed）、`zam build .`、`bash scripts/build-arch.sh`、`git diff --check` PASS。
- 未启动 GUI/Compositor，未使用 Hyprland IPC、鼠标键盘注入或特权清理接口；未触碰 Swap/ZRAM/Polkit。
- 规格分支更新官方 3.5.2 源码规格、资源/交互矩阵、Linux 语义、安全边界、真实 Windows 截图缺口、Zamak 技术结论及紧凑尺寸约束；长期规则写入 AGENTS/PRD/ARCHITECTURE/LONG_TERM。PR #5 创建时的源码规格 head `90d62100e840aa408e4ea99cbbd866ef2025e99e` 上，build-and-test push `40191896559` 与 PR `40191901022` 均 PASS。记忆同步提交会使 head 前进，但不改程序源码；最终 branch SHA 以远端校验为准。Windows release 截图和真实像素比较仍 BLOCKED。
- 未启动本机 GUI 或 compositor，未访问 Hyprland IPC，未执行 Polkit、内存清理、Swap/ZRAM 操作。PR #2/#4/#5 不合并；P2B 不开始。下一步等待 PR #5 Actions 完成并交付两个 Draft PR 供复审。


## 历史记录：P2A-R6 隔离 GUI CI 与规格提交（2026-10-25）

- 规格分支 `docs/issue-3-p2a-ui-spec` 基于 P1 `11b0b9444cd94f0026b855abe3849a06106426f2`；已提交源码级规格首版 `367efce8dcdecb487595507ff7e9f275bedd96bc`，仍待更新最终 CI 记录和创建独立 Draft PR。
- 测试分支 `test/issue-3-isolated-gui` 的 R6 验证源码 SHA `1cb7b77246275962f092fa0c0ccc27fbf982ce8b`。PR #4 为 OPEN / Draft，base P1 分支；PR #2 为 OPEN / Draft。没有改 main、P1 功能或生产 UI。
- 保留原 9 个 Zamak 测试函数，新增 3 项紧凑几何/双语测试；Zamak 0.2.0 `check/test/build` PASS，12/12 probe tests PASS。探针 SHA `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。
- X11 修正：`xdotool windowclose` 的上游实现是 `XDestroyWindow`，不发 WM_DELETE，不能用于正常 GTK 关闭验收；调用它导致异常销毁和 GTK critical。隔离 Xvfb 下启动 Openbox 后，实际通过 XTEST Alt+F4 验证 WM close：`WM_PROTOCOLS` 含 `WM_DELETE_WINDOW`，日志顺序包含 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`，退出码 0 且 X 窗口消失。Openbox 运行后 `xdotool windowquit` 尝试仍未触发回调，未将其标为通过。hover-only 与 held-press ROI、pressed 与 release ROI 像素差各为 8004。
- R6 Push/PR CI：GUI runs [40191087455](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191087455) / [40191091618](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191091618) PASS；build runs [40191087528](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191087528) / [40191091733](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191091733) PASS。Artifacts 附有截图、ROI 差异、源码/二进制 hash、协议状态和日志。
- Weston 13 Headless/Pixman：Wayland socket、Canvas 312×324 绘制、事件循环返回、退出码 0 PASS；没有广告 `wl_seat`，每次记录到一次 `gdk_seat_get_keyboard` critical。缺失输入 seat 是相符解释但非已证因果；没有屏蔽 GTK 诊断。
- 文档尺寸硬约束已明示：Mem Reduct 3.5.2 主窗 224×214 DLU；五个设置页模板各 260×168 DLU。生产 GUI 必须维持原版官方紧凑尺寸，禁止放大；CI 336×348 仅为探针估值。真实 Windows 截图、DLU→物理像素实测、Hyprland 输入、HiDPI 和 2px overlay 均 BLOCKED / NOT RUN。
- 本机非 GUI 检查：`verify_project.py` 29 项、C 测试 3 项、4 个 shell 脚本语法、Zamak 0.2.0 check、5 项项目 tests/build、Arch helper build、`git diff --check` 均 PASS。未操作本机 GUI/Hyprland IPC，未执行 Polkit、drop_caches、Swap/ZRAM 操作。
- 下一步：更新规格分支记忆/文档，推送并创建独立 Issue #3 Draft PR；保留 PR #4 Draft，等待 ChatGPT 复审；不合并 PR #2/#4，不进入 P2B。
- 纠正文档/诊断后的 PR #4 head `6ad0f401d2b831ee71a1387a858e7456bbe444e5` 再次通过全部 CI：GUI push/PR `40192348460` / `40192351634`，build push/PR `40192348489` / `40192351591`。Artifacts 重新上传；probe SHA 未变。本轮没有生产 UI 或特权代码改动。

## 历史记录：P2A-R5 尺寸修正与隔离 GUI CI（2026-10-25）

- 工作分支 `test/issue-3-isolated-gui`，P1 基线 `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #2 仍 OPEN / Draft、未合并。PR #4 仍 OPEN / Draft，base 为 `fix/issue-1-zamak-build`。
- 用户要求 GUI 按官方紧凑尺寸，不要放大。核对 Mem Reduct v.3.5.2 tag 源码 `2f5c55d15f69a2c88859e036b16c6f4c971e1968`：`IDD_MAIN` 为 224×214 Dialog Units，字体 `8 pt Ms Shell Dlg`。以 96 DPI 常见 6×13 px base-unit 估算，测试窗口请求 336×348；GTK Canvas 实测 312×324。该换算尚未在真实 Windows 上测量，也不是像素级基准。
- 仅收紧无特权 Canvas 探针窗口、修正 GTK Tab/Enter 字符事件名称并缩短占位文案；没有改生产 UI、P1 构建链路或特权功能。9 项已有路由测试函数体保持不变。
- 本机：`python3 scripts/verify_project.py` PASS（29 项）；C unittest PASS（3 项）；7 个 shell 脚本 `bash -n` PASS；Zamak 0.2.0 probe check/test/build PASS；9/9 probe 测试 PASS；源哈希验证和 `git diff --check` PASS。没有在本机启动 GUI、触碰活动桌面、安装依赖或执行内存/Swap/ZRAM操作。
- GitHub：测试源 SHA `6e0a4a79d56633603a298e5e0ef6520028c71b80` 的 push run [40187178574](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40187178574) 与 PR run [40187181722](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40187181722) 均 PASS；Wayland Headless/Pixman 与 Xvfb/XTEST 两个 job 均通过，截图、版本、日志和测试记录已作为 artifact 上传。
- 仍未完成：真实 Windows 3.5.2 截图与像素对照、Hyprland 实机输入验收、正式生产 UI 复刻。P2A 尚不可宣称正式验收；等待 ChatGPT 复审 PR #4，不进入 P2B、不合并任何 PR。

---

## 历史记录：P1 Zamak 0.2.0 编译与原生构建验收（2026-10-25）


## 执行范围
按 Issue #1 修复 Zamak 0.2.0 编译/构建问题，保留现有 GUI 与特权边界；运行本机 Arch/Omarchy 验收，准备关联 Issue 的 Draft PR。工作分支 `fix/issue-1-zamak-build`，基于 `origin/main` / `080417e153d91bf2adac756e5154311fef49333f`。未修改 main，未扩展功能。

## 实际改动
- `zam.toml`：Zamak `project.name` 从非法连字符形式改为合法标识符 `memreduct_zamak`；产品显示名、仓库名、安装命令和 helper 仍保留 `memreduct-zamak`。
- `src/memory.zm`：将不存在的无参 `split()` 调整为 Zamak 0.2.0 支持的 `words()`，并正确传播解析错误。
- `scripts/install-arch.sh`：按 `memreduct_zamak-<hex-hash>` 匹配 GUI 二进制，避免把 `-test-` 测试产物当作应用。
- `scripts/verify_project.py`：将 manifest 标识符检查同步为 `memreduct_zamak`。

## 本机验证证据
环境：Omarchy 4.0.6、Hyprland 0.56.2、Wayland、GTK 3.24.52、Zamak 0.2.0。

| 命令 / 验收 | 状态与实际结果 |
|---|---|
| `python3 scripts/verify_project.py` | PASS，29 项检查。 |
| `python3 -m unittest discover -s tests -v` | PASS，3 项 C 安全测试。 |
| `bash -n`，逐个检查 `scripts/*.sh` | PASS，4 个脚本。 |
| `zam --version` | PASS，`0.2.0`。 |
| `zam check .` | PASS，`CHECK OK memreduct_zamak`。 |
| `zam test .` | PASS，5 passed，0 failed。 |
| `zam build .` | PASS，输出 `build/memreduct_zamak-00ee9d44b77125b6721133332f35be3f1f9d3d518e7ab20b40d8265940928d57`。 |
| `bash scripts/build-arch.sh` | PASS；Zamak 检查/测试/构建及 C Helper 均成功。 |
| Helper `--dry-run` | PASS；未修改内核状态。 |
| Hyprland GUI smoke test | PASS；普通用户 Wayland 窗口，中文/内存数据/控件/刷新可见，`xwayland=false`，正常关闭，退出码 0。未点击清理、未启用自动清理。 |

安装器候选扫描只匹配 GUI 产物；之前的动态库检查未发现缺失库。未执行涉及 `/usr` 的安装/卸载；Polkit E2E、真实 `drop_caches`、Swap/ZRAM 操作均未运行。无任何真实内核清理写入。

## GitHub / 下一步
- 当前已验证 `gh` 账号为目标 `MilchstraBe-Endless10`；目标仓库仅为 `MilchstraBe-Endless10/memreduct-linux-zamak`。
- 代码提交 `1d4654867f59154981010007288cff1fda42f457` 使用 GitHub noreply 邮箱；文档验收记录后续提交 `9b5177e992eb1c86b72643a0ae2c0cda9a570555`。记录该状态时，`git ls-remote` 与本地 HEAD 相同，工作树干净。
- Draft PR #2：<https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/2>，base `main`，该次核验的 head 为 `9b5177e992eb1c86b72643a0ae2c0cda9a570555`，正文 `Refs #1`。PR 仍为 OPEN / Draft。
- Actions `build-and-test` 对代码提交成功：push run [40170617141](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170617141)，pull_request run [40170634633](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170634633)。只含文档记录的后续提交也成功：push run [40170749833](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170749833)，pull_request run [40170752581](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170752581)。
- 等待 ChatGPT 复审及用户决定是否合并；不得自动合并或进入 P2。
