# 会话日志（追加式）

## 2026-10-26 — P2C-3R1 GitHub Actions APT 稳定性补强与证据闭环
- 工作基线：PR #7 的本地/远端 HEAD `52b7977b7582ee197470ba4a0e946a7b1558c2aa`，工作树干净，PR OPEN/Draft、base `feat/issue-3-p2b-main-window`。先读了 PR 最新 ChatGPT R3 review、AGENTS、P2C-3 视觉报告、语言差异矩阵和三层记忆。
- 旧 P2C run `40324420754` attempt 1 的 Linux Xvfb bulk APT 安装步被取消，非应用失败；attempt 2 的三 job 均成功。日志直接显示 Ubuntu 24.04.5 runner 从 `azure.archive.ubuntu.com` 拉取 108 MB 花费 31 分 18 秒、平均 57.7 kB/s，APT update 约 1 秒；未找到 DNS、apt lock、缺包证据。compare `python3-pil` 537 kB 安装数秒通过。慢下载是观察到的阻塞，不推断更深层根因。
- 新增 `scripts/ci/apt-diagnostics.sh`，workflow update/install 分步、step/command 超时、HTTP(S) 30 秒 timeout、retry 3、dpkg lock wait 180 秒、noninteractive、stdout/stderr 和实际 exit code、分类、镜像/OS/APT sources/包版本 metadata、失败 artifact。应用于 P2C Xvfb/Pillow 和生产 GUI P2B Xvfb/Weston。
- 去除 P2B workflow 对 P2C 分支 push 的触发；P2C PR 仍保留 P2B 主窗 XTEST / Weston 与 P2C 独立设置 XTEST/截图对照，避免重复设置截图；P2C/P2B workflow 分别按 PR 分组，`cancel-in-progress:false`。没有删 Windows baseline、Linux screenshot/comparison job 或改变任何几何/raster 阈值。
- CI-only 提交 `406491b70594d202f3d2bb4efa667f2e0886c552` 推送到既有 P2C 分支，未改 main/其他 PR、未创建 PR/合并。P2C run `40336249517` 全部 3 job PASS；artifacts Windows `12657162841`、Linux X11 `12656813101`、comparison `12656279296`、APT diagnostics `12656977979` / `12656638843`。P2B GUI `40336249521` 两 job PASS，artifacts X11 `12655914336` / Weston `12656259412`、APT diagnostics `12655954131` / `12656972640`。PR build `40336249519` 与 push build `40336242138` PASS；PR check 7 PASS / 0 FAIL。唯一 skipped step 是 P2B Xvfb 中重复的 settings test，由 P2C 同 head Xvfb job 完整覆盖。没有失败或取消必要 job。
- 新 P2C Xvfb 的 APT update/install 16/26 秒，Pillow compare 14/27 秒，P2B X11 6/27 秒，Weston 9/23 秒。Runner `ubuntu24 20261023.334.3` / Ubuntu 24.04.5 / APT 2.8.3；APT logs 区分了 update/install 并记录包版本和源。P2C Linux 的 6 个 source/config SHA、2 个 binary SHA 全验证；Windows PNG hashes 41/41、Linux PNG hashes 17/17 verified。比较器未变：184 geometry/10 nav/102 tree/36 raster PASS；10 页面差异率较 P2C-2 下降，1:1 仍 NOT PASS，语言 12 `DECISION_REQUIRED` 仍不变。详细 id/hash 在 P2C-3 visual report 和 artifacts。
- 本机非 GUI 验证：project verify 33 项 PASS，C/Python safety tests 3/3 PASS，所有 `scripts/*.sh` 与 `scripts/ci/*.sh` shell 语法 PASS，Zamak 0.2.0 check/test (29/29)/build PASS，`scripts/build-arch.sh` PASS，`git diff --check` PASS。`build-and-test` 现有 apt install 在 push/PR 用时 7:42 / 8:43 均通过，仍是 15 分钟 job 的未加固风险，本轮未改 `test.yml`。未启动 GUI/Compositor/Xvfb/Wayland，未访问 Hyprland IPC、桌面输入、drop_caches、Polkit、Swap/ZRAM。

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

## 2026-10-27 — P2C-4 最终证据同步（516e824）

