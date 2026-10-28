# 短期记忆 / Latest Handoff
## 当前真实状态（2026-10-28；PR #7 main-base CI 已验收）

- **主线及 PR：** `main@83dbf58fb039645f08e08bc89170adcdaa7997be`。PR #7 为 OPEN / Draft、未合并，Base=`main`，Head=`019a7f1e0755570c2c3a0b1202a6c764540f14e2`，Tree=`5cd9e6fe978570bc63fc626e5bccaccf5a43cc5f`，双亲依序为 `b7ea31e6aea121b0b606866704f52ac0ddfe8be8` 和 `83dbf58fb039645f08e08bc89170adcdaa7997be`。GitHub 当前报告 `mergeable=true`。改 Base 没有修改 main、PR HEAD 或提交历史；未标记 Ready、未合并。
- **main-base CI（均针对 PR #7 Head `019a7f1…` 的新运行）：** [Build 40653975796](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975796)、[P2A GUI 40653976301](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653976301)、[P2B production GUI 40653975764](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975764)、[P2C Windows/Linux 40653975755](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755) 全部 SUCCESS。Build 的治理、Shell、安全、Zamak check/test/build 步骤均 PASS。P2A 的 Xvfb/XTEST 与 Weston/Pixman 两条 lane 均 PASS，Canvas 探针 12/12、APT update/install 退出码 0；Weston 仅验生命周期，不声称真实键鼠输入。P2B 的生产 Xvfb 与 Weston 两条 lane 均 PASS；Xvfb 30 项 Zamak tests、C 安全 3/3、Alt+F4/WM close 退出码 0。P2B workflow 内设置页步骤按分支条件跳过；设置页覆盖由 P2C 专用工作流执行，不构成覆盖缺失。
- **真实 Hyprland 启动检查（2026-10-28，本轮）：** 本机环境为 `XDG_SESSION_TYPE=wayland`、`WAYLAND_DISPLAY=wayland-1`、Hyprland instance 存在，GTK 3.24.52、Zamak 0.2.0。以普通用户和 `GDK_BACKEND=wayland` 启动当前 PR #7 构建产物；`ss -xap` 显示该进程与 Hyprland 的 `wayland-1` socket 有已建立连接，进程持续运行约 8 分钟，无 stdout/stderr。随后只对本轮启动的测试进程发送 SIGTERM 结束；这不是 WM_CLOSE/Alt+F4 验证。Codex UI 自动化返回原生 app 列表为空，且 `cua.listWindows` 不可用，因此没有对活动桌面发送键鼠输入、截屏或声称交互 PASS。当前主窗口几何 337×369 仍是临时估算；源码会按实际 Canvas 分配比例缩放控件，可能解释 tiled 窗口放大，但缺少本轮窗口截图和尺寸观测，根因未确认，未改生产代码。
- **本机隔离 GUI 运行限制：** `Xvfb`、`openbox`、`weston` 均未安装。生产 Xvfb/XTEST 脚本和私有 Weston 生命周期脚本尝试后均因首个缺失工具退出码 2；未安装系统包。对应 PR HEAD 的 GitHub Actions 仍为上述 8/8 jobs PASS，不能替代本机实测。治理 33 项、C/Python 安全 3/3、Shell 语法、Zamak 0.2.0 check/test（30/30）/build、生产 GUI 构建本轮均 PASS。
- **P2C 视觉与交互结果：** Windows 原版来源、Linux Xvfb 截图与比较 job 全 PASS。几何 184/184、控件栅格 36/36、导航选中 10/10、树线像素 102/102、中间滚动 2/2；10 组 P2C-2 回归全部改善。深色语义配对 20/20，主题/语言切换顺序比较 10/10，深浅色几何 210 个资源矩形一致。整页像素 1:1 仍 **NOT PASS**。12 项简中来源差异继续 `DECISION_REQUIRED`；8 条历史 review threads 仍 outdated / unresolved。完整指标和图片清单位于 [P2C 比较 artifact 12801450123](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755/artifacts/12801450123)，ZIP SHA-256 `9dfe05278ca8bca375ce9630f35d4d3480340aa7fdf368685b6d9deac3fc62d6`；[Windows artifact 12801170097](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755/artifacts/12801170097)，SHA-256 `0ce2c551f8b7081256c8cd76ac9a3568a965bb8e994aca108389ca783f50f201`；[Linux screenshot artifact 12801041004](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755/artifacts/12801041004)，SHA-256 `b7d821bb58af856723b249a547b8c862940d3340be23a12732b626258d664b4a`。
- **阶段边界：** PR #7 尚待独立复审；真实 Wayland/Hyprland 键鼠输入 NOT RUN，托盘与清理确认/结果仍受限，实际清理、Polkit、Swap/ZRAM、root GUI 和活动桌面操作不在验收范围。ADR-0004 仍 PROPOSED。

## 历史快照（2026-10-28；PR #7 集成提交推送前的本地候选审计）

- **主线已核实：** main@83dbf58fb039645f08e08bc89170adcdaa7997be 是 PR #9 文档/记忆 Merge Commit，Tree a10f22817479a9e91052420f40921e8446fd3a24；父提交依次为 c0f616e5b0f3c7511cedcfe7d0b4536e8d54f0f2 和 de19d6aa7bb9eb7cf82fd253b6b6d4e4cff200aa。该 merge 相对第一父提交只改 memory/MEDIUM_TERM.md、memory/SHORT_TERM.md。PR #9 CLOSED/Merged，主线 Push Build [40642082611](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40642082611) SUCCESS。
- **PR #7 远端状态只读复核：** OPEN / Draft，head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`；base 分支仍为 `feat/issue-3-p2b-main-window`，该远端分支当前 HEAD 为 `483ac837cd51d3293baf6c3d8b72309dd02f2ff9`，且已是 main 的祖先。PR API 的 `base_sha=911a3d0b99b05f15bd0109dca475b0b5174e7888` 与旧 head 的共同祖先相符；PR 仍报告 `mergeable=false`。8 条历史 Review 线程均为 outdated / unresolved。本轮没有写入远端。
- **三方结果：** merge-base 为 911a3d0b99b05f15bd0109dca475b0b5174e7888。git merge-tree --write-tree 返回 tree 73ab4bed3a3a9c0d09f0c4b63caf996a007ab030、退出码 1，确证四个内容冲突：.github/workflows/p2b-production-gui.yml、memory/MEDIUM_TERM.md、memory/SESSION_LOG.md、memory/SHORT_TERM.md。.github/workflows/test.yml 自动合并；scripts/ci/apt-diagnostics.sh 两侧 blob SHA 均为 dabb4a04b74c8e9763ccfda7dc1bc170dede8971。
- **本地验证：** 治理 33 项、Python/C 安全 3/3、Shell 语法、Zamak 0.2.0 check/test（30/30）/build、Arch 原生构建、Canvas 探针 check/test/build（12/12）、四个 workflow YAML 解析、对 main 的 diff check 均 PASS。该环境没有 Xvfb、Weston 或 actionlint，故生产 XTEST/Weston 运行与 actionlint 为 NOT RUN；候选未推送，所以新 HEAD GitHub Actions 也 NOT RUN。
- **集成净差异：** 候选相对 PR #7 原 head 有 23 个路径变化；相对 main 有 29 个路径变化、无路径删除。PR #4 探针及 PR #6 主窗口验证脚本保留；`p2b-production-xvfb.sh` 仅调整已测量窗口内容内缩后的 XTEST 点击坐标；`p2b-production-wayland.sh` 在保留主窗生命周期检查的基础上增加私有 Weston 设置窗表面/退出验证，不声称 Wayland 输入；`build-production-gui.sh` 纳入设置 UI 与 GTK bridge 源文件 SHA。采样失败清空旧值、重绘和 `em-dash` 检查仍在。P2C 几何 184/184（最大分量偏差 1.5 px）与局部采样结果不代表整页像素 1:1；该项仍 NOT PASS。真实 Wayland/Hyprland 输入 NOT RUN，托盘与清理确认/结果仍受限，12 项简中来源差异仍 DECISION_REQUIRED。
- **安全边界：** 未执行内存清理、Polkit、Swap/ZRAM、root GUI 或活动 Hyprland 操作；ADR-0004 仍 PROPOSED，不实现偏好持久化。

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

