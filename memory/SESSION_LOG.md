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