- 复核 PR #7 最新 ChatGPT review：最新正式 review 仍是 P2C-3R1（针对 `2c745710210c8794b52a0e9eb19dd1830c00d8d6`），其 CI gate 已接受，但整体 Windows/GTK 1:1 仍 NOT PASS；未发现更新的 review 或未解决行内评论。
- P2C-4 exact evidence head `516e824cf4bbb840650bccdbbfdedb2d352d9090`：P2C run `40415153443` 为 Windows native、生产 Xvfb/XTEST、视觉比较 3/3 PASS；P2B run `40415153555` 为生产 Xvfb/XTEST + Weston Headless/Pixman 2/2 PASS；PR build `40415153495` 与 push build `40415146990` PASS，PR checks 7/7。P2B 重复设置页步骤条件跳过，由同 head P2C 完整覆盖。
- Artifact digest/SHA 和指标同步至 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`、`docs/UI_INTERACTION_MATRIX.md`、三层记忆。几何 184/184（最大分量 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36、滚动 2/2 PASS；亮色差异率 5.0741%–14.8853%，暗色 5.9434%–73.2932%，整页 1:1 NOT PASS。
- 生产 UI 保持源码 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；没有生产字体/UI/阈值改动。Windows 专有字体未打包，12 项简中来源差异仍 `DECISION_REQUIRED`；托盘和无安全路径的清理弹窗仍 BLOCKED；Wayland 仅生命周期通过，输入 NOT RUN。
- PR #7 保持 OPEN / Draft。没有合并 PR、真实清理、Polkit/Swap/ZRAM 写入、root GUI、活动 Hyprland IPC 或本机桌面输入。

## 2026-10-27 — P2C-4 最终 artifact 与 APT 诊断复核（105d71d）

- PR #7 仍 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，head `105d71d567d7ba096e8b928c072ceb4af15bfe6f`；最新正式 ChatGPT review 仍为 P2C-3R1，接受 CI gate，但整体 1:1 NOT PASS。未发现更新 review；无新的行内 review。
- exact-head P2C run `40416882722` Windows native / production Xvfb-XTEST / visual compare 3/3 PASS；P2B run `40416882708` Xvfb-XTEST / Weston Headless-Pixman 2/2 PASS；PR build `40416882739`、push build `40416873128` PASS，7/7 PR checks 成功。P2B 重复五页步骤按条件跳过，P2C 同 head 作业已完整执行。
- APT artifact 核验：Ubuntu 24.04.5、runner image `ubuntu24 20261023.334.3`、APT 2.8.3。P2B Xvfb update 12.6 MB / 1 秒成功；install 101 MB / 36m11s（作业步骤 36m23s），`fonts-noto-cjk` 61.2 MB，镜像为 `azure.archive.ubuntu.com`，APT exit 0。没有观察到 DNS、锁、缺包或仓库错误；慢速已证实，根因未证实。
- P2C Artifacts: Windows `12695303623`, Linux X11 `12694879677`, visual/dark/font `12694609240`; P2B: X11 `12696373914`, Weston `12695762656`;诊断附件和每个 ZIP digest 记录在 P2C-4 report。复核 P2B X11 10/10 screenshot SHA、6 个源码 SHA 及 production/helper 二进制 SHA；生产窗口正常关闭 exit 0。P2C PNG 清单 Windows 41/41、Linux 44/44、visual 83/83、dark 70/70、source 10/10 匹配。
- 几何/局部栅格保持 184/184、导航 10/10、tree 102/102、raster 36/36、中段滚动 2/2 PASS；明/暗全页差异率范围仍分别 5.0741%–14.8853% / 5.9434%–73.2932%，1:1 NOT PASS。12 项来源差异 `DECISION_REQUIRED`；Windows tray 和没有无副作用获取路径的 clean popup/confirmation/result 仍 BLOCKED。
- 无生产源代码改动；本次提交仅同步 exact-head 报告/交互矩阵/记忆。未执行清理、Polkit、Swap/ZRAM、root GUI、活动 Hyprland IPC 或主机桌面输入；推送后按 PR 页面核对新文档 head 的 Actions。

## 2026-10-27 — P2C-4 最终 exact-head 闭环（748a61e）

- 对 PR #7 的最新正式 Review 与所有行内评论完成复核；最新仍是 P2C-3R1，CI gate 接受、整体 1:1 NOT PASS。PR 为 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`。
- 对应 evidence head `748a61e62c597793e0acedb3f83bccb12f950285` 的 P2C run `40422156194` (3/3)、P2B run `40422156208` (2/2)、PR build `40422156138`、push build `40422151693` 均 PASS，PR checks 7/7。Windows/Linux/visual artifacts、APT diagnostics、digest 与 PNG/source SHA 清单记录于 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。
- Weston 的实际依赖安装成功但耗时 21m45，79.6 MB 以 61.6 kB/s 下载；无 DNS、包锁或包缺失证据，底层原因未确认。Wayland 主窗和设置窗客户端各退出码 0；`gdk_seat_get_keyboard` critical、未公告 `wl_seat` 与 SIGTERM 后 layer teardown `BUG` 均保留，不推断因果或声称 compositor 正常关闭。
- 184/184 几何、10/10 导航、102/102 树线、36/36 控件栅格、2/2 中间滚动 PASS；全页像素仍 NOT PASS。Liberation Sans 未用于生产；12 项来源差异维持 `DECISION_REQUIRED`，Windows tray 与无安全清理路径的弹窗 `BLOCKED`。
- 本轮只同步报告、交互矩阵和三层记忆，生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b` 未改。无真实清理、特权操作、本机 GUI 或活动 Hyprland 操作。

## 2026-10-27 — P2C-4 evidence 修订与 Actions 最终核对（4f84fb9）

- 对 PR #7 重新核对：OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，现有正式 review 仍为 P2C-3R1，无新的 Review 或行内意见。远端和本地 evidence head `4f84fb9a9e247a5987a11cd435e5da4b05f78a53` 一致。
- 精确 head 的 P2C run `40425954672`（Windows 原版、生产 Xvfb/XTEST、对照 3/3）、P2B `40425954460`（Xvfb/XTEST、Weston lifecycle 2/2）、PR build `40425954317` 与 push build `40425944842` 均 PASS；PR checks 7/7。artifact IDs、GitHub SHA-256 digests 和本地 manifest 计数见 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。
- 最新度量 184/184 geometry，nav 10/10，tree 102/102，raster 36/36，scroll 2/2 PASS；亮色 changed-pixel 5.0741%–14.8853%，暗色 5.9434%–73.2932%，完整 1:1 NOT PASS。字体候选未进入生产；12 项原版简中来源差异保持 `DECISION_REQUIRED`。
- 本次 APT 成功且诊断可追溯，过去下载变慢的根因仍未知。Weston 客户端退出 0，保留 GDK seat 和 Weston teardown 警告；Wayland 输入 NOT RUN。无真实清理、特权操作、活动 Hyprland 或用户桌面输入。

## 2026-10-27 — P2C-4R1 Windows 深色/语言状态复验（ff5794f）

- 对固定 Windows Mem Reduct v.3.5.2 基准执行四种互相独立的语言/深色顺序。20/20 页的语言、应用暗色勾选、导航选中项与页面资源 ID 匹配；相同语言顺序截图 SHA 相同，十组顺序对照变化为 0。每个进程均 WM_CLOSE 设置页后重新打开，4/4 恢复导航/页面/语言和暗色菜单状态。新实验没有复现简中暗色混合亮暗控件。
- 旧简中图存在导航与页面不对应；状态证据见 R1 报告。旧采集错配的具体发生环节未确认，不归咎于原版应用或 Linux。比较器语言关联已修复；最终 R1 P2C run `40443810060` 3/3 PASS，Artifacts `12707093733` / `12706823610` / `12706903621`。
- R1 暗色语义配对的五页英文/简中差异率、截图 SHA 与环境写入 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`；交互状态矩阵写入 `docs/UI_INTERACTION_MATRIX.md`。几何、控件栅格、截图阈值、12 项 `DECISION_REQUIRED` 文案均未更改。
- R1 代码改动仅比较脚本的语言 ID 映射；生产 UI 源码仍是 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。aggregate checkpoint-to-HEAD 还有 Windows 采集脚本与两条 UI workflow 的改动，文件分类见报告。旧代码 head P2B run `40443810053` 最终 2/2 PASS（APT 依赖安装 27m48s）；当前 metadata head `7369db0` 的 build run `40445420366`、push build `40445426755` PASS，Windows job PASS，Linux Xvfb/P2B jobs仍在执行。未进行主机桌面操作、清理或特权写入。