## PR #7 P2C 分支历史快照（来源 head b7ea31e6；记录于 2026-10-27）

- 开始本轮时分支与 `origin/feat/issue-3-p2c-settings-ui` 同为 `663a9d3e627ccb2d9bcb884cd708f140006f58a5`；PR #7 OPEN / Draft，base #6 `911a3d0b99b05f15bd0109dca475b0b5174e7888`。本轮待提交内容是集成报告时间/文件数更正及记忆补录，未改生产 UI、测试源码或 workflow。
- PR #7 当前 checks 7/7 PASS。P2C run [40463959137](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463959137) 原版采集、生产 Xvfb、比较 3/3 PASS；P2B run [40463959086](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463959086) 最终 Xvfb/Weston 2/2 PASS。Weston 首次 attempt 因 15 秒内无有界退出标记超时，失败 artifact `12716375114` 保留；只重跑失败 job 后 artifact `12716440984` 对应成功 attempt 2，根因仍未确认、无 compositor 崩溃证据。构建 PR/push runs `40463959092` / `40463951002` PASS。Artifact IDs、digests 及视觉指标详见集成报告。
- 该 HEAD 的几何 184/184（最大偏差 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36、滚动比较 2/2；Windows 四组主题/语言实验 4/4、页面状态 20/20、设置窗重开 4/4。整页像素 1:1 NOT PASS，Wayland 输入 NOT RUN，Windows tray/清理弹窗 BLOCKED，12 项中文来源差异仍 `DECISION_REQUIRED`。
- PR #2/#4/#5/#6/#7 均 OPEN / Draft，远端 head/base 与集成报告表格一致；无 PR 合并、main 或其他分支修改。P3A Issue [#8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8) 已建；ADR-0004 为 PROPOSED、未批准且未实现。实际仍无清理、Polkit、Swap/ZRAM、活动 Hyprland 或主机桌面操作。
- 本轮本机验证：`verify_project.py` 33 项 PASS；C/Python 安全测试 3/3 PASS；Shell 语法 PASS；Zamak 0.2.0 `check` PASS、`test` 30/30、`build` PASS。仅文档和记忆有改动，未执行本机 GUI、Arch 安装构建或特权测试。

## P2C-EXIT / P3A-PRE 当前交接（2026-10-27）

- 当前工作分支 `feat/issue-3-p2c-settings-ui`；P2C-4R1 已复审 HEAD `556a727c100aad4af1a25c48c10513c4dd42c5c7`，生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`，图形采集/比较源码 checkpoint `37d07453d60c4d1046f5aad6577c9425c271180a`。P2C-EXIT/P3A-pre 首次文档提交 `b1f2fd95333a26af1c3ea51e6fea4a245db640c9` 的 exact-head CI 已完成；本条目随后的提交只补充该运行证据与术语更正，不改生产或测试代码。
- b1f2fd9 P2C run `40461922846` 3/3、P2B `40461922577` 2/2、PR build `40461922736` 与 push `40461917796` 全 PASS（PR checks 7/7）。Artifacts：Windows `12714602648`、Linux Xvfb `12714528369`、visual `12714648221`、P2B X11 `12715275894`、Weston `12714144687`；digest 见 P2C report。几何 184/184、raster 36/36、nav 10/10、tree 102/102、滚动 2/2 compare-only；P2C2 回归 10/10 improved，20/20 深色页面状态配对有效。整页 1:1 NOT PASS。
- 四组 Windows 新进程实验 4/4、页面语义 20/20、设置窗重开 4/4 PASS。几何 184/184（最大 1.5 px）、导航 10/10、树线 102/102、栅格 36/36 PASS；滚动状态 2/2 compare-only。深色 changed-pixel EN 5.9434%–12.6625%、简中 5.4004%–12.4014%；整页 1:1 NOT PASS。Wayland 输入 NOT RUN，Windows tray/清理对话框 BLOCKED，12 项中文差异 DECISION_REQUIRED。
- 新增文档：`docs/PR_INTEGRATION_READINESS.md`、`docs/ADR/0004-user-preferences-persistence.md`（PROPOSED）、P2C-4 报告归档与 UI 交互矩阵 P3A 状态；long-term 仅更新已接受证据 snapshot，未把 ADR 草案升格为永久架构决策。
- PR #2/#4/#5/#6/#7 全部 OPEN / Draft，没有合并。只读 merge-tree 发现 #4 与 P2A/P2B/P2C 的 `memory/SESSION_LOG.md`、`memory/SHORT_TERM.md` 冲突，集成必须人工合并保留双方记录。PR #4 的 12 项 probe tests 与生产 Xvfb/Weston tests 是不同能力，二者都要保留。
- P3A 只规划语言和应用主题的用户级持久化；跟踪 Issue [#8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)。目标 XDG 配置、schema 1、atomic/no-follow/0600 策略、Reset 事务与临时目录测试均见 ADR 草案。实现尚未开始；清理、Polkit、自动清理、Swap/ZRAM 仍禁用。
- 创建并核验 P3A Issue [#8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)，PR #7 顶层交接评论为 [issuecomment-6435453708](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#issuecomment-6435453708)。没有执行真实清理、特权写入、用户桌面/Hyprland IPC、root GUI 或远端合并。本轮未运行本机项目测试；仅做文档 diff 检查，并使用上述 GitHub-hosted Actions。P3A ADR 仍 PROPOSED，等待 ChatGPT/用户架构复审。
## P2C-4 前序图形证据 snapshot（历史；2026-10-27）

- PR #7 OPEN / Draft；base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；当前截图/比较 evidence head `4f84fb9a9e247a5987a11cd435e5da4b05f78a53`。生产 UI 源码 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b` 未改；当时最新正式 ChatGPT Review 为 P2C-3R1；之后的 P2C-4 review 及 P2C-4R1 状态见本文件开头。
- P2C run [40425954672](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425954672) 3/3 PASS；Windows artifact `12699069397` (`sha256:b9ce68670d7137d24923e64532d5a301d99e743fc423c6935165d5ad8181e1fa`)、Linux X11 `12700585092` (`sha256:e69b53bcf64021e8f0b2f0d70b10fe22ac24e9c00ed46efd32799b1c7264ab53`)、visual/dark/font `12700001976` (`sha256:209f4664b942017097bd17d5734d98b4983a58de3561ddab01739fc692e08bf7`)；APT diagnostics `12698999880` / `12700196476`。
- P2B run [40425954460](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425954460) 2/2 PASS；X11 artifact `12699671782` (`sha256:f1b1dc344fba5de07f30bcde18689694b286dc8d9690c1ffcea8c718229ad305`)、Weston `12700250527` (`sha256:6233aae01f776ac2a03da09c7129fb9b9036bad01e51802ad511a2b593575595`)；APT diagnostics `12700215495` / `12699741667`。PR build `40425954317`、push build `40425944842` PASS；PR checks 7/7。
- Manifest/hash 核验：Windows PNG 41/41、Linux PNG 44/44、comparison 83/83、dark 70/70、P2C source 10/10、P2B X11 PNG 10/10、P2B source/binary 8/8。资源几何 184/184（最大分量 1.5 px）、nav 10/10、tree 102/102、raster 36/36、scroll 2/2 PASS；light changed-pixel 5.0741%–14.8853%，dark 5.9434%–73.2932%，整页 1:1 NOT PASS。
- 文本度量 132 行中，120 项同源文本有 9 项宽高差≤2 px、111 项仍不同，另 12 项来源差异保持 `DECISION_REQUIRED`。Liberation Sans 隔离实验：英文 66 个样本改善 64、退化 1、不变 1；简中 54 项改善 12、退化 12、不变 30，未采用到生产。Windows 专有字体没有打包。
- Ubuntu 24.04.5 / APT 2.8.3 的本次 update/install 均成功：P2C Xvfb 101 MB/13s，P2B Xvfb 101 MB/7s，Weston 79.6 MB/25s，Pillow 503 kB/<1s。历史慢速下载原因未确认。Weston 主窗和设置窗客户端退出码 0；保留 seat 与 layer teardown 诊断。Headless 输入 NOT RUN，P2B 重复五页步骤跳过由 P2C 覆盖。
- Windows tray、没有安全采集路径的清理 popup/确认/结果仍 BLOCKED。没有真实清理、Polkit、Swap/ZRAM、root GUI、Hyprland IPC 或主机桌面输入。完整证据与结果见 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。

