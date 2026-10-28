# 会话日志（追加式）

## 2026-10-25 — Bootstrap governance
- 输入：Zamak v0.2.0 本地源码；GitHub 公共仓库尚未成功创建。
- 输出：架构、记忆、开发计划、Codex 协作协议与公共仓库发布脚本。
- 测试：C 安全单测通过；Zamak 编译/GUI 尚未验证。
- 待办：用户本机创建公开 GitHub 仓库，Codex 完成 P1 编译验收。

## 2026-10-25 — P0 GitHub bootstrap
- 账号核验：`gh api user --jq .login` 为 `MilchstraBe-Endless10`；通过 `scripts/publish-github.sh` 的精确 PUBLIC 确认提示。
- 创建并推送 PUBLIC 仓库：<https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak>；默认分支 `main`。
- 初次提交 `02ca561b41d55611a131338bdca50243e78c4aa7` 与当时远端 `refs/heads/main` SHA 一致；作者邮箱使用 GitHub noreply。
- 本机项目完整性检查通过；3 项 C 安全测试通过；4 个 shell 脚本语法检查通过。发布主机未安装 `zam`。
- GitHub Actions run [40169273278](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40169273278)：Zamak `0.2.0` 安装和版本检查成功，`zam check .` 因 `project.name must be an identifier` 失败，后续 Zamak 测试和 native build 未运行。
- 创建 P1 编译验收 Issue #1：<https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/1>。

## 2026-10-25 — P1 Zamak 0.2.0 build qualification
- 从最新 `origin/main` (`080417e153d91bf2adac756e5154311fef49333f`) 检查后创建/续接 `fix/issue-1-zamak-build`；远端同名分支原先不存在，本地既有改动保留。
- 根因与修复：`zam.toml` 的 `memreduct-zamak` 不是 Zamak 标识符，改为 `memreduct_zamak`；按 Zamak 0.2.0 API 使用 `words()` 并传播解析错误；安装器按真实 GUI 构建名匹配并排除测试目标；项目验证脚本同步标识符。产品名、仓库名和安装路径继续用 `memreduct-zamak`。
- 本机验证（Omarchy 4.0.6 / Hyprland 0.56.2 / Wayland / GTK 3.24.52）：项目验证 29 项、Python/C 安全测试 3 项、四个 shell 脚本语法检查均通过；Zamak 0.2.0 `check` 通过、5 项 `test` 通过、`build` 输出 GUI；`scripts/build-arch.sh` 和 Helper `--dry-run` 均通过。
- GUI 以普通用户在 Wayland 打开，确认中文、RAM/缓存/Swap 信息、周期刷新、清理与自动清理控件显示、正常关闭且进程退出码 0；未触发清理或自动模式。未执行 `/usr` 安装卸载、Polkit E2E、真实缓存写入或 Swap/ZRAM 操作。
- 详细命令与输出摘要见 `docs/TEST_PLAN.md` 和 `memory/SHORT_TERM.md`。Draft PR / GitHub Actions 状态待本轮推送后补记。
- 提交 `1d4654867f59154981010007288cff1fda42f457` 使用 GitHub noreply 作者邮箱；推送分支后，`git ls-remote` SHA 与本地 HEAD 相同。
- 创建 Draft PR #2 [https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/2](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/2)，PR 正文引用 Issue #1。`build-and-test` 两个事件均通过：push run `40170617141`、pull_request run `40170634633`，均针对该代码 SHA。
- 文档验收记录提交 `9b5177e992eb1c86b72643a0ae2c0cda9a570555` 的 Actions 也通过：push run `40170749833`、pull_request run `40170752581`；当时 PR head 与远端 SHA 一致。
- PR 保持 Draft，未合并；等待 ChatGPT 复审和用户批准。未进入 P2。

> 本文件按日期追加。旧条目中的 OPEN/Draft 与“未开始”描述只表示其记录时状态；当前 GitHub 状态见 memory/SHORT_TERM.md。

