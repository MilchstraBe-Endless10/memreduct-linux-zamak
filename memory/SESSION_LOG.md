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