### 前一个完整截图/视觉 artifact snapshot（105d71d，2026-10-27）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；以下 screenshot/visual evidence snapshot 使用 head `105d71d567d7ba096e8b928c072ceb4af15bfe6f`，生产 UI checkpoint 为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。之后本分支只同步报告/矩阵/记忆，未改字体、字号、Canvas 或工作流阈值；当前 PR check 状态以 PR 页面显示为准。
- P2C run [40416882722](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416882722) Windows native、Linux production Xvfb/XTEST、visual compare 3/3 PASS；Windows artifact `12695303623` (`sha256:db33cc90dcd9aa0b8b9678adfe44fed0d6ff6733ee4ae6087603b94bc662e7e7`)，Linux `12694879677` (`sha256:424d9d863a3c6a7d2c2f66224150f58e34aec9b62a4f6196131f0c5b4048a782`)，visual/dark/font `12694609240` (`sha256:5b3e9348af249a02ceb7871d76d4d5cb7f7d90e5647dfc7456e5d2726e3caadc`)；APT diagnostics `12695029426` / `12694434533`。
- P2B run [40416882708](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416882708) production Xvfb/XTEST + Weston Headless/Pixman lifecycle 2/2 PASS；X11 artifact `12696373914` (`sha256:bf70d1f1b7a5fba60ed9ef175071009c12c411f887399e38ab5841ac2c04a9d3`)，Wayland lifecycle artifact `12695762656` (`sha256:dbe4352648a14e4660b2edf518ede8bc1406aec720a4b6a2dd92aeda72d23adb`)。重复五页设置 step 条件跳过，由同 head P2C 完整执行。PR build `40416882739`、push build `40416873128` PASS；PR checks 7/7。
- P2C artifact 核验：Windows PNG 41/41、Linux PNG 44/44、visual 83/83、dark 70/70、P2C source 10/10；P2B X11 PNG 10/10、源码六项及生产/helper 二进制两项 SHA 均匹配。几何 184/184（最大分量偏差 1.5 px）、导航 10/10、tree 102/102、raster 36/36、滚动 2/2 PASS；亮色 changed-pixel 5.0741%–14.8853%，暗色 5.9434%–73.2932%，整页 1:1 NOT PASS。
- APT 诊断确认 Ubuntu 24.04.5 runner image `ubuntu24 20261023.334.3`。P2B Xvfb update 获取 12.6 MB / 1s；安装 101 MB / 36m11s（工作流 step 36m23s），其中 `fonts-noto-cjk` 61.2 MB，来自 `azure.archive.ubuntu.com`，退出码 0。日志没有 DNS、锁或缺包错误；慢速事实已确认，根因仍未确认。
- 字体候选未进生产；Windows 专有字体未打包。12 项中文来源差异仍 `DECISION_REQUIRED`；Windows tray、无安全采集路径的 clean popup/confirmation/result 仍 `BLOCKED`。Weston 只验生命周期，Wayland 输入 `NOT RUN`；无真实清理、Polkit、Swap/ZRAM、root GUI、Hyprland IPC 或本机桌面输入。