## 2026-10-27 — P2C-4R1 exact-head CI 与 artifact 闭环（c847710）
- 核对本地和远端 PR 分支均为 `c84771063e6b349828e012463222b82bc57dd479`；PR #7 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`，未合并。`7369db0..c847710` 仅有报告、交互矩阵和记忆变化，生产 UI、比较/采集脚本和 workflow 未变。
- c847 的 P2C run `40448208937`（Windows 原版、Linux production Xvfb/XTEST、visual compare）3/3 PASS；P2B `40448208791` Xvfb/Weston 2/2 PASS；PR build `40448208724` 与 push build `40448202360` PASS。P2B 的重复五页设置步骤条件 SKIPPED，由同 head P2C Xvfb 作业实际执行五页测试；所有必需 job 均成功。
- Artifacts：P2C Windows/Linux/comparison `12709690360` (`sha256:e9ed3a52bf6354000586752209a724ff30521190094f1f0f6f93becf59939c33`) / `12708759424` (`sha256:2fcf48320346316938834c9b1701017cc6e97a25e7ce276ce598c32540a58d71`) / `12709710212` (`sha256:8304c023bc881260f7462ee7e9482097ecdf94ec66fcd8d2758ffca206874f68`)；P2B X11/Weston `12708669133` (`sha256:810b6ae3485ac6d658081e85099b314201ebe2fe2ff781ef551b43a62a9d9174`) / `12709227100` (`sha256:46da0401b1cb495d9929b83f0f79b8a8bf73394002bda811749c267e2d7a82cd`)。PNG/source manifest 计数和代表截图 SHA 见报告。
- 四种 Windows 语言/暗色顺序试验 20/20 状态匹配，设置窗关闭/重开 4/4 PASS；未复现混合亮暗。有效暗色差异率 5.4004%–12.6625%，整体像素 1:1 NOT PASS。184 geometry、10 nav、102 tree、36 raster、2 scroll 均 PASS；12 项简中来源差异仍 `DECISION_REQUIRED`。
- P2B Weston 主窗与设置窗客户端退出码均为 0，但保留 GDK `gdk_seat_get_keyboard` critical、无 `wl_seat` 和 compositor layer teardown `BUG`；Wayland 输入 NOT RUN。无生产 GUI 变更、活动 Hyprland/桌面输入、真实清理、Polkit、Swap/ZRAM 或内核缓存写入。阶段停止于等待 ChatGPT 复审 PR #7。

## 2026-10-27 — P2C-4R1 final exact-head artifact audit (90b2308)
- Current PR #7 head 90b23084f0f5f31f15f7b5983b19999ea37a70bc, base 911a3d0b99b05f15bd0109dca475b0b5174e7888, remains OPEN / Draft. Latest formal ChatGPT review remains the P2C-4 review on ab0f62c; its bounded R1 request is closed by the four clean-process experiments. No merge or privileged feature work.
- Exact-head P2C 40449858329 (3/3), P2B 40449858347 (2/2), PR build 40449858304 and push build 40449852425 all PASS. P2B repeated settings-page step is conditionally SKIPPED; the exact-head P2C Xvfb job actually ran the full bilingual five-page suite.
- Artifacts and GitHub digests: P2C Windows 12709528954 (sha256:a4614a46dc09ff319dd2a8bb160c2afd06b8e12b16b26e93ae941abd1028bbe6), Linux X11 12710625392 (sha256:80cb7dd8191a390d746193da43fd88da974298acd725c0c8b46461dcd23388ce), comparison 12709712520 (sha256:c32886e4b07f8c19c5691d8a95debfffc5067155fa17ad353882be7329a20514); P2B X11 12709933995 (sha256:2a88b89dd245a3aa54705820c8d54b0e4212c49f1150f4821d241f177b822851) and Weston 12710380599 (sha256:bcb29e53c3e18bfb6197816e20f05d4d6f9a27c8ec180197d8bcf7061ab39ac7).
- Recalculated artifacts: dark-theme manifest 155/155, visual SHA manifest 83/83, Linux PNG manifest 44/44, P2C source 10/10, P2B X11 PNG 10/10, source/binary 8/8. Windows R1 experiments 4/4, semantic page rows 20/20, page controls 200/200 visible with 196 enabled; WM_CLOSE/reopen 4/4; transition-order comparisons 10/10 zero-diff.
- Valid dark changed-pixel range 5.4004%–12.6625%; whole-page 1:1 remains NOT PASS. Geometry 184/184 (max 1.5 px), navigation 10/10, raster 36/36, tree samples 102/102, scroll state 2/2 PASS. zam test 30/30, governance 33, shell 4, C safety 3 PASS. Weston clients exited 0; preserve GTK seat critical, no advertised wl_seat, and compositor layer teardown BUG; no SIGABRT/SIGSEGV was present, and root cause is unknown.
- P2B Xvfb dependency install lasted about 19m40s but returned exit 0 and diagnostic classification PASS. No underlying slow-install root cause confirmed. No production GUI source changes, real memory cleaning, Polkit, Swap/ZRAM, root GUI, active Hyprland IPC, or host-desktop input.

