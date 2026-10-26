# 短期记忆 / Latest Handoff

## 最新核对：P2C-4 exact-head Actions 证据（2026-10-27）

- CI 验证的 PR #7 分支 head `dc1e112ba27bb1b89bcdd4b3c55ddf1465695e10`，Base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；head 只隔离 P2B/P2C concurrency group，未改生产代码、基准图片或阈值。以下文档/记忆同步提交发生在该 CI checkpoint 之后；其自身 Actions 状态以 PR #7 最新 Checks 为准。
- P2C run `40382203136`：Windows v3.5.2 来源、Linux production Xvfb/XTEST、视觉/暗色比较 3/3 PASS。Artifacts Windows `12679133143` (`sha256:1417e252b1d837c509670b73524ea9b9058eec0055104cfe5da478beb242e66a`)，Linux `12677879049` (`sha256:26fbc176837d7bce5fde49256f8d0f6744ddd235d1adfe13ea3a3e0972019805`)，comparison `12679860675` (`sha256:72d78940a992fae4c60efd0157bc4f7d9d9c4fcc702ed097b577e5e4ff5e6d8d`)。比较日志确认 558×335、10 screenshots、184/184 geometry、36/36 control raster、10/10 navigation selection、102/102 tree pixels、2/2 middle scroll；暗色图 10/10 配对，整页像素同一性 NOT PASS。比较 artifact 含逐图 SHA-256 manifest 及 Overlay/Difference/Heatmap。
- P2B run `40382203135` 首次 Xvfb APT install attempt 1 以 `124` 超时；只重跑失败 job 后 attempt 2，P2B Xvfb 和 Weston 两 job 均 PASS。首跑日志：Ubuntu 24.04.5 / image `20261012.334.1`，APT update 成功；`fonts-noto-cjk` 61.2 MB 下载出现 `Ign`/重试并达到 38 分钟硬时限。未见 DNS、APT 锁、缺包，底层网络原因未确认。attempt 2 同包随 101 MB 安装下载 4 秒完成；X11 artifact `12681997882` (`sha256:eb2ec95f941c81e179787a72dc6c1f9368ddd2fb7b6a75574b0c314cf285d5fd`)，Weston artifact `12681398424` (`sha256:e9a57f49ccf67ba376b703d6e0e4d1db548aaea2d1b872c251aa572e1966c801`)，APT diagnostics `12681383785` / `12680778845`。P2B 上重复设置页 step 按条件 SKIPPED；完整五页由同 head P2C 已执行。
- Build/governance/safety run `40382203131` PASS。本机非 GUI：Zamak `0.2.0` version/check/test (30/30)/build、`scripts/build-arch.sh`、治理 33 项、Python/C tests 3/3、`bash -n scripts/*.sh`、`git diff --check` 全 PASS。没有启动本机 GUI、触碰活动桌面/Hyprland、输入注入、执行清理、Polkit、Swap/ZRAM 或内核写入。

## P2C-4 字体/控件绘制/深色主题实施记录（2026-10-27）

- PR #7 仍 OPEN / Draft；Base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；P2C 生产代码/工作流 head `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。Windows v.3.5.2 tag SHA 固定 `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- P2C run `40373093546` Windows 来源、生产 Xvfb/XTEST 与比较 3/3 PASS；P2B run `40373092535` 的主窗 Xvfb/XTEST、Weston Headless/Pixman 两 job PASS；build/governance/safety PR run `40373092652` 与 push run `40373081011` PASS。P2B 重复设置页 step 按条件 skipped，完整设置页交互在同 head P2C run 已执行。
- Artifacts：Windows `12673463315` (`sha256:22eaadc99b82745dc548af7036bca0bafe7f0c0b38ae5956553d9976dcbfddf1`)，Linux Xvfb `12674390489` (`sha256:f0b49b1577dec13c436cb967c65793ed7388ab0030e1fddc644e7636cc64d2f2`)，视觉/字体/暗色比较 `12673073193` (`sha256:0897e9e15d2a5c8eff9616b6d1eef730fc55e2d454be28238bf39a66810388a3`)。SHA 清单：Windows PNG 41/41、Linux PNG 44/44、dark 70/70、visual 83/83 全匹配。
- 几何/局部 raster 回归：184/184 resource rectangles PASS（最大 1.5 px），nav 10/10、tree guide 102/102、discrete raster 36/36 PASS。暗色 10 对 Windows/Linux screenshot 全部生成比较素材；变化率 5.9434%–73.2932%，仅 `CAPTURE_AND_COMPARE_PASS`，全像素 1:1 仍 `NOT PASS`。
- 字体候选：Win32 `MS Shell Dlg 2` / 8.25 pt / weight 400 / ascent-descent 11/2；Linux Pango Sans 8 pt / DejaVu Sans + Noto Sans CJK SC / ascent-descent 10/3。隔离 Liberation Sans 对 English 64/66 度量改善、简中 12/54 改善/12/54 退化；双语全页变化率没有充分收益，生产字体保持不变，不打包字体。
- GDI/Pango ascent+descent 总和均为 13 px，但 ascent/descent 分配不同 1 px；Linux text origin 不是 GDI baseline 的直接等价测量。GDI 字符串宽度/平均字宽已记；当前证据没有独立且可对齐的 character-spacing/tracking 量，未添加字距补偿。
- XTEST 双语验证 File/View/Settings/Help、语言子菜单、键盘 Enter/方向/Esc、分隔区跳过、暗色 5 页、safe clean dropdown 均通过；清理项禁用。Windows tray、原生清理 popup、安全确认/结果对话框仍 BLOCKED。简中 12 项来源差异维持 DECISION_REQUIRED。
- 不操作本机 GUI/Hyprland；无内存清理、Polkit、root GUI、内核写入或 Swap/ZRAM 变化。日志中的 Weston GTK seat critical 未屏蔽、未推断根因。
- 文档/记忆首轮提交 `6d5f4d4b8212913ea12b59c20f0916b62219aeea` 的 P2C run 状态及早期 APT 超时记录转入历史快照；当前状态以本文件顶部 `dc1e112` exact-head 核验和 P2C-4 报告为准。

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

## 最近记录：P2B-2 原版主窗口结构与数据可靠性修复（2026-10-25）

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

日期：2026-10-25；任务：P1 Zamak 0.2.0 编译与原生构建验收。

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