### 前一 exact-head 核对（33367f3）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，核验 head `33367f3c15273f3e817bba27e9272a19663ea829`；生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。本轮未改生产字体/字号/UI 源码或比较阈值。
- P2C run [40408926862](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408926862) 的 Windows、Linux production Xvfb/XTEST、比较 jobs 3/3 PASS；artifacts Windows `12691828080` (`sha256:ca0e98ec99ce5332956c037271fb91fbcb7cb4ac11fa086e050eedabaf6b4542`)，Linux `12692965976` (`sha256:9171f0ca79938f360cc36b4937744f65c1f4b42b874ed7ec7244be27c4c88f3f`)，visual/dark/font `12692891146` (`sha256:115c23d9d3d1d90cce265cab20a68176d5fbd104bd76e59728cb59a0d1b186bd`)。
- P2B run [40408926842](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408926842) Xvfb/XTEST 和 Weston Headless/Pixman 2/2 PASS；X11 artifact `12694248993` (`sha256:5f14c003ca627693f928e1d54384afeafa683cb66ceba14f7ab6728327cdf7ac`)，Wayland artifact `12690409508` (`sha256:58cf6b8da7c450ad0374892282d444e92869a1f042f0810d6ae1efcc0ba305d9`)。P2B 五页重复 step 条件 `SKIPPED`，由 P2C 同 head 覆盖。PR build `40408926778`、push build `40408920155` PASS；PR checks 7/7。
- 首次 Xvfb APT update 成功；install 于 `fonts-noto-cjk` 61.2 MB 下载阶段触发 38 分钟硬超时，exit `124` / `command timeout`。未见 DNS、锁或缺包证据，底层网络/镜像原因未确认。只重跑失败 job 后安装 exit `0`。attempt 1/2 diagnostics artifacts `12693989271` / `12693778915`；本次成功不证明长期网络稳定。
- 哈希覆盖核验：Windows PNG 41/41、Linux PNG 44/44、visual 83/83、dark 70/70、P2C source 10/10、P2B source/binary 8/8。主程序/helper SHA-256：`fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404` / `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。几何 184/184（max 1.5 px）、导航 10/10、tree 102/102、raster 36/36、scroll 2/2 PASS；light changed-pixel 5.0741%–14.8853%，dark 5.9434%–73.2932%，full-page pixel identity NOT PASS。
- Windows 官方 v3.5.2 provenance 已验证；Linux XTEST 覆盖五页双语及菜单，清理仍禁用。Liberation Sans 未进入生产；Weston 仅验收隔离生命周期，保留 GTK seat critical，Wayland 输入 NOT RUN。12 项语言差异 `DECISION_REQUIRED`；Windows tray 与安全条件不满足的 clean popup/confirmation/result `BLOCKED`。没有真实清理、Polkit、Swap/ZRAM、root GUI、活动 Hyprland 或本机桌面输入。详见 [P2C-4 report](../docs/P2C-4_FONT_THEME_MENU_REPORT.md)。

### 前一 exact-head 核对（f4c107b）

`f4c107b5ef6bbc34a4a78fd3600dbfe4c4e81161` 的早先 P2C/P2B 运行、artifact 与 metrics 已由 P2C-4 报告保留；它不是当前最新的 Actions evidence snapshot。

### 前一 exact-head 核对（d238649）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，当前核验 head `d23864905f7e841eb428135bb1da244abca5d1df`；生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。最新 ChatGPT review 仍是 P2C-3R1 对 `2c745710...` 的审查：CI gate accepted，整体 1:1 NOT PASS。
- P2C run [40393863774](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863774) Windows native、Linux Xvfb/XTEST、比较 3/3 PASS。artifacts：Windows `12687194357` (`sha256:39d347c00ab6cac66a155fcdd3c0c3b76ece305d49a3893872efc3e4239c82e0`)，Linux X11 `12690875520` (`sha256:880a6b11f3cf1555c082d511568ee6d486271b94ffbfdad79db910fc1622612d`)，comparison `12689194953` (`sha256:fa9abb4a9b835a231dfc288c73de6d1d3654e8bd636480977d532b4b93e89312`)。
- P2B run [40393863850](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863850) Xvfb/XTEST 与 Weston Headless/Pixman 2/2 PASS；artifacts X11 `12688395963`、Wayland `12686999481`。PR build [40393863878](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863878) 和 push build `40393857503` PASS，PR checks 7 PASS / 0 FAIL。
- 本轮下载核验 PNG/source hashes：Windows screenshots 41/41、Linux screenshots 44/44、visual 83/83、dark 70/70、P2C source 10/10、source+binary 8/8。几何 184/184（max 1.5 px）、nav 10/10、tree 102/102、raster 36/36、scroll 2/2 PASS。Light changed pixels 5.0741%–14.8853%；dark 5.9434%–73.2932%，均为 compare-only；全页像素 1:1 NOT PASS。
- Windows tag/source/release provenance 与 detached PGP 验证成功；Windows native 菜单仅四项 English。Tray、clean popup 及真实 clean confirmation/result 保持 BLOCKED。字体候选没有达到双语全页收益，因此生产仍 Sans/DejaVu Sans + Noto Sans CJK SC，无 Windows 字体文件进入仓库。
- Xvfb 使用 GitHub-hosted Ubuntu 24.04 / GTK 3.24.41 / Adwaita:light；暗色通过应用 Canvas palette。Weston 13.0.0 Headless/Pixman 客户端退出 0，但保留 `gdk_seat_get_keyboard` critical 和 SIGTERM 后 layer-finalization 诊断；根因未知，不声称有 Wayland 输入验收。当前 P2C APT 成功但下载 69.7 MB 用时 31m33s；旧超时底层原因未确认。
- 没有本机 GUI、Hyprland IPC、用户桌面输入或真实内存/特权操作。证据索引见 [P2C-4 report](../docs/P2C-4_FONT_THEME_MENU_REPORT.md)。

### 更早 exact-head 核对（37cefdb，历史）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；核验 head `37cefdb351b1cfcdacbc6ca24eb8b06fead93cc1`。生产源码 checkpoint 为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；之后调整 P2B/P2C workflow 并发组并同步报告/记忆，未修改 UI、goldens、依赖或验收阈值。最新 ChatGPT Review 仍是针对 `2c745710...` 的 P2C-3R1 评审：CI gate accepted，整体 1:1 仍未通过。
- P2C run `40390082314` 在该 head 上 Windows 原版来源/截图、Linux 五页双语 Xvfb/XTEST、比较均 PASS（3/3）；artifacts Windows `12682119919` (`sha256:9a0463631da7bf4e049f5fe319afc9496f16b0fd2b940fae85363f6c2f845acc`)，Linux `12684755661` (`sha256:9bfb89a4bb10da11da9b42bc865627e36bc50c1e654ee3212ee14c7086ba0af2`)，comparison/dark/font `12683006399` (`sha256:8b765ec896abad794b91cbe80194c3959dc98a9c4fabb83cf2ffccdc276437b2`)。
- P2B run `40390082510` 的生产主窗 Xvfb/XTEST 和 Weston Headless/Pixman jobs 2/2 PASS；重复的五页 step 按条件 SKIPPED，P2C 已在相同 head 完整覆盖。X11 artifact `12682830560` (`sha256:d5e4967f92c7feb51defd5623b08b0135abed00ace0a2d17d5a995b22e901fc4`)，Weston artifact `12682645763` (`sha256:1c68e2e22f6945514b14efb065da7001b3633d3266795895198c2846b5d86524`)。客户端退出码 0；Weston 收到测试清理 SIGTERM 后记录 `BUG: finalizing a layer with views still on it.`，根因未确认，不能称为无诊断的干净 compositor shutdown；没有把它归为应用崩溃。
- PR build/governance/safety run `40390082353` 与 push build `40390075275` 均 PASS。下载 artifact 后 SHA 复核：visual 83/83、dark-theme 70/70、Linux screenshot 44/44、源码 10/10、二进制 2/2。资源几何 184/184（最大偏差 1.5 px）、导航 10/10、树线 102/102、raster 36/36、中段滚动 2/2 PASS；五页双语暗色对照均有原图/overlay/difference/heatmap；整体像素 1:1 NOT PASS。
- Zamak build 版本 0.2.0；CI 产物主程序 SHA-256 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。Linux 测试环境 Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、96 DPI/scale 1、Adwaita:light；Windows Server 2022 build 20348、96 DPI/100%。没有本机 GUI/Hyprland 操作、输入注入或内存特权动作。

## P2C-4 字体/控件绘制/深色主题实施记录（2026-10-27）

- PR #7 仍 OPEN / Draft；Base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；P2C 生产代码/工作流 head `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。Windows v.3.5.2 tag SHA 固定 `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- P2C run `40373093546` Windows 来源、生产 Xvfb/XTEST 与比较 3/3 PASS；P2B run `40373092535` 的主窗 Xvfb/XTEST、Weston Headless/Pixman 两 job PASS；build/governance/safety PR run `40373092652` 与 push run `40373081011` PASS。P2B 重复设置页 step 按条件 skipped，完整设置页交互在同 head P2C run 已执行。
- Artifacts：Windows `12673463315` (`sha256:22eaadc99b82745dc548af7036bca0bafe7f0c0b38ae5956553d9976dcbfddf1`)，Linux Xvfb `12674390489` (`sha256:f0b49b1577dec13c436cb967c65793ed7388ab0030e1fddc644e7636cc64d2f2`)，视觉/字体/暗色比较 `12673073193` (`sha256:0897e9e15d2a5c8eff9616b6d1eef730fc55e2d454be28238bf39a66810388a3`)。SHA 清单：Windows PNG 41/41、Linux PNG 44/44、dark 70/70、visual 83/83 全匹配。
- 几何/局部 raster 回归：184/184 resource rectangles PASS（最大 1.5 px），nav 10/10、tree guide 102/102、discrete raster 36/36 PASS。暗色 10 对 Windows/Linux screenshot 全部生成比较素材；变化率 5.9434%–73.2932%，仅 `CAPTURE_AND_COMPARE_PASS`，全像素 1:1 仍 `NOT PASS`。
- 字体候选：Win32 `MS Shell Dlg 2` / 8.25 pt / weight 400 / ascent-descent 11/2；Linux Pango Sans 8 pt / DejaVu Sans + Noto Sans CJK SC / ascent-descent 10/3。隔离 Liberation Sans 对 English 64/66 度量改善、简中 12/54 改善/12/54 退化；双语全页变化率没有充分收益，生产字体保持不变，不打包字体。
- GDI/Pango ascent+descent 总和均为 13 px，但 ascent/descent 分配不同 1 px；Linux text origin 不是 GDI baseline 的直接等价测量。GDI 字符串宽度/平均字宽已记；当前证据没有独立且可对齐的 character-spacing/tracking 量，未添加字距补偿。
- XTEST 双语验证 File/View/Settings/Help、语言子菜单、键盘 Enter/方向/Esc、分隔区跳过、暗色 5 页、safe clean dropdown 均通过；清理项禁用。Windows tray、原生清理 popup、安全确认/结果对话框仍 BLOCKED。简中 12 项来源差异维持 DECISION_REQUIRED。
- 不操作本机 GUI/Hyprland；无内存清理、Polkit、root GUI、内核写入或 Swap/ZRAM 变化。日志中的 Weston GTK seat critical 未屏蔽、未推断根因。
- 文档/记忆首轮提交 `6d5f4d4b8212913ea12b59c20f0916b62219aeea`、`dc1e112` checkpoint 的旧状态及早期 APT 超时记录保留为历史快照；当前已核验状态以本文件顶部 `6892110` exact-head 证据和 P2C-4 报告为准。P2C-4 的生产源码 checkpoint 仍是 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`，后续提交只同步证据文档/记忆。

## 先前未完成状态记录（已被上方 2026-10-27 核对覆盖）

以下各段保留当时进度，不能作为当前状态。

## 当前续接：P2C-4 字体/控件绘制/深色主题校准（2026-10-26）

### 最新核对（2026-10-26 17:44 UTC）

