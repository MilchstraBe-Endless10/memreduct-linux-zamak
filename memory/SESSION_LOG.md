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

## 2026-10-25 — P2A-R6 源码规格 Draft PR
- 将正式 P2A 源码规格从独立分支 `docs/issue-3-p2a-ui-spec` 推送；基于 P1 `fix/issue-1-zamak-build` / `11b0b9444cd94f0026b855abe3849a06106426f2`，未从 PR #4 派生。创建关联 Issue #3 的 [Draft PR #5](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/5)，PR #4 测试代码没有混入。
- 文档列明官方 v.3.5.2 SHA、资源和双语文本、主窗口及五页资源尺寸、四个菜单、八项清理映射、托盘/事件、交互矩阵、Zamak 技术可行性和 Windows 原生截图采集检查清单。项目长期规则明确 UI 高保真不可重新设计/放大，并把 Windows API 等价性独立映射。
- PR #5 创建时 head `b94731b9a9bc52a4834358f9f71fcf86a2033925`；创建后继续提交了记忆与会话日志同步，因此最终 head 另有推进，以当前远端 `docs/issue-3-p2a-ui-spec` SHA 为准。Push 与 pull_request build-and-test 已触发。
- 源码规格 head `90d62100e840aa408e4ea99cbbd866ef2025e99e` 的 PR `40191901022` 与 push `40191896559` build-and-test 均 PASS；验证治理文件、shell syntax、C safety tests、Zamak 0.2.0 check/test/build。随后本次记忆同步提交只变更 Markdown；提交后需要再核对 PR head 与其新 Actions。
- 真实 Windows 3.5.2 截图、实际 DLU 像素换算、双边截图 overlay/diff 和 2 px 验收仍 BLOCKED / NOT RUN。该 PR 是源码级规格审阅交付，不是 P2A 像素验收或 P2B 实现批准。

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