## 2026-10-27 — P2C-4R1 artifact rerun after handoff correction (716a1ca)
- PR #7 remained OPEN / Draft, base `911a3d0b99b05f15bd0109dca475b0b5174e7888`; evidence HEAD `716a1caaadeb780d32e8b9604961c333a752825d` changed only report, interaction matrix and memories versus prior evidence HEAD `90b2308`. Production GUI and capture/comparison source manifests were unchanged.
- Exact-head P2C `40453430941` passed 3/3, P2B `40453430924` passed 2/2, push build/governance/C-safety/Zamak `40453423515` passed. P2B duplicate settings-page step was conditionally skipped; P2C ran the full bilingual five-page capture and interaction flow.
- Artifacts: P2C Windows `12711301420` (`sha256:04146be8bcd7a2c62c79f9a118c9a2617cf1c9141b48db0f402c49bb6bf03632`), Linux X11 `12711351794` (`sha256:50a0994cd71ffafc69049b4d25dd69acdbbb61d18271a8212a433e84423544f6`), comparison `12711536604` (`sha256:fbfe08b2d50f5f52c4ac957e997dcbad60566c9073f244a40b5fec57ec520032`); P2B X11 `12711018044` (`sha256:d9d43ae6219f82c60307d160462f082030e4ee232b065db3d2127f9cd9259a34`) and Weston `12711281579` (`sha256:1a55915bc3858d3753701d140651517520c4f83d8124329823b0664197f452c3`).
- Rehashed manifests: dark 155/155; visual 83/83; Linux PNG 44/44; P2C source 10/10; P2C source/binary 8/8; P2B X11 PNG 10/10 and source/binary 8/8. Geometry 184/184 PASS, maximum component 1.5 px; navigation 10/10, raster 36/36; scroll comparison 2/2 remains compare-only. Four clean Windows processes / 20 semantic page pairs pass; no mixed dark state reproduced. Full details and refreshed raw PNG hashes are in `docs/P2C-4_FONT_THEME_MENU_REPORT.md`.
- No production UI changes, real memory cleaning, Polkit, Swap/ZRAM writes, root GUI, active Hyprland IPC, or host desktop input injection.