- P2C `40363399854` 已结束 FAILURE：Windows job PASS，Xvfb 截图 job 因 APT 38 分钟硬超时退出 `124`，comparison job SKIPPED。APT diagnostic artifact `12672670397` 的日志中 `fonts-noto-cjk` 61.2 MB 包下载未完成，之前出现 `Ign` 后重试；未看到 DNS、锁或缺包错误，底层网络/镜像根因未确认。字体候选及 Linux 截图均未执行。
- P2B `40363344884` 在 17:44 UTC 仍处于 Xvfb 与 Weston 的依赖安装中，未取消、不预判结论。
- 本轮暗色 palette 改动仍为未提交工作树内容；不改变 Light palette、字体字号、几何、文案、Windows goldens 或验收阈值。workflow 正在准备共享同一 PR 并发组、`cancel-in-progress: false`，压低两条 GUI workflow 的同时 APT 争用，但保留各自独立覆盖。
- 重新执行本机非 GUI检查：Zamak 0.2.0 `check/test/build` PASS，30/30 Zamak tests；`verify_project.py` 33 项 PASS；Python/C safety 3/3 PASS；Shell syntax、Arch build 与 `git diff --check` PASS。未在本机启动 GUI、Xvfb、Wayland 或操作 Hyprland。
- PR build run `40363344887` 和 push build run `40363337339`（head `eea82a8...`）PASS；当前远端视觉结果仍没有新 Linux截图。Windows 41/41 PNG SHA 核验仅是上游基准证据，不是暗色修复验收。
- 17:50 UTC 更新：P2B run `40363344884` 的 production Xvfb/XTEST job PASS，APT 安装用时约 35 分钟；它只验证旧推送头，设置页重复场景按条件 SKIPPED。Weston lifecycle job 仍卡在自己的 APT 安装阶段，仍无终态。此次观察支持“Hosted APT 安装极慢”的症状，但不能证明单一根因。
- 17:52 UTC 终态：P2B run `40363344884` FAILURE；Xvfb job PASS，Weston job APT exit `124`。Wayland diagnostic artifact `12672539453` 中同样显示 `fonts-noto-cjk` 61.2 MB 下载未完成。Xvfb apt artifact `12673795414` 显示相同版本依赖最后成功，约 35 分钟。当前工作树已移除仅英文 Weston 生命周期所需作业中的无用 Noto CJK 下载，并增加跨 P2B/P2C 的同 PR 并发控制；尚未推送/验证。

- PR #7 OPEN / Draft，branch `feat/issue-3-p2c-settings-ui`，base `feat/issue-3-p2b-main-window`；本轮起点为 P2C-3R1 已验收 HEAD `2c745710210c8794b52a0e9eb19dd1830c00d8d6`，当前推送 HEAD `eea82a8c2cafb16b25acf3d319e084335548356d`。本地未提交改动已按修复前真实 Windows/Linux 暗色截图校准 Canvas 暗色颜色；不改 Light 调色板、字体、布局、窗口几何、资源文案、goldens 或阈值，等 exact-head CI 验证后提交；未合并 PR。
- Exact-head Windows P2C job [40363399854](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40363399854) PASS，artifact `12670040771`，digest `sha256:74ca2a4a52afcde5fe04334918587a77b1fc6c469ffce03916dbb9920aa616c6`。来源标签 v3.5.2、官方资产 SHA、setup detached PGP signature 与二进制版本核验通过；环境 Windows Server 2022 build 20348、1024×768、96 DPI/100%。设置外框 574×374、客户区 558×335。GDI 132 行：逻辑 face `MS Shell Dlg 2`，8.25 pt/400，ascent/descent 11/2 px；注册表 substitution 指向 Tahoma，尚不足以证明实际字体文件。41/41 PNG hash 对照 `result.txt` 通过。
- Windows 已有主窗与五页 EN/简中 light/dark、四个 English 主菜单、Memory cleaning 滚动两语言截图。托盘菜单未被 UI Automation 暴露；安全清理菜单未能无副作用打开；确认/结果弹窗需真实清理，均 BLOCKED。未执行任何清理。
- Exact-head Linux P2C run `40363399854` 的 Xvfb 作业仍运行于 APT `Install runner-only build and X11 dependencies`，自 16:53:35 UTC 开始；截至上次查询 GitHub 未公开运行中 job 日志。APT 停滞根因未知，Linux截图、字体候选、dark comparison/overlay 尚无结果。P2B exact-head Weston run `40363344884` pending；旧 head run `40355264157` 的 Weston job状态陈旧且仍 reported in progress，不取消/不冒充当前结果。
- 旧字体基线：Windows GDI face alias `MS Shell Dlg 2`、registry substitute Tahoma；Linux 8 pt Sans / DejaVu Sans、简中 Noto Sans CJK SC，Pango ascent/descent 10/3；历史同源 120 行中 25 行宽高差≤2 px、95 行仍有度量差。Liberation Sans 作为独立 CI 进程候选，许可证文件随 artifact 检查；不提交字体文件、不修改生产默认、不通过缩放或缩小字号掩盖误差。Zamak Canvas `Painter.text` 当前给字号/粗体参数，没有 per-call 字体 family 参数；是否需要桥接等候候选实测后决定。
- 本机无图形桌面运行：Zamak 0.2.0；治理 33 项 PASS、Python/C safety 3/3 PASS、Zamak 30/30 PASS、check/build PASS、`scripts/build-arch.sh` PASS、所有 shell `bash -n` PASS、`git diff --check` PASS。未运行本机 Xvfb、Wayland、GUI、桌面输入或 Hyprland IPC。

## 历史续接：P2C-3R1 CI 稳定性与最终证据闭环（2026-10-26）

- PR #7 OPEN / Draft，分支 `feat/issue-3-p2c-settings-ui`，base `feat/issue-3-p2b-main-window`。原始任务 HEAD `52b7977b7582ee197470ba4a0e946a7b1558c2aa`；CI-only commit `406491b70594d202f3d2bb4efa667f2e0886c552` 已推送，PR 远端 head 已核对。生产 UI checkpoint 保持 `60be390eb0ff6c18564f35abcd23d36406c6d3f8`，没有 UI 或比较器变更。
- 修复前 P2C run `40324420754` attempt 1 在 Xvfb bulk APT step 被取消；attempt 2 全部 3 jobs PASS，但包源 `azure.archive.ubuntu.com` 下载 108 MB 用 31 分 18 秒、57.7 kB/s；APT update 本身快速完成，未见 DNS/锁/缺包错误。故障证据指向慢下载，不对更深层网络根因作推断。Pillow 安装 537 kB / 数秒成功。
- 新增 APT 有界诊断 runner，update/install 拆步，内部 8/38 分钟硬时限，workflow step 10/40 分钟，APT 网络超时 30 秒、retry 3、锁等待 180 秒，记录 stdout/stderr/退出码/原因类别/runner 与包版本，失败仍上传 artifact。P2C Xvfb/compare 与 P2B Xvfb/Wayland 都复用；P2B 总时限为 60 分钟。
- P2B 不再对 P2C 功能分支的 push 重复触发；P2C PR 时 P2B 仍运行主窗 X11 与 Weston 生命周期，五页 UI 由 P2C workflow 运行一次。两个 workflow 各自 PR concurrency group 使用 `cancel-in-progress: false`，避免中断正在生成证据的 run。所有 Windows 原生截图、Linux 生产截图和比较作业保留；184/184 几何等阈值/样本未变，1:1 仍 NOT PASS，12 项中文资源差异仍 `DECISION_REQUIRED`。
- P2C run `40336249517`：Windows 官方 3.5.2 provenance/native capture、Linux Xvfb/XTEST 与视觉比较 3/3 PASS；Artifacts Windows `12657162841`、Linux `12656813101`、visual comparison `12656279296`，APT logs `12656977979` / `12656638843`。P2B GUI run `40336249521`：Xvfb main 与 Weston lifecycle 2/2 PASS；artifacts `12655914336` / `12656259412`，APT logs `12655954131` / `12656972640`。该 run 的 P2C 五页重复 XTEST step 被有意跳过，P2C run 同提交已完整执行该测试。PR build `40336249519` 和 push build `40336242138` 均 PASS；PR checks 7 PASS / 0 FAIL，无取消或失败 job。
- P2C APT update/install：Xvfb 16/26 秒、Pillow compare 14/27 秒；P2B X11 6/27 秒、Weston 9/23 秒。Ubuntu 24.04.5 LTS runner image `ubuntu24 20261023.334.3`、APT 2.8.3；安装日志及关键版本和退出码均已在 APT artifacts 核验。
- 最终视觉比较保持 184/184 控件/滚动行 ≤2 px PASS（最大 1.5 px）、导航 10/10、树线 102/102、控件 raster 36/36；10 组页面变化率均优于 P2C-2，本轮范围 5.0741%–14.8853%，全页像素 1:1 仍 NOT PASS。12 项简中资源差异保持 `DECISION_REQUIRED`。source/binary manifest 8 项 SHA 全通过；Windows PNG 41/41、Linux PNG 17/17 SHA 全通过。完整哈希见 `docs/P2C-3_VISUAL_FIDELITY_REPORT.md` 和本轮 artifact manifest。
- build-and-test 系统工具 APT 步骤花 8分43秒但成功；Zamak 0.2.0 check/test/build、治理、Shell、C/Python safety 与 Arch build 全 PASS。本机非 GUI：project verify 33 项、C/Python safety 3/3、shell syntax、Zamak test 29/29、Arch build、diff check 均 PASS。没有启动本机 GUI、Xvfb/Wayland、Hyprland IPC、桌面输入或特权操作。