## 2026-10-25 — P2A-R6 文档与隔离 GUI CI 补强（进行中）
- 规格分支基于 P1 SHA `11b0b9444cd94f0026b855abe3849a06106426f2`，PR #4 测试分支 HEAD `a522e5629fc89969533527b4d806e71e58fb9be7`；PR #2/#4 均 OPEN / Draft，未合并。
- 迁入并审阅上游 v.3.5.2 UI 基线、交互矩阵、差异报告、可行性、截图采集清单和 Linux 能力映射；强化官方紧凑尺寸约束：主窗口 224×214 DLU、五个设置页模板各 260×168 DLU，R6 336×348 CI 探针不作为生产尺寸。
- 本机项目验证 29 项、3 项 C 测试、4 个 shell 语法、Zamak 0.2.0 check、5 项 Zamak tests/build、Arch helper build 以及 diff whitespace 检查通过；未启动本机 GUI。
- R6 Actions：build push/PR runs `40189616005`/`40189619010` PASS；Wayland Headless jobs 在 `40189616016`/`40189619021` PASS；Xvfb 两次运行在独立 `windowclose` 测试失败。日志证据：未触发 `CI_WINDOW_CLOSE`，Xvfb 未运行 WM，GDK 报 unexpected destroy 与 GTK critical。hover→pressed ROI 与 pressed→release ROI 各有 8004 changed pixels。修复计划是在 Xvfb 中启动/跟踪/清理本次子进程 Openbox，再真实复测，不屏蔽 GTK 错误。
- 未做真实清理、Polkit、Swap/ZRAM、Hyprland IPC 或主机 GUI 测试；真实 Windows 源图、DLU 实际像素换算和 2 px overlay 仍 BLOCKED / NOT RUN。P2B 未开始。

## 2026-10-25 — P2A-R6 isolated GUI CI follow-up
- PR #4 test branch final recorded validation source SHA `1cb7b77246275962f092fa0c0ccc27fbf982ce8b`; base remains P1 `11b0b9444cd94f0026b855abe3849a06106426f2`. PR #2/#4 remain OPEN / Draft; no merge.
- Fixed ROI test now compares same-pointer hover-only vs held-press and pressed vs release (8004 changed pixels each). Preserved all nine original pure Zamak tests; added three compact geometry/locale tests for 12 total.
- Corrected close-test semantics after reading upstream xdotool implementation: `windowclose` calls `XDestroyWindow`, so it does not test WM_DELETE/GTK close and caused expected GDK teardown warnings. Under isolated Xvfb + Openbox, XTEST Alt+F4 exercised the WM close path and emitted `CI_WINDOW_CLOSE`, `CI_APP_RETURNED`; process exited 0 and the window disappeared without close teardown criticals. A separate `windowquit` attempt did not invoke the callback and is not claimed as passing.
- GUI Actions push/PR `40191087455` / `40191091618` passed both Xvfb and Weston jobs; build push/PR `40191087528` / `40191091733` passed. Artifacts were uploaded. Source file SHA `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`.
- Weston 13 Headless/Pixman logged one `gdk_seat_get_keyboard` critical per run; `wl_seat` was not advertised. Client still connected, drew the Canvas, returned from `app.run` and exited 0. Cause is consistent with headless missing input seat but not proven; GTK diagnostics remain visible.
- Official UI size contract is now explicit: `IDD_MAIN` 224×214 DLU; five settings templates 260×168 DLU. Production must follow the official compact resources, no enlargement. The 336×348 / 312×324 values are CI probe-only and do not establish a Windows pixel target.
- The independent P2A spec branch has local first commit `367efce8dcdecb487595507ff7e9f275bedd96bc`; Windows-native screenshots, measured DLU conversion, paired overlays/2px acceptance remain BLOCKED / NOT RUN. No production UI, host GUI, compositor IPC, memory cleanup, Polkit, Swap, or ZRAM operation.

## 2026-10-25 — R6 close-test provenance correction
- Final full-diff review found `tests/gui/canvas_probe/README.md` and `SOURCE_PROVENANCE.md` still describing `xdotool windowclose` as the passing close path. Corrected them to the actual XTEST Alt+F4 WM_DELETE path under the Xvfb-owned Openbox process; documented that `windowclose` calls `XDestroyWindow`. Corrected two stale shell failure messages to refer to the WM close request.
- `bash -n scripts/ci/p2a-xvfb-gui.sh` and `git diff --check` passed. No change to Canvas probe source or event/test logic; source SHA remains `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`. Push and PR Actions are rerun for the documentation/diagnostic update.
- Final head `6ad0f401d2b831ee71a1387a858e7456bbe444e5` passed the rerun: GUI push/PR `40192348460` / `40192351634` and build push/PR `40192348489` / `40192351591`; all jobs successful. Fresh GUI artifacts were uploaded. The PR remains Draft.