## 2026-10-27 — P2C-4R1 exact-head evidence and project-memory closure (37d0745)
- 核对分支本地/远端一致为 `37d07453d60c4d1046f5aad6577c9425c271180a`，PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`。最新 ChatGPT review 仍为 P2C-4 对 `ab0f62c...` 的复审；本轮无生产 GUI 修改。`716a1ca..37d0745` 只改报告、交互矩阵与记忆。
- Exact-head P2C `40454514318` 3/3、P2B `40454514305` 2/2、PR build `40454514283`、push build `40454508196` 全部 PASS；PR checks 7/7。Artifact IDs/digests、逐页 PNG SHA、来源与二进制 SHA 写入 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。本地复算：visual 83/83、dark 155/155、Linux screenshots 44/44、P2C source 10/10、P2B X11 PNG 10/10、source/binary 8/8。
- Windows 四组新进程实验 20/20 页面语义有效、10/10 顺序图像对照零像素差、4/4 WM_CLOSE 后重开、104/104 暗色区域采样未发现混合亮暗。旧错配图源头未知，不归因。当前五页双语暗色指标 5.4004%–12.6625%，整页像素 1:1 仍 NOT PASS；12 项语言来源差异维持 `DECISION_REQUIRED`。
- Weston 13 Headless/Pixman 下主窗和设置窗客户端退出码 0；Wayland 输入 NOT RUN。保留 `gdk_seat_get_keyboard` critical、未广播 `wl_seat` 和 SIGTERM teardown layer `BUG`，根因未知。没有活动桌面操作、真实清理或特权写入。PR 等 ChatGPT 复审，不合并。
## 2026-10-27 — P2C-EXIT / P3A-PRE 集成审计与无特权偏好设计

- 基线分支 `feat/issue-3-p2c-settings-ui`，开始 HEAD `556a727c100aad4af1a25c48c10513c4dd42c5c7`，与远端一致；远端 main `080417e153d91bf2adac756e5154311fef49333f`，P1 `11b0b9444cd94f0026b855abe3849a06106426f2`，P2A `d89d74fde433da1863b4de0223f974c824fb590e`，P2B `911a3d0b99b05f15bd0109dca475b0b5174e7888`，P2C `556a727...`，probe `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1`。
- 复核 #2/#4/#5/#6/#7 均 OPEN / Draft、GitHub mergeable=true、未授权合并。Current exact-head Actions：P2C `40456081269` 3/3、P2B `40456081264` 2/2、PR build `40456081238`、push build `40456075786` 均通过；复审要点与每个 PR 的文件范围记于 `docs/PR_INTEGRATION_READINESS.md`。
- 只读 `git merge-tree` 模拟 sibling PR4 与 PR5/P2B/P2C 均在 `memory/SESSION_LOG.md`、`memory/SHORT_TERM.md` 内容冲突；未写工作树。PR4 12 项 probe tests 不在生产 P2B/P2C 分支，集成方案明确要求保留两类测试。
- P2C 阶段按 ChatGPT review `5662810728` 归档：PR HEAD `556a727`、生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`、图形源码 checkpoint `37d07453d60c4d1046f5aad6577c9425c271180a`；20/20 页面语义、4/4 重开、184/184 几何 max 1.5 px、导航10/10、tree102/102、raster36/36。整页 1:1 NOT PASS；Wayland 输入 NOT RUN；tray/清理弹窗 BLOCKED；12 项中文来源差异 DECISION_REQUIRED。
- 阅读 Zamak v0.2.0 官方 stdlib 文档：`std/fs` 有 read/write/mkdir/rename 等，但公开列表未提供 no-follow、mode、锁与 fsync API。新增 `docs/ADR/0004-user-preferences-persistence.md` PROPOSED 草案，仅支持语言与应用主题；新增 PR 集成准备度报告。未添加生产代码。
- 已检索确认没有现成 P3A Issue，并按任务授权创建 [Issue #8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)。PR #7 顶层交接评论为 [issuecomment-6435453708](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#issuecomment-6435453708)；本轮不合并、不修改 main、不执行清理/Polkit/Swap/ZRAM/活动桌面操作。
- 首次文档提交 `b1f2fd95333a26af1c3ea51e6fea4a245db640c9` 已推送到既有 PR #7。精确该 head 的 P2C/P2B/PR build/push Actions `40461922846` / `40461922577` / `40461922736` / `40461917796` 全成功；比较器再次报告 184/184 几何、36/36 栅格、10/10 导航、102/102 树线，整页 1:1 仍 NOT PASS。Artifacts 和 ZIP digest 已补入 P2C 报告。
- 本轮本机只做了 `git diff --check` 和提交内容敏感路径扫描，没有运行本机项目测试；最新 GitHub hosted checks 是 b1f2fd9 上的成功作业。本会话最后追加的证据整理仍是文档/记忆变更，不改应用、测试脚本或 workflow。

## 2026-10-27 — PR 集成报告校正与 663a9d3 exact-head 复核

- 复核远端 PR #7 为 OPEN / Draft，head `663a9d3e627ccb2d9bcb884cd708f140006f58a5`、base #6 `911a3d0b99b05f15bd0109dca475b0b5174e7888`；PR checks 7/7 PASS。#2/#4/#5/#6 仍 OPEN / Draft，head/base 未变。PR #7 仍关联既有评论 `issuecomment-6435453708`。
- 663a9d3 的 P2C run `40463959137` 3/3 PASS，P2B run `40463959086` 最终 2/2 PASS；PR build `40463959092`、push build `40463951002` PASS。P2B Weston 首次 attempt 超时（退出标记在 15 秒内未出现），失败 artifact `12716375114` 保留；只重跑该失败 job 后成功，artifact `12716440984`。日志没有 compositor crash 证据，超时根因未确认。P2C/P2B artifacts、GitHub digests 及几何/视觉值列在 `docs/PR_INTEGRATION_READINESS.md`。
- `PR_INTEGRATION_READINESS.md` 将 #7 changed-file 数修正为 30，并将 556a727 初始审计标明为历史快照，避免与 663a9d3 当前状态混淆。只改文档/记忆，没有 UI、采集/比较源码或 workflow 变化。本机 `verify_project.py` 33 项、C/Python 安全 3/3、Shell 语法、Zamak 0.2.0 check/test/build（30/30）均通过。
- P2C 接受基线、Wayland 输入 NOT RUN、tray/清理弹窗 BLOCKED、整页 1:1 NOT PASS、12 项中文差异 DECISION_REQUIRED 均保持原判定。Issue #8 开放；ADR-0004 仍 PROPOSED。无合并、强推、main/其他 PR 分支修改、特权操作或主机桌面触碰。


## 2026-10-27 — P3A-PRE-R1 ADR 与集成审计修正

- 基线 `629fc3f88ee91d0cc3582d6be1789991fc3f72b6`，PR #7 OPEN / Draft。先复核 AGENTS、长期/中期/短期记忆、P2C 报告、UI 矩阵、测试计划、ADR、Issue #8 和 ChatGPT review `5663699910`。
- PR #2/#4/#5/#6/#7 仍全 OPEN / Draft；未做 merge/base/ref 修改。#4 远端 workflow 过滤条件、PR #6 尚未解决的采样失败线程及 exact-head CI artifact 均实查。
- 已修订 ADR-0004：明确定义类型化 load/store 结果、rename 与目录 fsync 的状态转换、不确定提交回读协调、重试和幂等规则、损坏/future-schema 恢复/Reset、路径根 symlink 与 fd-relative 安全策略、稳定锁文件及故障注入测试。默认和实现授权仍待产品/架构审批。
- 已更新 `docs/PR_INTEGRATION_READINESS.md` 的当前 PR 快照、P2C-4R1 归档、#4 测试能力保留、记忆文件冲突处理、PR #6 Review 拟回复和 P3A Review 对应表。三个 ADR inline 线程仅记录处理，不直接 resolve；PR #6 thread 仍未回复/未关闭。
- exact-head #7 CI 基线 runs `40465893251`、`40465893241`、`40465893268` 均成功；PR#7 7/7。已下载 Xvfb artifact `12717146938` 并核验 `client.log` 中采样失败、失效 redraw 与 unavailable/em-dash 标记；artifact digest 和日志 SHA 写入集成报告。
- 本地验证 PASS：治理检查 33 项、Python/C 安全测试 3/3、Shell 语法、Zamak 0.2.0 check/test/build（30/30）、`git diff --check`。变更仅文档/记忆。待提交推送后检查新 HEAD Actions，再向 Issue #8 添加带最终 SHA/CI 结果的 P3A-PRE-R1 评论。
## 2026-10-28 — P2B-INTEGRATION-PRE：PR #6 本地集成模拟

- 核对远端：main `b78785af233dbae7a80085fd7a9f45f30ae42711`；PR #6 head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，旧 base 分支 `docs/issue-3-p2a-ui-spec` 当前为 `a19474ca3f79de65a18e76f6852ab503ce11b173`；PR #7 head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`。PR #6 为 OPEN/Draft 且 GitHub 报 DIRTY；PR #7 为 OPEN/Draft 且报 CLEAN。未改远端状态。
- 在独立 worktree 从 PR #6 head 执行 `git merge-tree --write-tree origin/main origin/feat/issue-3-p2b-main-window`；四个文档冲突：`docs/UI_FIDELITY_TEST_PLAN.md` 与三层记忆中的 MEDIUM_TERM、SHORT_TERM、SESSION_LOG。解决草稿分别保留 P2A/PR #4/PR #5 与 P2B 历史，更新当前 PR #5 已合并状态；未提交，未改 Base。
- 保留 P2B 生产源、GUI 测试和报告；候选树包含 PR #4 独立探针、12 项测试、Xvfb/Weston 脚本、来源清单及 APT 诊断脚本，probe 源 SHA-256 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3` 与清单一致。
- `.github/workflows/p2b-production-gui.yml` 本地 CI 草稿加入 main 与 Base 编辑/同步事件、分离 APT update/install 诊断步骤、有限步骤及 60 分钟 job 超时、Node 24 固定 Action SHA、`if-no-files-found: error` 与不取消运行中证据作业的并发策略。PyYAML 语法解析 PASS；actionlint 未安装。
- 本地结果：治理 29 项、C/Python 安全 3 项、Shell 语法、主项目 Zamak 0.2.0 test 13/13 与 check/build、Arch 原生构建、生产 GUI build、独立 Canvas probe test 12/12、`git diff --cached --check` / `git diff --check` PASS。没有 Xvfb/Openbox/Weston/wayland-info，因此隔离生产 GUI XTEST 与 Weston 生命周期本地运行 NOT RUN；无 root 安装、Hyprland 输入或内存/Swap/ZRAM 写入。
- 实际复核 PR #6 head `911a3d0…` 的旧-base GUI run `40208944364` 与 build run `40208944120`，两者均成功；X11 artifact `12604299350`、Weston artifact `12604988043`。截图确认三组竖排、双语及语言菜单边界；采样失败截图显示三组数值即时失效。此类旧-base CI 不代表 main-base CI。两条结构/语言菜单旧 review thread 仍未解决，采样失败重绘 thread 已解决；本轮没有关闭任何 GitHub thread。
- PR #7 仍堆叠于 PR #6。相对原 base `911a3d0…`，PR #7 为 30 个路径；P2B 与当前 PR #7 同时修改 `p2b-production-gui.yml`、MEDIUM_TERM、SHORT_TERM，后续合并要手工合并这些差异。PR #7 的 `apt-diagnostics.sh` 与 main 为相同 blob。其他 #4 文件在 PR #7 树中缺失属于旧分支基线差异，不是其相对 PR #6 的删除；PR #6 集成后须按新祖先关系复核。
- 未创建 commit、未 push、未改 PR Base/Ready 状态或 main/#7；无 GitHub main-base 新 CI。

## 2026-10-28 — P2C-MAIN-INTEGRATION-R1 本地合并候选验证

- 只读复核远端：main `83dbf58fb039645f08e08bc89170adcdaa7997be`，PR #7 head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`；PR #7 base 分支 `feat/issue-3-p2b-main-window` 当前远端 HEAD `483ac837cd51d3293baf6c3d8b72309dd02f2ff9`，旧 head 的共同祖先为 `911a3d0b99b05f15bd0109dca475b0b5174e7888`。PR 仍 OPEN/Draft，GitHub 报 mergeable=false；8 条历史 Review 线程 outdated/unresolved。
- 隔离候选以 PR #7 原 head 为第一父、main 为第二父；初始 `git merge-tree --write-tree` 的结果 `73ab4bed3a3a9c0d09f0c4b63caf996a007ab030` / exit 1，真实冲突为 P2B workflow 和 MEDIUM_TERM、SESSION_LOG、SHORT_TERM。`test.yml` 自动合并，APT 诊断脚本两侧 blob 相同；在 SHORT_TERM 中恢复并标注 PR7 分支的 P3A-PRE-R1 历史快照。
- PR #4 的 12 项 Canvas 探针源码、CI 与来源文件均保留；probe build/check/test 12/12 通过，source SHA-256 为 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。PR #6 生产 GUI 路径保留；`p2b-production-xvfb.sh` 只有基于 12 px 内容内缩校准的鼠标坐标变化；`p2b-production-wayland.sh` 保留主窗生命周期并增加私有 Weston 设置窗表面/退出验证，不声称 Wayland 输入；`build-production-gui.sh` 增加设置 UI 与 GTK bridge 源文件 SHA；失败采样后的清空/重绘/不可用值断言仍存在。
- 本地 PASS：治理 33 项、Python/C 安全 3/3、Shell 语法、Zamak 0.2.0 check/test 30/30/build、Arch 原生构建、四个 workflow YAML 解析、candidate 对 main 与原 PR7 head 的 `git diff --check`。本机缺少 Xvfb、Weston、actionlint，故真实隔离 XTEST/Weston 与 actionlint 为 NOT RUN。
- 候选未推送；无新的精确候选 HEAD GitHub Actions。未改 PR #7 Base/Ready/merge，未更改 main/#6 远端；未执行特权操作或活动 Hyprland 输入。

## 2026-10-28 — 第三方界面路线复核与内存 helper 测试硬化

- 起始本地安全工作树 `codex/p2c-main-integration-r1@cd6703d670d82f9712e83012dde0325efec5e615`。只读核验 PR #7 仍 OPEN / Draft、Base main、远端 Head `019a7f1e0755570c2c3a0b1202a6c764540f14e2`；远端 main `83dbf58fb039645f08e08bc89170adcdaa7997be`。本轮未推送、未改分支 Base/main；旧 P2B 审计工作树未触碰。
- 查阅 Microsoft、Qt、GTK、wxWidgets、Wine/Winelib 与 Linux kernel 官方资料，并复核现有 GDI/Pango/截图证据。Qt 6.12 `QWindowsStyle` 在本机仍映射 Liberation Sans，单纯换库不会复制 Win32 GDI 控件；推荐后续固定尺寸 General 单页 Qt `QProxyStyle` POC，保留原文资源与状态像素验收。字体/暗色和几何现有 PASS 项不扩大为整体 1:1。
- 更新 `docs/ZAMAK_UI_FEASIBILITY.md` 的第三方工具路线、字体与真实差异、单页 POC 建议；`docs/ADR/0005-manual-page-cache-reclaim.md` 保持 PROPOSED，限制为未来手动页缓存候选，不连接 GUI。
- harden `native/memreduct_helper.c`：生产路径固定，测试宏单独开放临时普通文件；拒绝 symlink/非 regular target，EINTR 一次重试、短写/close 错误作为 outcome uncertain，不自动重试。`tests/test_native.py` 与 `tests/native_write_shim.c` 注入 EINTR/短写并测试临时文件，无真实 `/proc/sys/vm/drop_caches` 写入。
- 本轮最新生产脚本：治理 34 项、C/Python 安全 7/7、Zamak 0.2.0 check/test（41/41）及 GUI/helper build PASS；独立 Canvas probe 12/12 PASS。构建 Artifact 写入外部报告目录。Xvfb/Openbox/Weston 工具缺失，本机隔离 GUI 输入/生命周期未运行，用户活动 Hyprland 未触碰。
- 新增/修改文件仍在工作树，待做最终 diff/SHA 与完整回归；不推 PR #7，不改 Ready/Merge。

## 2026-10-29 — Winelib/Qt/GTK 路线核验与内存核心提交

- 从 `codex/p2c-main-integration-r1@cd6703d670d82f9712e83012dde0325efec5e615` 继续；只读核对远端 main `83dbf58fb039645f08e08bc89170adcdaa7997be`、PR #7 `019a7f1e0755570c2c3a0b1202a6c764540f14e2`，PR #7 保持 OPEN / Draft / Base main。没有远端写入；`p2b-r6-integration-audit` 预暂存 worktree 未触碰。
- 官方资料确认 Winelib 的 `winegcc`/`wrc` 可重编译 Win32 C/RC 资源；Qt `QProxyStyle` 可局部重画，`QCommonStyle` 适用于完整自定义样式；GTK CSS/Pango 仍受主题级联与 Linux 字体 fallback 影响。Winelib 是优先验证的原资源单页 POC，不保证 Windows GDI 像素一致。最新 WineHQ 开发版 11.20；2026-08 的 11.15 Arch Wayland bug 报告仅作为待复验风险。当前本机缺少 Wine/Winelib、Xvfb、Weston，故 Winelib 与本机隔离 GUI 交互均 NOT RUN。
- 内存 core 修改 8 个文件，提交 `f8bd6cc876b3cbb4623d6dbe6f5d4b94a348c05d`、Tree `eaa616a12bc17761881fbf86d5cc9a209a96a670`、父 `cd6703d670d82f9712e83012dde0325efec5e615`。严格检查 `/proc/meminfo` 的 kB 单位并拆分 Cached/SReclaimable；helper 仅 pagecache 固定路径，测试临时文件注入 symlink/FIFO/EINTR/短写/close/waitpid 故障。GUI 未调用 helper，测试没有触碰 `/proc/sys/vm/drop_caches`。
- 回归 PASS：治理 34、C/Python 11/11、Shell 脚本语法、Zamak 0.2.0 check/43 tests/build、生产 GUI/helper build、独立 Canvas 12/12、Arch build、`git diff --check`。本地无新 GitHub CI。
- 视觉事实沿用现有精确基线：设置几何 184/184、最大 1.5 px；同源文本 25/120 在 2 px 内；整页 1:1 NOT PASS；12 项简中差异 DECISION_REQUIRED。有限 Wayland 图像有 SHA，但没有 compositor 外框物理尺寸，且不是本次交互验收。报告已追加在 `docs/ZAMAK_UI_FEASIBILITY.md`，README 与 MEDIUM/SHORT/SESSION memory 随此文档候选更新。
- 不运行清理、Polkit、Swap/ZRAM、root GUI、活动桌面输入；未推送、未改 PR Base/Ready/Merge、未改 main。

## 2026-10-29 — 用户反馈后的视觉差异盘点与路线校正

- 复核用户指出的窗口观感、字体及布局问题。设置页几何 184/184（最大分量 1.5 px）仍只覆盖控件矩形；同源 120 条文本仅 25 条宽高差≤2 px，95 条超差。有效配对五页浅色整页 changed-pixel 为 5.0741%–14.8853%，深色为 5.4004%–12.6625%；整页 1:1 均未通过。简中资源 12 项差异仍 `DECISION_REQUIRED`。主窗历史 20.9436% 配对因数据/边界/安全状态不一致，不是有效验收率。
- 现有 Wayland capture 报告 GTK main client allocation 337×369、scale factor 1；GDK_SCALE 1/2 旧截图尺寸相同。没有 Hyprland decoration/物理外框测量，因此“大窗口”成因未确认；本轮没有擅自缩小固定 DLU 尺寸或操作活动桌面。
- 查阅 WineHQ 官方资料并校正先前过度谨慎表述：Wayland driver 已在 Wine 9.22 发布说明中注明进入默认配置；Bug 60152 的 Wine 11.15 启动回归在上游标记为 11.16 已修，当前 Arch 仓库为 11.20-1。Wine/Winelib 仍不等于 Windows GDI，尚未安装或实测 Winelib；下一项前端试验改为官方 `resource.rc`/Win32 标准控件的 Winelib General 单页 POC，不再先做 Qt 平行演示。
- 本轮重新运行 `scripts/ci/build-production-gui.sh`（治理34、C/Python 11/11、Zamak 43/43、GUI/helper build）、Canvas probe 12/12、`scripts/build-arch.sh` check/test/native build、全 Shell 语法及 diff check，均 PASS。测试未写真实内核、不运行 helper、未触碰桌面。没有新 GitHub CI，PR #7 与 main 未修改。
## 2026-10-29 — R6 Winelib 结果收口与共享设置重绘修复

- 在 `codex/p2c-main-integration-r1` 继续完成此前开始的 UI 路线核验。代码修复提交 `640fb5df044475df827308388c3da5be25d4ffad`，Tree `f30cac1ce113016dfef58eca46417549253d2be2`，父提交 `f8bd6cc876b3cbb4623d6dbe6f5d4b94a348c05d`。远端 main、PR #7 与 PR 状态没有变化；未推送、未改 Base、未 Ready、未合并。
- R6 Winelib General 页证据来自固定上游 `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 的 `.rc`/标准控件子集。校正背景与焦点后，558×335 changed-pixel 为 English `13.2531%`、简中 `12.2725%`，背景样本 0/2500 不同；GroupBox 仍 20.5864%–21.7213%，按钮 37.2570%–37.6336%。英文 Winelib/GDI 垂直度量均为 ascent/descent `11/2 px`；简中需要 Noto Sans CJK SC，Winelib 为 `13/3 px`。Wine Common Controls v6 manifest 报缺 assembly。结果证明可复用资源，但不是 Windows GDI 1:1。
- 修复 `src/main.zm`：共享主题/语言动作现在会使已打开设置 Canvas 重绘；加入相关状态断言。`tests/test_native.py` 增加 fake `pkexec`，覆盖异步桥退出码 `1`、`3`、`126` 且验证不重复调用。
- 本轮最终本机结果：治理 34、C/Python 12/12、Zamak 44/44、生产 GUI/helper 构建、Canvas 探针 12/12、Shell 语法、`git diff --check` PASS。Wine/winegcc/wrc、Xvfb、Weston 未安装，本机 Winelib/隔离图形测试未运行；活动 Hyprland 未操作；未执行真实清理、Polkit 或内核写入。
- 前端差异清单：设置文本 95/120 宽高差大于 2 px；有效配对浅色 changed-pixel 5.0741%–14.8853%，深色 5.4004%–12.6625%；全页 1:1 NOT PASS。主窗 GTK 客户区 337×369 的 Windows DLU 换算仍 provisional，Hyprland 外框未测；12 项原版简中来源差异仍 `DECISION_REQUIRED`。完整技术路线与来源见 `docs/ZAMAK_UI_FEASIBILITY.md`。
- R6/当前构建证据保留在用户 AI 产出目录的 R6/R7 子目录；没有覆盖原截图。本地候选未取得 GitHub CI 结果。
- 文档、可行性路线、ADR、README 和记忆修订单独提交为 `84dcd46af45d0f30f7b64d25256054fefe569388`（父为代码提交 `640fb5df044475df827308388c3da5be25d4ffad`）；未推送。

## 2026-10-29 — 本地候选真实 Wayland 限时启动与截图

- 使用本地代码 HEAD `640fb5df044475df827308388c3da5be25d4ffad` 对应的生产二进制（SHA-256 `fe5c609e2718a2f348c7724ff44ccee43349488dcf9780afd1762075d9b08658`），通过 `GDK_BACKEND=wayland` 和内置 7000 ms 测试计时器打开 Main 与 English/Light General Settings。当前环境有 Wayland/Hyprland 会话标记；未调用 Hyprland IPC、未注入键鼠、未触碰其他窗口。
- GTK 3.24.52，scale factor 1，Adwaita Sans label style 11 pt，Canvas 请求 8 pt，记录 ascent/descent `11/3 px`。Main allocation/client `337×369`、Settings `558×335`；两个 app-owned capture 成功，GDK frame extent 与 client 相同，仍未量到 Hyprland 可见装饰边界。日志未见 Gtk-WARNING/CRITICAL/ERROR；`CI_QUIT source=bounded_test_timer`、`CI_APP_RETURNED`，退出码 0。
- PNG：Main `fe90da01aac4cfa96861d8db42bdbbe0127087b694071f893ec9d7b21f0e0f3a`；General `ee33df415eacd6e69d448cc269b44be7fd17092ee436bab616e0d494798e1551`。日志、环境记录和 PNG 在 AI 产出目录 R7 的 `verification-640fb5df/real-wayland/`。本次只验收启动/绘制/采样/计时器退出；鼠标键盘、五页切换、深色/简中、关闭重开仍 NOT RUN，整页 1:1 NOT PASS。

## 2026-10-30 — P2C-R8 资源控件本地实现

- 在 `codex/p2c-main-integration-r1` 完成源码提交 `3c41879d7589472fd64b7f8563268d60bc0b99bb`，Tree `c0cdd138aa1633dc46893e11e13edb60a64338e7`，父 `307cd889d9878ec01cf72937f84f67ee8252f50f`。新增资源映射与 GroupBox/AutoCheckbox/ComboBox 生产渲染/交互状态、滚轮事件桥、CI/Xvfb 测试扩展和 Windows 原生采集字段；没有包含 README、报告和记忆变动在这个代码 commit。
- 完整本地回归：Zamak 68/68、治理34、C/Python13/13、全 Shell12/12、生产 GUI/helper build、Arch/native build、独立 Canvas probe12/12、`git diff --check` PASS。当前 UI 执行脚本未获得运行环境：Xvfb script exit2（`Xvfb` 缺失）、Weston lifecycle script exit2（`weston` 缺失）；`wine`、`winegcc`、`wrc` 缺失，故 Winelib A/B NOT RUN。没有新 GitHub Actions。
- General English/Light 的历史截图在 GroupBox 修正前/后全页差异 9.3388%/7.6649%；这是早于最终 ComboBox 箭头改动的截图，不标为 exact-head。最终箭头栅格有 source test，无新截图。95/120 文本度量仍超 2 px；全页 1:1 NOT PASS。现有 184/184 等视觉计数不冒用为当前源码重验。
- PR #7 仍远端 OPEN / Draft、Base `main`、HEAD `019a7f1e0755570c2c3a0b1202a6c764540f14e2`，main `83dbf58fb039645f08e08bc89170adcdaa7997be`；未推送或修改 PR。Hyprland 活动桌面未操作；未运行清理、Polkit、Swap/ZRAM 或 root GUI。R8 exact tests/logs/source hashes 在 AI 产出目录 `报告与研究/Mem Reduct P2C-R8-2026-10-30/r8-closeout-20261030-v3/`。