## 历史续接：P2C-3 原版视觉差异收敛（2026-10-26）

- PR #7 仍 OPEN / Draft；分支 `feat/issue-3-p2c-settings-ui`，Base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。本轮生产源码 SHA `60be390eb0ff6c18564f35abcd23d36406c6d3f8`；随后本记忆/报告更新为文档提交，不改生产源码；未改 main 或其他 PR。
- Exact-source Windows native + Linux Xvfb + comparison run `40319586672` 三个 job 均 PASS；artifacts：Windows `12648157361`、X11 `12648453073`、Overlay/heatmap/metrics/SHA `12648552362`。Windows 的 `IDC_HOTKEY=132` 经 `HKM_GETHOTKEY` 正确采到 `Ctrl+F1`（English/zh-CN），根因是旧采集器把返回值字节顺序反读。
- 对照结果：184/184 控件/滚动行资源矩形 ≤2 px PASS，最大分量 1.5 px；原缺少的 12 个中间滚动状态现有真实双端截图和对应矩形。导航 selection 10/10、树状点线 102/102、raster 定点样本 36/36 PASS。与 P2C-2 相同条件下 10/10 五页双语变化像素率改善，当前范围 5.0420%–14.8853%，仍非 1:1。
- Windows `WM_GETFONT`/GDI 实测 LOGFONT alias `MS Shell Dlg 2`, 8.25pt, weight 400, -11 px, text height 13/ascent 11/descent 2；注册表 substitute `Tahoma`，不能据此证明实际字体文件。Linux 8pt `Sans` / DejaVu Sans，简中 Noto Sans CJK SC fallback，Pango ascent 10/descent 3；英文文本宽度比中位数 1.1469。12 项简中运行时/资源差异矩阵维持 `DECISION_REQUIRED`，不私自选择弱化中文或偏离上游。
- 最终文档 head `7f1f75a031881b576f9e191dc62eb13f52680544` 的 PR Actions 全部 PASS：production Xvfb/XTEST + Weston run `40323243037`；Windows release/settings capture/comparison run `40323243043`；build/governance/safety run `40323243029`。Artifacts Windows `12650430810`、Linux production X11 `12650680581`、comparison `12649498772`。最终比较保持 184/184 geometry ≤2 px、10/10 双语全页变化率改善，变化率与 `60be390` exact-source run `40319586672` 相同。最终 run 的五页 client-area crop 与 Linux 图像 SHA 与此前相同；Windows 整窗 PNG 有独立 SHA，故 hashes 分 run 保管。Zamak check/test/build 29/29、治理 33、Python/C 安全 3/3、Shell 8、Arch build、diff check 本机 PASS。
- 历史诊断：独立 lifecycle run `40319586679` 于 12:23:26 UTC CANCELLED（Weston PASS、Xvfb 依赖安装阶段无结果）。同源码 push run `40319576201` 的 Weston PASS、Xvfb FAIL（exit 1）；artifact 无 client log、截图或 XTEST 记录，发生在生产进程启动前，具体根因无证据；对应 PR run `40319586672` 与最终 head run `40323243037` 的 Xvfb/XTEST 均 PASS。未在本机启动 GUI/Xvfb/Wayland，未访问桌面 IPC 或注入输入。
- Windows 已捕获五页 EN/zh-CN light/dark、主菜单四项及主窗；dark Linux 对照未运行。Tray menu、clean-region menu、cleanup confirmation/result 仍 BLOCKED，不触发真实清理。整体像素 1:1 NOT PASS；没有清理、Polkit、Swap/ZRAM、root GUI 或活动 Hyprland 操作。

## P2C-2 已核验历史证据（2026-10-26）

### 最新 exact-head 验收（production source SHA `20c8fbc8fc80c481965fecbde17aa507eb7247fe`）

- PR #7 仍 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；本轮在同一分支更新导航选中区和对照器，没有修改 main 或其他 PR 分支。
- 当前 Windows 3.5.2 native / Linux Xvfb / comparison run `40294897392` 成功；artifacts：Windows `12636924611`、Linux X11 `12638110426`、overlay/difference/geometry `12637747957`。所有原图和派生图的 SHA-256 manifest 在比较 artifact 中。
- 导航复核结果：资源 `IDC_NAV=104` 的 5 页 × English/简中，共 10 个 selection bound 全部 PASS，`dx/dy/dw/dh` 均为 0；102/102 个树状点线像素为精确 `#6D6D6D`。选中色是原版 `#D9D9D9`，不再绘制整行焦点框。
- 同次比较：172 项资源 ID 控件几何 PASS（≤2 px），0 FAIL；12 项 Linux 中间滚动状态在 Windows 未采集，标记 `NOT_COMPARABLE_NATIVE_STATE`。页面/控件 raster 12/12 PASS。changed pixels 6.8528%–16.8042%，所以 1:1 像素等价仍未通过。
- 同 HEAD 的生产 Xvfb/XTEST + private Weston/Pixman run `40294897349` 成功，artifacts：X11 `12638002197`、Wayland `12637667843`。Xvfb 日志记录 GTK 3.24.41 targeted delete handler、单一已核验 holder、重复显示/关闭、主窗仍可用及退出码 0。Weston 客户端退出码 0，但日志保留 1 条 `gdk_seat_get_keyboard` critical 和 SIGTERM 关机期间的 layer 诊断；未屏蔽，也没有证据确认因果或 compositor crash。这不代表活动 Hyprland 实测或 Wayland 输入验收。
- `build-and-test` PR run `40294897341` 与同 HEAD push run `40294892563` 均已完成并 PASS。
- Windows 环境：Server 2022，1024×768，96 DPI / 100%，light，8 pt `Ms Shell Dlg`；设置外框 574×374，客户区 558×335。Linux：Ubuntu 24.04、GTK 3.24.41、Xvfb 1024×768×24、scale 1 / 96 DPI、Adwaita light、DejaVu Sans / Noto Sans CJK SC。字体基线、抗锯齿和原生控件 raster 仍不同；暗色 Windows 参考缺失。
- 本机当前源码验证：Zamak `0.2.0`；`zam check .` PASS；`zam test .` 29/29 PASS（含双语导航几何测试）；`zam build .` PASS；治理检查 33/33、Python/C 安全测试 3/3、Shell 语法、比较脚本 `py_compile`、`scripts/build-arch.sh`、`git diff --check` 均 PASS。
- 本机没有启动 GUI/Wayland/Xvfb，没有 Hyprland IPC、特权操作、Polkit、清缓存或 Swap/ZRAM 修改。历史 GroupBox、默认只读勾选/颜色预览、无底部固定提示、GTK 针对性 close handler 的已验证状态在下方旧 checkpoint 记录中保留。

### Previous exact-head 验收（production source SHA `06be74b740e8906e64d837d8ac77e0255bea1081`）