## 2026-10-25 — P2A-R6 隔离 GUI CI 复测完成
- 在 `test/issue-3-isolated-gui` 修正窗口关闭说明后提交 `e2d3c5e4481e9494472913f41636580369038cd8` 并推送；`git ls-remote` 与本地 HEAD 相同。PR #4 仍 OPEN / Draft，base 为 P1 分支 `fix/issue-1-zamak-build`。
- GitHub Actions 全部针对该 SHA 成功：GUI push `40191454446`、GUI pull_request `40191456330`、build push `40191454361`、build pull_request `40191456339`。Artifacts 已下载核验，包含测试截图、ROI 差异、X11 close/WM 状态、Weston 与客户端日志、版本及 SHA。
- 隔离 Xvfb/Openbox 的 XTEST Alt+F4 触发 GTK close handler；日志包含 `CI_WINDOW_CLOSE` 和 `CI_APP_RETURNED`，进程 exit 0，窗口随后消失。`xdotool windowclose` 会调用 `XDestroyWindow`，故不用于正常 WM close 测试。Hover-only→pressed ROI 与 pressed→release 像素差各为 8004。
- 保留既有 9 个 Zamak 测试函数，新增 3 项紧凑几何/双语检查；12/12 PASS。Weston Headless/Pixman 的 socket、Canvas draw、受控退出与 exit 0 PASS；每次记录一次未屏蔽的 `gdk_seat_get_keyboard` critical，且 Weston 未公布 `wl_seat`，因果未证实。
- 本机 `verify_project.py` 29 项、C 安全测试 3 项、shell syntax、Zamak 0.2.0 check/test/build、Arch build helper、diff check 全部 PASS。未启动主机 GUI/Hyprland、未触碰内存清理、Polkit、Swap/ZRAM。
- 正式源码规格独立分支仍以 P1 SHA `11b0b9444cd94f0026b855abe3849a06106426f2` 为 base；规格继续固定 Windows 3.5.2 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 与官方紧凑 DLU 尺寸。真实 Windows 截图、实测 DLU 像素换算、像素 overlay/2 px 验收仍 BLOCKED / NOT RUN；P2B 未开始。

## 2026-10-25 — P2A-R6 源码规格 Draft PR
- 将正式 P2A 源码规格从独立分支 `docs/issue-3-p2a-ui-spec` 推送；基于 P1 `fix/issue-1-zamak-build` / `11b0b9444cd94f0026b855abe3849a06106426f2`，未从 PR #4 派生。创建关联 Issue #3 的 [Draft PR #5](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/5)，PR #4 测试代码没有混入。
- 文档列明官方 v.3.5.2 SHA、资源和双语文本、主窗口及五页资源尺寸、四个菜单、八项清理映射、托盘/事件、交互矩阵、Zamak 技术可行性和 Windows 原生截图采集检查清单。项目长期规则明确 UI 高保真不可重新设计/放大，并把 Windows API 等价性独立映射。
- PR #5 创建时 head `b94731b9a9bc52a4834358f9f71fcf86a2033925`；创建后继续提交了记忆与会话日志同步，因此最终 head 另有推进，以当前远端 `docs/issue-3-p2a-ui-spec` SHA 为准。Push 与 pull_request build-and-test 已触发。
- 源码规格 head `90d62100e840aa408e4ea99cbbd866ef2025e99e` 的 PR `40191901022` 与 push `40191896559` build-and-test 均 PASS；验证治理文件、shell syntax、C safety tests、Zamak 0.2.0 check/test/build。随后本次记忆同步提交只变更 Markdown；提交后需要再核对 PR head 与其新 Actions。
- 真实 Windows 3.5.2 截图、实际 DLU 像素换算、双边截图 overlay/diff 和 2 px 验收仍 BLOCKED / NOT RUN。该 PR 是源码级规格审阅交付，不是 P2A 像素验收或 P2B 实现批准。
## 2026-10-25 — P2A-R7 开工状态（历史快照）
> 以下是当日记录的审查与分支状态；其中 OPEN/Draft、截图缺口和“尚未实现”均不代表当前状态。后来取得的 P2B/P2C 证据见同期报告和当前状态快照。

- 当日记录的 PR #5 规格来源 SHA 为 `6b3862b0e6d105f42dbf8d2d6c679cdfb182ad16`，PR #4 最终测试来源 SHA 为 `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1`；对应 PR #4 GUI/build Actions 为 `40192531809` / `40192531948`，push runs 为 `40192528601` / `40192528611`。
- 当日明确 `IDD_MAIN=224×214 DLU` 是主窗口模板，五个 `IDD_SETTINGS_* = 260×168 DLU` 各为设置子页面模板，不是完整外框。可开始未校准的生产主窗口候选，但 Windows 原生截图、字体/DPI 像素校准、overlay 与 2 px 验收当时仍 `BLOCKED / NOT RUN`。
- 该历史快照后来不再代表当前 PR 状态；上述 SHA 与旧 CI 结果仅用于追溯，不替代当前 main、PR #5/#6/#7 的状态或后续验证。

