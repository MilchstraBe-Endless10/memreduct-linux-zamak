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

## 2026-10-25 — P2A-R6 文档与隔离 GUI CI 补强（进行中）
- 规格分支基于 P1 SHA `11b0b9444cd94f0026b855abe3849a06106426f2`，PR #4 测试分支 HEAD `a522e5629fc89969533527b4d806e71e58fb9be7`；PR #2/#4 均 OPEN / Draft，未合并。
- 迁入并审阅上游 v.3.5.2 UI 基线、交互矩阵、差异报告、可行性、截图采集清单和 Linux 能力映射；强化官方紧凑尺寸约束：主窗口 224×214 DLU、五个设置页模板各 260×168 DLU，R6 336×348 CI 探针不作为生产尺寸。
- 本机项目验证 29 项、3 项 C 测试、4 个 shell 语法、Zamak 0.2.0 check、5 项 Zamak tests/build、Arch helper build 以及 diff whitespace 检查通过；未启动本机 GUI。
- R6 Actions：build push/PR runs `40189616005`/`40189619010` PASS；Wayland Headless jobs 在 `40189616016`/`40189619021` PASS；Xvfb 两次运行在独立 `windowclose` 测试失败。日志证据：未触发 `CI_WINDOW_CLOSE`，Xvfb 未运行 WM，GDK 报 unexpected destroy 与 GTK critical。hover→pressed ROI 与 pressed→release ROI 各有 8004 changed pixels。修复计划是在 Xvfb 中启动/跟踪/清理本次子进程 Openbox，再真实复测，不屏蔽 GTK 错误。
- 未做真实清理、Polkit、Swap/ZRAM、Hyprland IPC 或主机 GUI 测试；真实 Windows 源图、DLU 实际像素换算和 2 px overlay 仍 BLOCKED / NOT RUN。P2B 未开始。

## 2026-10-25 — P2A-R6 隔离 GUI CI 复测完成
- 在 `test/issue-3-isolated-gui` 修正窗口关闭说明后提交 `e2d3c5e4481e9494472913f41636580369038cd8` 并推送；`git ls-remote` 与本地 HEAD 相同。PR #4 仍 OPEN / Draft，base 为 P1 分支 `fix/issue-1-zamak-build`。
- GitHub Actions 全部针对该 SHA 成功：GUI push `40191454446`、GUI pull_request `40191456330`、build push `40191454361`、build pull_request `40191456339`。Artifacts 已下载核验，包含测试截图、ROI 差异、X11 close/WM 状态、Weston 与客户端日志、版本及 SHA。
- 隔离 Xvfb/Openbox 的 XTEST Alt+F4 触发 GTK close handler；日志包含 `CI_WINDOW_CLOSE` 和 `CI_APP_RETURNED`，进程 exit 0，窗口随后消失。`xdotool windowclose` 会调用 `XDestroyWindow`，故不用于正常 WM close 测试。Hover-only→pressed ROI 与 pressed→release 像素差各为 8004。
- 保留既有 9 个 Zamak 测试函数，新增 3 项紧凑几何/双语检查；12/12 PASS。Weston Headless/Pixman 的 socket、Canvas draw、受控退出与 exit 0 PASS；每次记录一次未屏蔽的 `gdk_seat_get_keyboard` critical，且 Weston 未公布 `wl_seat`，因果未证实。
- 本机 `verify_project.py` 29 项、C 安全测试 3 项、shell syntax、Zamak 0.2.0 check/test/build、Arch build helper、diff check 全部 PASS。未启动主机 GUI/Hyprland、未触碰内存清理、Polkit、Swap/ZRAM。
- 正式源码规格独立分支仍以 P1 SHA `11b0b9444cd94f0026b855abe3849a06106426f2` 为 base；规格继续固定 Windows 3.5.2 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 与官方紧凑 DLU 尺寸。真实 Windows 截图、实测 DLU 像素换算、像素 overlay/2 px 验收仍 BLOCKED / NOT RUN；P2B 未开始。