- PR #7 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；继续使用 `feat/issue-3-p2c-settings-ui`，未修改 main 或其他 PR 分支。
- 修复 GroupBox 1 px 描边的半像素对齐。精确源截图对照 run `40290615170` 通过；artifacts：Windows native `12636291172`、Linux production X11 `12635727110`、overlay/diff/geometry `12635877774`。Windows 与 Linux 实际截图覆盖五页双语，所有原图 SHA-256、并排图、50% overlay、差异热图、RGB 样本及 CSV 几何表见 `docs/P2C-1_SETTINGS_UI_REPORT.md`。
- 184 个几何记录：172 PASS（偏差≤2 px）、12 `NOT_COMPARABLE_NATIVE_STATE`（Linux 中间滚动状态未在 Windows 捕获）、0 FAIL。Control/page raster 12/12 PASS；Appearance source-color samples 8/8 PASS。五页/两语言 changed pixels 为 7.3958%–17.6612%，故仍不等于 1:1。
- Xvfb/XTEST exact-source PR run `40290615170` PASS，应用正常退出 0；P2B GUI push run `40290609997` 的 Weston Headless/Pixman 也 PASS，artifact `12635732860`。Wayland artifact 保留一条 GDK `gdk_seat_get_keyboard` critical 与 Weston shutdown `BUG: finalizing a layer with views still on it.`，没有证据确认因果；不宣称 Wayland 输入验收。PR 事件 GUI run `40290615331` 的 Xvfb PASS，Weston 依赖安装步骤在报告快照时仍运行。
- GTK 3.24.41 日志记录 `delete-event` signal 66、唯一预存 unblocked handler 70 被单独 block 后连接目标 handler；运行时树确认唯一直接 GtkBox holder 且 border 12→0。XTEST 覆盖 Close/Escape/Alt+F4/WM_DELETE、重复隐藏/重开、主窗口继续可用及正常退出时释放窗口资源。
- 本机：Zamak 0.2.0 check/build PASS、test 28/28 PASS；项目治理 33 项 PASS、C 安全单测 3/3 PASS、Shell 语法、比较脚本 py_compile、Arch build、`git diff --check` PASS。主机没有 Xvfb/Weston，故本机运行时截图 `NOT RUN`；GUI 证据来自隔离 runner。没有启动本地 GUI、访问 Hyprland、清理内存、Polkit 或修改 Swap/ZRAM。
- 环境：Windows Server 2022 1024×768、96 DPI/100%、light、`Ms Shell Dlg` 8 pt；Linux Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、DPI 96/scale 1、Adwaita light、DejaVu Sans/Noto Sans CJK SC。字体/抗锯齿仍可见；暗色 Windows 截图和 Windows 未采集的中间滚动几何仍是缺口。

### Exact-head 验收更新（源码 head `1d1d6562cb27ddb63fb38a2bf71756b5e88d63ee`）

- PR #7 仍 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；PR review 无新行内结论。HEAD 与远端一致，工作树起初干净；未改 main、PR #2/#4/#5/#6，也未启动本机 GUI 或操作桌面。
- 生产 Xvfb/XTEST + Weston Headless/Pixman run `40284909745` 两作业均 PASS。Artifacts：X11 `12633129180`、Wayland `12633864035`。Settings 客户区 558×335；XTEST 覆盖五页双语、Close/Escape/Alt+F4/WM_DELETE_WINDOW、重复 hide/show、相同窗口重开、主窗存活和最终窗口资源释放，应用退出码 0。GUI bridge 日志：GTK `3.24.41`，`delete-event` signal 66，唯一预存 unblocked handler 70 被单独 block 后连接目标 handler；holder 为唯一直接 GtkBox、原 border 12，验证后改为 0。
- Windows native v3.5.2 run `40284909821` 的 Windows job PASS，artifact `12633108874`；源 tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`，binary 版本 3.5.2。PR run `40284909821` 的 Xvfb job 于 25 分钟 job timeout 后 CANCELLED，比较 job SKIPPED；push duplicate `40284904057` 同样在 Xvfb timeout 后 CANCELLED / 比较 SKIPPED。另一个独立生产 Xvfb+Weston run `40284909745` 两项通过。
- 同一 source HEAD 的 Windows artifact `12633108874` 与 Linux artifact `12633129180` 已用隔离 Bundled Python/Pillow `12.3.0` 作本地 exact-head comparison：截图 10 张；控件几何 172 PASS / 0 FAIL；12 个中间滚动项 `NOT_COMPARABLE_NATIVE_STATE`；页面/checkbox raster 10 PASS；上游颜色采样 8 PASS。Overlay/heatmap/CSV 已在本地比较输出生成。Changed-pixel 比例 7.9548%–18.6990%，仍非像素等价。
- 截图与源码 SHA-256、字体/主题/DPI、运行与几何细节已记入 `docs/P2C-1_SETTINGS_UI_REPORT.md`；矩阵补了只读默认态、无外框与证据状态说明。Windows `8pt Ms Shell Dlg` 与 Linux DejaVu Sans/Noto Sans CJK SC 抗锯齿/字体差异仍存在；暗色主题、托盘/菜单/对话框和 Wayland 输入未验收。Weston 留有一次 `gdk_seat_get_keyboard` critical、无 `wl_seat` 和 shutdown `BUG: finalizing a layer with views still on it.`；未观察到 compositor 崩溃，因果未定。
- 本机非 GUI：Zamak 0.2.0 `check`, `build`, `scripts/build-arch.sh` PASS，`test` 28/28；project verify 33 项、C safety 3/3、相关 Shell 语法和 `git diff --check` PASS。Pillow 比较使用 bundled runtime，未安装本机依赖。
- 已调整 P2C Windows baseline workflow：仅由目标 PR 触发，避免同一提交同时启动 push/PR 两套重复截图任务；Xvfb timeout 45 分钟、比较 timeout 20 分钟。此工作流修正尚待当前提交的 Actions 验证。没有在本机启动 GUI、Xvfb、Wayland 或操作 Hyprland。

### P2C-2 GroupBox 校准记录

- 参考与首次未校准样本为 Windows `#DCDCDC`、Linux `#BBBBBB`。仅改颜色后的 CI 样本为 `#E5E5E5`，原因是 1 px Cairo stroke 在整数路径中心覆盖半像素。路径移至半像素并缩小矩形后，exact-head run `40290615170` 在双语均得到 `#DCDCDC`，相关几何与栅格检查通过。暗色值仍是临时值，没有真实 Windows 暗色参考。

- 继续使用 PR #7 分支 `feat/issue-3-p2c-settings-ui`。开工时本地与远端 HEAD 均为 `90f5b31a9dede8896a44bd356c43f38862a005db`；PR #7 OPEN / Draft，base `feat/issue-3-p2b-main-window`，没有修改 main 或其他 PR 分支。
- 已下载并核对该 HEAD 的 CI 证据：Windows 原生/几何 artifact `12632986603`、生产 Xvfb artifact `12632328288`、overlay/diff/几何 artifact `12632129496`；run `40282499292` Windows+Xvfb+comparison、`40282499286` Xvfb+Weston、`40282499313` build/test 均 PASS。该批证据只对应修正前的 `90f5b31`。
- 修正前可比较控件矩形 172 PASS，12 个 Linux 中间滚动项为 `NOT_COMPARABLE_NATIVE_STATE`，0 FAIL；截图像素变化 61.7290%–80.1728%，故不通过 1:1 像素验收。
- 当前未提交修改：浅色设置页背景改为真实 Windows 截图测得 `#F0F0F0`，导航区域保持白色；复选框方框调整到 13 px；内存区域列表 checkbox / label 内缩改成 4 px / 22 px，并由 `ui_geometry.zm` 集中管理；比较脚本增加页面底色样本和 checkbox 像素边界检测。
- 本机非 GUI 验证：Zamak 0.2.0 `check` PASS，`test` 28/28 PASS，`build` PASS；项目治理 33 项 PASS；C 安全测试 3/3 PASS；比较脚本 `py_compile` PASS；`scripts/build-arch.sh` PASS；`git diff --check` PASS。没有本地启动 Xvfb、Weston、生产 GUI 或活动 Hyprland。
- Exact-head `f5c5488b4f4e0871311c52dfe4bde61d89be7b7a` Actions：GUI/XTEST + Weston `40284199595` PASS；build/test `40284199576` PASS；Windows/Xvfb/comparison `40284199572` 的采集/Xvfb PASS，但 comparator FAIL 两个 Memory checkbox raster 行。比较结果是 172 geometry PASS / 0 FAIL（另有 12 个中间滚动项 NOT_COMPARABLE_NATIVE_STATE），六个页面底色样本和两个 General checkbox 样本 PASS；截图 changed pixels 降为 7.9548%–18.6990%，仍非像素等价。
- 检查比较产物 `12634010092` 后确认 Memory checkbox ROI 下边缘包含下一行的一个深色像素，导致两种语言都误报高度偏差；已把裁剪范围缩到当前列表行以内，待新 commit/Actions 验证。Exact head Windows/Xvfb artifacts 为 `12633352398` / `12633925245`。Windows 原生 2 px 几何、深色主题、菜单/托盘截图和像素级 1:1 仍未整体验收。所有设置快照仅为上游只读默认值；未执行设置保存、清理、Polkit、Swap/ZRAM 操作。