## 2026-10-27 — P2A-INTEGRATION-PRE：PR #5 本地集成模拟

- 核验源：main=459415001a7e3538e9496eeb90b8ee75a7378f14，PR #5 head=d89d74fde433da1863b4de0223f974c824fb590e。PR #2/#4 已合并；#5/#6/#7 仍 OPEN / Draft，base/head 详见同期 memory/SHORT_TERM.md。
- git merge-tree --write-tree origin/main origin/docs/issue-3-p2a-ui-spec 生成虚拟 tree a2ef41463d55915b93f90ee1abe5adfbf1c2efab；唯一内容冲突为 memory/SHORT_TERM.md 与 memory/SESSION_LOG.md。
- 在隔离 worktree 中模拟合并并手动保留 PR #4 独立探针/CI 记录和 PR #5 P2A 规格记录；没有创建合并提交、更新远端分支、改 Base 或修改 main。
- 修订范围包括历史截图状态快照、当前 P2C-4R1 验收边界、12 项中文文案差异的待决状态，以及 P2B/P2C Draft 证据未集成至 main 的标注。
- 合并候选验证：项目治理 29 项 PASS；Python/C 安全测试 3 项 PASS；8 个 Shell 脚本 `bash -n` PASS；root Zamak 0.2.0 check/test（5/5）/build PASS；独立探针 check/test（12/12）/build PASS；`git diff --check HEAD` PASS。敏感路径/凭据/私钥/云密钥/Windows 二进制与字体扫描 PASS。
- 探针源码 SHA-256 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3` 与原分支清单一致。未运行 GUI/特权清理或活动 Hyprland 测试；未创建提交或修改远端状态。

## 2026-10-28 — 合并冲突中保留 PR #6 历史记录

> 以下原始记录来自 PR #6 分支历史，原记录日期为 2026-10-25。其中当时的 PR 开放状态、Actions 编号和截图缺口仅供历史追溯；当前状态以 `memory/SHORT_TERM.md` 顶部快照为准。

## 2026-10-25 — P2A-R7 规格门槛核对与 P2B-1 生产主窗口

- 复核 PR #5 已修正 head `d89d74fde433da1863b4de0223f974c824fb590e`、base P1 `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #5 仍 OPEN / Draft，CI 2/2 PASS。`IDD_MAIN=224×214 DLU` 是主窗模板，每个 `IDD_SETTINGS_* =260×168 DLU` 是单页模板；真实 Windows 像素验收仍阻塞。
- P2B 分支从 `docs/issue-3-p2a-ui-spec` 创建，未从 PR #4 探针分支派生；Draft PR #6 base 为修正规格分支。生产代码经提交 `a6df90389a69fc9cad12272c2daa2f273c834385` 测试通过；期间修复 XTEST 语言子菜单、分割按钮和清理弹窗坐标，依据实际 GTK Canvas 12 px inset 的 CI 事件日志校准。
- 正式生产窗口 Xvfb/XTEST PR run `40199653180` 与 push run `40199649320` PASS；生产生命周期 Weston/Pixman 使用同一 PR run PASS；build-and-test PR run `40199653440` 与 push run `40199649284` PASS。X11 截图/日志 artifact `12601156413`，Wayland/client artifact `12601781592`。
- Xvfb 实测 top-level client 337×369、Canvas 313×345、scale 1；英文/简中主窗、View/Language 菜单及清理下拉有截图和 SHA-256。菜单、split-button 命中、双语切换、键盘与 Alt+F4 close 的测试来自生产程序，不是 Canvas 探针。
- Weston Headless 13.0.0 使用私有 Wayland socket、Pixman、XDG_RUNTIME_DIR mode 0700；应用读真实 meminfo 并正常返回，退出码 0。日志含一条 GTK seat critical、未公告 `wl_seat`，以及 Weston SIGTERM 清理时 `BUG: finalizing a layer with views still on it.`。异常被如实保留；没有证据将其归为生产应用故障，也没有操作活动 Hyprland。
- 本机 `python3 scripts/verify_project.py`（29 项）、C 安全测试（3 项）、shell syntax、Zamak 0.2.0 check/test/build（7/7）、Arch build（含 helper）、`git diff --check` 均 PASS。没有启动本机 GUI、Hyprland IPC、内存清理、Polkit、Swap 或 ZRAM 写操作。
- 生产控件状态与截图证据见 `docs/P2B-1_PRODUCTION_UI_REPORT.md`；测试矩阵和交互表已更新。真实 Windows v3.5.2 截图、真实字体/DPI DLU 校准、overlay/2 px、Hyprland 输入、HiDPI、设置页和托盘仍 BLOCKED / NOT RUN。PR #2/#4/#5/#6 均未合并。
# P2A-R7 规格修正与 P2B-1 生产主窗口候选（2026-10-25）

## 开工状态
- PR #2、#4、#5 均 OPEN / Draft；PR #5 head `6b3862b0e6d105f42dbf8d2d6c679cdfb182ad16`，PR #4 head `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1`；未合并任何 PR。
- 以 PR #5 最新复审为依据：刷新 R6 最终 GUI/build PR Actions 为 `40192531809` / `40192531948`，对应 push runs `40192528601` / `40192528611`；明确 `IDD_MAIN=224×214 DLU` 是主窗模板，每个 `IDD_SETTINGS_* =260×168 DLU` 是子页模板而非完整外框。
- 允许在截图仍缺失时实现未校准的紧凑生产候选，但需把几何换算集中参数化；真实 Windows 源图、字体/DPI 像素校准、overlay 与 2 px 验收仍 BLOCKED / NOT RUN。

## 本轮结果
- （本轮结束后补记：PR #5 修正提交、P2B-1 生产代码、验证、CI、截图 artifacts 与实际 SHA。）
- 用户主桌面和 Hyprland 未操作；未执行内存清理、Polkit、Swap/ZRAM 写入。PR #2/#4/#5 不合并。

## 2026-10-28 — P2B-INTEGRATION-PRE：PR #6 本地集成模拟

- 核对远端：main `b78785af233dbae7a80085fd7a9f45f30ae42711`；PR #6 head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，旧 base 分支 `docs/issue-3-p2a-ui-spec` 当前为 `a19474ca3f79de65a18e76f6852ab503ce11b173`；PR #7 head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`。PR #6 为 OPEN/Draft 且 GitHub 报 DIRTY；PR #7 为 OPEN/Draft 且报 CLEAN。未改远端状态。
- 在独立 worktree 从 PR #6 head 执行 `git merge-tree --write-tree origin/main origin/feat/issue-3-p2b-main-window`；四个文档冲突：`docs/UI_FIDELITY_TEST_PLAN.md` 与三层记忆中的 MEDIUM_TERM、SHORT_TERM、SESSION_LOG。解决草稿分别保留 P2A/PR #4/PR #5 与 P2B 历史，更新当前 PR #5 已合并状态；未提交，未改 Base。
- 保留 P2B 生产源、GUI 测试和报告；候选树包含 PR #4 独立探针、12 项测试、Xvfb/Weston 脚本、来源清单及 APT 诊断脚本，probe 源 SHA-256 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3` 与清单一致。
- `.github/workflows/p2b-production-gui.yml` 本地 CI 草稿加入 main 与 Base 编辑/同步事件、分离 APT update/install 诊断步骤、有限步骤及 60 分钟 job 超时、Node 24 固定 Action SHA、`if-no-files-found: error` 与不取消运行中证据作业的并发策略。PyYAML 语法解析 PASS；actionlint 未安装。
- 本地结果：治理 29 项、C/Python 安全 3 项、Shell 语法、主项目 Zamak 0.2.0 test 13/13 与 check/build、Arch 原生构建、生产 GUI build、独立 Canvas probe test 12/12、`git diff --cached --check` / `git diff --check` PASS。没有 Xvfb/Openbox/Weston/wayland-info，因此隔离生产 GUI XTEST 与 Weston 生命周期本地运行 NOT RUN；无 root 安装、Hyprland 输入或内存/Swap/ZRAM 写入。
- 实际复核 PR #6 head `911a3d0…` 的旧-base GUI run `40208944364` 与 build run `40208944120`，两者均成功；X11 artifact `12604299350`、Weston artifact `12604988043`。截图确认三组竖排、双语及语言菜单边界；采样失败截图显示三组数值即时失效。此类旧-base CI 不代表 main-base CI。两条结构/语言菜单旧 review thread 仍未解决，采样失败重绘 thread 已解决；本轮没有关闭任何 GitHub thread。
- PR #7 仍堆叠于 PR #6。相对原 base `911a3d0…`，PR #7 为 30 个路径；P2B 与当前 PR #7 同时修改 `p2b-production-gui.yml`、MEDIUM_TERM、SHORT_TERM，后续合并要手工合并这些差异。PR #7 的 `apt-diagnostics.sh` 与 main 为相同 blob。其他 #4 文件在 PR #7 树中缺失属于旧分支基线差异，不是其相对 PR #6 的删除；PR #6 集成后须按新祖先关系复核。
- 未创建 commit、未 push、未改 PR Base/Ready 状态或 main/#7；无 GitHub main-base 新 CI。