## P2C-1 最新续接（2026-10-26）

- PR #7 仍 OPEN / Draft；base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。生产源码测试 checkpoint `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`；文档/记忆同步在 CI-verified head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df` 上完成。本段再更新的只是报告哈希和 Actions 状态，未改生产源码。分支 `feat/issue-3-p2c-settings-ui`。未合并或改写任何其他分支。
- 已修正 libui-ng settings content holder 的 12 px border。最新 X11 artifact `12627741039` 记录 `CI_SETTINGS_CONTENT_BORDER old=12 new=0`，Canvas 与 Windows 原生 client area 均为 `558×335`（固定 Windows Server 2022 / 96 DPI / 100% 条件）。此前 Linux `523×321` / `547×345` 数值不再作为当前尺寸。
- 首次从隔离 Windows Server 2022 interactive runner 获得实际原版 `v.3.5.2` Native 截图。固定 upstream SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`；release archive、setup SHA、setup detached PGP signer、portable executable 3.5.2 及 SHA 均通过核验。环境/几何/PNG SHA-256 与 5 页双语索引在 `docs/P2C-1_SETTINGS_UI_REPORT.md`；Windows artifact `12626872531`，Linux settings X11 artifact `12627741039`。
- Windows 捕获 1024×768、96 DPI、100% scale、light theme、8pt Ms Shell Dlg，时间 `2026-10-26T04:16:04Z`。Settings 外框 574×374、客户区 558×335；IDC_NAV `(12,10) 132×312`，页面 `(156,10) 390×273`，Reset `(153,299) 75×23`，Close `(471,299) 75×23`。Windows 主窗 352×407。采集只在临时 runner 执行，未触发内存清理。
- 当前 code head 本机验证：Zamak 0.2.0 check/build PASS、test 22/22；`verify_project.py` 33 项、C safety 3/3、Arch build、两份变更 shell `bash -n`、`git diff --check` 均 PASS。未启动本机 GUI、Xvfb、Weston 或活动 Hyprland；未执行 Polkit/内核写入、Swap/ZRAM 修改。
- 完整 PR Actions 在文档 head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df` 全 PASS：GUI `40269534094`（Xvfb 与 Weston 两项），build `40269534153`，Windows native baseline `40269534205`。对应 artifacts：Linux X11 `12627279867`、Wayland `12627990285`、Windows native/screenshots `12627309561`。Windows runner 最近采集时间 `2026-10-26T04:31:57Z`；截图 SHA 与环境清单在设置报告中。Push GUI `40269530616` 的 Weston 通过，但 Xvfb job在最后一次检查时仍运行；PR checks 均已完成成功。
- 实测客户区几何相同不等于视觉 1:1。尚未对齐窗口客户区后生成 overlay/diff 或验证 2 px；深色主题、主菜单/托盘/清理对话框截图未取得；设置项持久化、托盘服务、自动/手动真实清理仍未实现/启用。Weston artifact 保留一次 `gdk_seat_get_keyboard` GTK critical、无 `wl_seat` 和 shutdown layer diagnostic，因果未定。

## 最近记录：P2C-1 五页设置 UI 与原版截图 runner（2026-10-26）

- P2C-1 在独立分支 `feat/issue-3-p2c-settings-ui`，Draft PR #7 base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。实现五个设置页与 English/简体中文切换；PR #7 仍 OPEN / Draft，不合并。
- 在 head `988513ad7c9afe5fd79cb0fe6239f7759cbdaf86` 的 Xvfb run `40260614636` 真实 WM_DELETE_WINDOW 测试失败：关闭设置窗导致应用循环退出，缺少设置 close callback marker。已在 `native/gui_window.c` 添加只作用于应用设置窗的 GTK `delete-event` 重新路由，提交 `396c3126fbb8b9af33755ebf8f2eb67942c71f83` 并推送。新 artifact 观察 `signal_id=66 disconnected=1 connected=1`、delete callback、Zamak 将设置窗隐藏并保持主窗、随后 app 正常退出 0。
- 修复 head 的 PR GUI Actions [40261537129](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261537129) 的 Xvfb/XTEST 和 Weston Headless/Pixman 两作业均 PASS；build-and-test [40261541370](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261541370) PASS；push GUI run `40261541380` PASS。Artifacts：X11 `12624986143`，Wayland `12623629586`。中英文五页截图与源 SHA 记录在 `docs/P2C-1_SETTINGS_UI_REPORT.md`。
- X11 环境：Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、scale 1/DPI 96、Adwaita light；主窗 337×369，设置窗 547×345，均是 Linux 测试尺寸而非 Windows 像素基准。语言/页面、Reset 确认、Escape、Close 按钮和窗口管理器关闭均通过；标题栏 close 不再退出主应用。未在本机安装 Xvfb/启动 GUI。
- Wayland 私有 Weston/Pixman 下主窗和设置窗 surface 创建/绘制/退出码 0；不测输入；记录一次 `gdk_seat_get_keyboard` critical、无 `wl_seat`，未归因。没有访问活动 Hyprland 或桌面 IPC。
- 本机：`verify_project.py` 33 项 PASS；Python/C 单测 3/3；shell 语法 8/8；Zamak 0.2.0 check/build PASS，test 21/21；`scripts/build-arch.sh`、`git diff --check` PASS。没有 Polkit、drop_caches、Swap/ZRAM 或 root GUI 操作。
- Windows runner 第一次尝试 run `40261537106` / `40261541252` 在 20 分钟限制取消：资产 SHA PASS 后卡在 Chocolatey GnuPG 安装，artifact `12625127901` 只含部分 `result.txt`。移除安装、使用 Git for Windows 自带 GPG 后，run `40263501261` / `40263504561` 又因 MSYS GPG 将 `D:\...` `GNUPGHOME` 误当 MSYS 相对路径而快速 FAIL；tag 和 archive/setup SHA 均 PASS，签名验证、binary version、桌面检查、截图尚未到达。当前工作副本以 `cygpath.exe` 将 GNUPGHOME/key/setup/signature 路径转成 POSIX 格式，待新 Windows Actions 验证。真实 Windows 截图、DLU 实测、overlay/2px 和 Hyprland 输入仍 BLOCKED / NOT RUN。设置持久化/托盘/特权功能未实现或禁用。

## 历史快照（2026-10-27；P3A-PRE-R1，基线 629fc3f）
> 以下为 P3A-pre 当时状态与证据；OPEN/Draft、审批及“未实施”描述仅对应该日期，当前状态以上方 2026-10-28 快照为准。

- 远端 fetch 后本地/远端 P2C 分支一致 `629fc3f88ee91d0cc3582d6be1789991fc3f72b6`，工作树开始时干净；未切换分支、未合并、未改 base/main。
- 核对 ChatGPT review `5663699910` 和三个新 ADR 线程：`4386187875`（rename 后 fsync 状态）、`4386187883`（损坏配置保存）、`4386187887`（路径/锁/权限）。ADR 保持 PROPOSED，修订详见 `docs/ADR/0004-user-preferences-persistence.md`。
- 当前 exact baseline PR #7 checks 7/7：P2C `40465893251`、P2B `40465893241`、build PR `40465893268`；P2B push build `40465886984` 也通过。证据检查 artifact `12717146938` digest `sha256:d9c99dcaa330aa76c25be8ef60332ad61cb082e5a951fd3ca153369ff9f8bfd5`，client.log SHA-256 `39ef3d24f255df6cd3ccbdd90fd7cf280105933ea130a8351aeccc84c06d08c2`。该 Xvfb 日志含失败注入、立即 redraw 请求和 em-dash 状态标记。
- PR #4 workflow 触发器已从远端文件核实：push `test/issue-3-isolated-gui`，pull_request base filter `fix/issue-1-zamak-build`，权限 `contents: read`。若改 base 到 main，须先更新 PR 过滤条件。独立的 12 项 probe tests 不得被生产 GUI 测试替代。
- 本地验证 PASS：`python3 scripts/verify_project.py` 33 项；`python3 -m unittest discover -s tests -v` 3/3；`bash -n scripts/*.sh scripts/ci/*.sh`；Zamak 0.2.0 `check`、`test` 30/30、`build`；`git diff --check`。本轮仅文档/记忆改动，未运行图形或桌面操作。
