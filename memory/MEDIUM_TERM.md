# 中期记忆 / Roadmap State

更新时间：2026-10-27；仅已见到的证据，未经推测。

## P2C-4 最终 exact-head CI 复核

PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；完整截图/比较 evidence head `4f84fb9a9e247a5987a11cd435e5da4b05f78a53`。P2C run `40425954672` Windows 原版 + 生产 Xvfb/XTEST + 比较 3/3 PASS；P2B run `40425954460` 生产 Xvfb/XTEST + Weston Headless/Pixman 2/2 PASS；PR build `40425954317`、push build `40425944842` PASS，PR checks 7/7。Artifacts、digests、PNG/source SHA 与逐项指标见 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。生产 UI 源码 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；本轮未改生产字体、字号、控件绘制或视觉阈值。

184/184 resource geometry（最大 1.5 px）、nav 10/10、tree 102/102、raster 36/36、scroll 2/2 PASS；light changed-pixel 5.0741%–14.8853%，dark 5.9434%–73.2932%，整页 1:1 NOT PASS。深色图像是在 GTK `Adwaita:light` runner 上切换应用 Canvas palette 的结果，不代表 GTK 桌面原生暗色主题。132 行文字度量记录中，120 个同源项目有 9 项宽高差≤2 px、111 项仍有差异；12 项来源/语言差异仍 `DECISION_REQUIRED`。隔离 Liberation Sans 候选未进生产：English 64/66 改善、1 退化、1 不变；简中 12/54 改善、12 退化、30 不变。

本 head 的 APT update/install 均退出码 0：P2C Xvfb 101 MB / 13 秒，P2B Xvfb 101 MB / 7 秒，Weston 79.6 MB / 25 秒，Pillow 503 kB / 小于 1 秒。早前慢速下载的根因仍未确认。Weston 主窗/设置窗客户端均退出 0；保留 `gdk_seat_get_keyboard` critical、未公告 `wl_seat` 与 SIGTERM teardown `BUG: finalizing a layer with views still on it.` 诊断，客户端生命周期 PASS 不等于 compositor clean shutdown。P2B 重复五页步骤条件跳过，由同 head P2C 覆盖。Windows tray、缺安全采集路径的清理弹窗/确认结果仍 BLOCKED；Wayland 输入 NOT RUN。没有真实清理、特权操作或活动桌面输入。

### 前一个完整截图/对照 artifact snapshot（105d71d）

2026-10-27 的前一完整截图/对照审计为 head `105d71d567d7ba096e8b928c072ceb4af15bfe6f`，P2C/P2B/build/push runs 为 `40416882722`、`40416882708`、`40416882739`、`40416873128`。此条目保留其逐文件审计历史；最新 exact-head 结果以上一节为准。

## 当前阶段

本次 P2C-4 evidence audit 时间：2026-10-27；测试对应提交 `748a61e62c597793e0acedb3f83bccb12f950285` 的 P2C/P2B/build/push runs 均成功。产品源码 checkpoint 未变；随后仅同步报告、交互矩阵和记忆。后续文档 head 仍须在 PR 页面核对各自 checks，不能把此 evidence SHA 冒称为后续 head。
**P1：Zamak 编译验收** — 本机 Zamak 0.2.0 check/test/build、Arch 原生构建、Hyprland GUI 基础验收及 Draft PR Actions 均通过。PR #2 仍 OPEN / Draft，等待 ChatGPT 复审和用户合并决定。

**P2A：源码规格与隔离 UI 测试** — 源码规格位于独立 Draft PR #5；CI 测试位于 PR #4。PR #5 R7 修正 head `d89d74fde433da1863b4de0223f974c824fb590e`，checks 2/2 PASS；R6 几何说明、最终 CI 证据与候选实现门槛已修正。PR #4 最终 head `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1` 的 GUI PR run `40192531809`、build PR run `40192531948` 均通过，push runs 分别为 `40192528601`、`40192528611`；保留原 9 项事件路由测试并新增 3 项探针测试，12/12 PASS。P2C Windows-hosted run 后续取得 v3.5.2 主窗及五个设置页的双语 Native 截图和设置窗实测尺寸；完整菜单/托盘/清理对话框/深色主题截图、Hyprland 输入、全套几何对照和 2 px 验收仍 BLOCKED / NOT RUN。`IDD_MAIN=224×214 DLU` 是主窗模板；每个 `IDD_SETTINGS_* =260×168 DLU` 仅是设置子页模板，不是含导航和底部按钮的完整外框。

**P2B-1：生产主窗口候选** — 独立 Draft PR #6（base 为 PR #5 修正规格分支），代码 tested head `a6df90389a69fc9cad12272c2daa2f273c834385`。PR GUI run `40199653180`、build-and-test `40199653440` 和对应 push runs `40199649320` / `40199649284` 全通过。生产 Xvfb/XTEST 截图、菜单、双语、内存采样、close 与私有 Weston 生命周期已有证据；控件状态见 P2B-1 report。仍是集中参数化的未校准估算窗口，不是像素验收。

**P2B-2：主窗口结构和采样可靠性** — PR #6 已推送最终 Draft head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，base 为 PR #5 `d89d74fde433da1863b4de0223f974c824fb590e`。最新 ChatGPT 的 P2B-2 scoped review 认可三组纵向布局、菜单边界、采样失效和隔离 GUI 证据；Windows 原生截图/2px 和 Hyprland 输入仍未验收。

**P2C-1：五页设置界面** — Draft PR #7，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。生产源码测试 checkpoint `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`；完整 Actions 在文档 head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df` 全部 PASS（之后仅报告/记忆内容需再同步）。最近修复移除 libui-ng GTK 内容 holder 的 12 px border；Xvfb 实测设置 Canvas 558×335，与隔离 Windows v3.5.2 runner 实测 client area 558×335（Windows outer 574×374，96 DPI / 100%）一致。生产设置窗五页、English/简体中文、资源控件映射及禁用状态详见 `docs/P2C-1_SETTINGS_UI_REPORT.md`。PR GUI run `40269534094` 的 Xvfb/XTEST 与 Weston Headless/Pixman 作业均 PASS，Artifacts：X11 `12627279867`、Wayland `12627990285`；PR build run `40269534153` PASS；PR Windows provenance/native screenshot run `40269534205` PASS，artifact `12627309561`。Windows-hosted runner 采集主窗及五页×两种语言；release/tag/资产哈希、setup PGP 签名、binary version 均核验。本机对代码 checkpoint `d4f1ca9` 的 Zamak 0.2.0 `check/build`、22/22 test、33 项 project verify、3 项 C tests、Arch build、shell syntax 与 `git diff --check` PASS。Windows/Linux 截图客户区几何在指定环境匹配，但没有裁剪叠图和像素差异计算，1:1 / 2 px 仍 NOT RUN；深色主题、菜单/托盘/清理对话框仍未采集。设置持久化、托盘/SNI、自动清理、通知和特权功能未实现或禁用。

**P2C-3：原版视觉差异收敛（进行中）** — PR #7 OPEN/Draft，Base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。本轮生产源码 SHA `60be390eb0ff6c18564f35abcd23d36406c6d3f8` 已由 exact-source CI 验证；本段为报告/记忆同步，不改生产源码。Windows native + production Xvfb + comparison run `40319586672` 全部 PASS；artifacts Windows `12648157361`、X11 `12648453073`、comparison `12648552362`。184/184 几何 ≤2 px PASS（最大分量 1.5 px），包括此前缺失的 12 个中间滚动行；导航 selection 10/10、树状点线 102/102、raster 样本 36/36 PASS。五页双语较 P2C-2 的像素差异率 10/10 改善，当前 5.0420%–14.8853%，仍非整体像素等价。Windows 热键采集修正后实际得到 Ctrl+F1。

字体：Windows LOGFONT `MS Shell Dlg 2` alias、注册表替代 `Tahoma`、8.25 pt、13 px 高、ascent/descent 11/2；不能证明实际字体文件。Linux 8 pt Sans → DejaVu Sans / Noto Sans CJK SC，ascent/descent 10/3；English 文本宽度比中位数 1.1469。12 项简中来源差异待产品决策。Production GUI lifecycle run `40319586679` 的 Weston PASS；Xvfb 最终状态记于短期记忆。暗色 Linux 对照、托盘/清理子菜单和清理对话框仍缺；不操作活动桌面或特权接口。

**P2C-3R1：Actions APT 稳定性补强（2026-10-26）** — CI-only commit `406491b70594d202f3d2bb4efa667f2e0886c552` 上 P2C run `40336249517`、P2B GUI run `40336249521`、PR build `40336249519`、push build `40336242138` 全部 PASS；P2C 3/3 job、P2B GUI 2/2、PR checks 7 PASS / 0 FAIL。P2C artifact：Windows `12657162841`、Linux X11 `12656813101`、comparison `12656279296`；APT artifacts `12656977979` / `12656638843`。生产 GUI source checkpoint 仍 `60be390eb0ff6c18564f35abcd23d36406c6d3f8`，未更改。

R3 中 Xvfb bulk APT 依赖在 Ubuntu Azure 镜像下载 108 MB 用时 31:18（57.7 kB/s）；未观察到 DNS、包锁或包缺失，attempt 1 被取消、attempt 2 最终通过。新增有硬时限、网络 timeout/retry、完整输出/退出码、runner/APT 源/版本记录和故障分类的脚本；P2C、P2B GUI 与 Pillow compare 共用。此次新脚本的依赖安装耗时 6–27 秒，诊断 artifacts 均有完整状态。P2B workflow 去掉 P2C branch push trigger；P2B main-window XTEST/Weston 继续通过，重复的设置页 XTEST 在 P2B run 中跳过，由同 head P2C Xvfb 完整覆盖。运行中的证据任务不取消。

本轮仍有观察到的风险：`build-and-test` 的旧式 `Install system tools` 在 push 和 PR run 分别用时 7:42 / 8:43（均通过），该 job 上限 15 分钟且本轮没有改 `test.yml`。UI 几何/raster 阈值与 184/184、10/10、102/102、36/36 样本保持；1:1 NOT PASS；12 项简中来源差异仍 `DECISION_REQUIRED`。

**P2C-4：字体/控件栅格/深色主题与菜单证据（本轮范围完成，2026-10-27）** — PR #7 OPEN / Draft，Base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，本轮生产源码/工作流 HEAD `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。P2C Windows native + Linux Xvfb/XTEST + comparator run `40373093546` 三个 job PASS；P2B Xvfb 与 Weston Headless/Pixman run `40373092535` 两个 job PASS；PR build/governance/safety `40373092652` 与 push build `40373081011` PASS。Artifacts：Windows `12673463315`、Linux `12674390489`、comparison `12673073193`；P2B X11 `12674265667`、Weston `12673517780`。

暗色 10 组双语页面在固定 558×335 客户区全部完成采集、Overlay/Difference/Heatmap 对照；全页 changed pixels 为 5.9434%–73.2932%，全部 `PIXEL_IDENTITY_NOT_PASS`。Win32 控件几何 184/184、导航 10/10、树线 102/102、栅格样本 36/36 PASS，最大几何偏差 1.5 px；这些局部 PASS 不等于全页一致。GTK runner 是 Adwaita light，暗色由 app Canvas palette 单独切换。Windows 原生字体仍是未解析物理文件的 `MS Shell Dlg 2` alias；Linux 默认 DejaVu Sans + Noto Sans CJK SC。Liberation Sans 仅隔离候选，英文 64/66 样本改善、中文 12/54 改善且 12/54 退化，全页英文改善小、中文基本不变；未更改生产字体/字号，无字体文件进入仓库。Linux 四菜单双语和安全清理 dropdown 通过 XTEST；Windows tray/安全清理 popup/确认结果对话框仍 BLOCKED。12 项简中来源差异保持 `DECISION_REQUIRED`；整体 1:1 NOT PASS。完整指标及 artifact SHA 在 P2C-4 报告。

**P2C-4 CI checkpoint 复核（2026-10-27）** — P2C/P2B 验收 run 对应分支提交 `dc1e112ba27bb1b89bcdd4b3c55ddf1465695e10`；后续文档/记忆同步不改生产代码和视觉阈值。P2C run `40382203136` 3/3 PASS，artifacts Windows `12679133143`、Linux `12677879049`、比较 `12679860675`；几何 184/184、raster 36/36、导航 10/10、树线 102/102、中段滚动 2/2 均 PASS，完整像素 1:1 仍 NOT PASS。P2B run `40382203135` 首次 Xvfb APT attempt 1 在下载 `fonts-noto-cjk` 61.2 MB 时超时 `124`；无 DNS/锁/缺包证据，网络/镜像原因未确认。仅重跑失败 job 后 attempt 2 的 P2B Xvfb 与 Weston 均 PASS，artifacts `12681997882` / `12681398424`。P2B 重复的设置页步骤按条件 SKIPPED，由同 head P2C run 覆盖。Build/governance/safety run `40382203131` PASS；APT 诊断、artifact digest 和 SHA manifest 见 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。

下方保留的旧 P2C-4 条目记录早期 APT 超时阶段，是历史快照；不代表当前状态。

### P2C-4 早期进度快照（历史记录；由上方完成状态覆盖）

旧 HEAD `eea82a8c2cafb16b25acf3d319e084335548356d` 上的 Windows 截图成功，但当时的 P2C Linux APT 安装超时、P2B Weston 失败。超时日志显示 `fonts-noto-cjk` 包未完成下载，根因未确认。后续工作流修复及 `6502342` exact-head CI 已完成；旧快照保留供问题追溯。

**17:44 UTC 状态更正：** P2C run `40363399854` 已结束 FAILURE，Xvfb APT install exit `124`（38 分钟超时），比较 job SKIPPED。APT log 指向尚未完成的 `fonts-noto-cjk` 61.2 MB 下载，底层网络/镜像根因未确认；诊断 artifact `12672670397`。P2B run `40363344884` 在当时仍处于依赖安装，不取消且不提前判定。共享 PR concurrency group 的 workflow 修正尚未提交/验证。

**17:50 UTC 更新：** P2B run `40363344884` 的隔离生产 Xvfb/XTEST job 已 PASS（APT 约 35 分钟）；设置页重复 step按条件 SKIPPED。Weston job 仍处于 APT 安装中，无 Wayland 结论。以上测试是推送头 `eea82a8...`，不含本地暗色调色板改动。

**17:52 UTC 终态：** P2B run `40363344884` 整体 FAILURE：Xvfb/XTEST job PASS；Weston job APT 38 分钟超时。APT artifacts `12673795414`（Xvfb，exit 0）与 `12672539453`（Weston，exit 124）；两边均涉及 `fonts-noto-cjk` 61.2 MB package，Xvfb 最终在约 35 分钟后完成，Wayland 未完成。并发造成慢速的因果关系未证实。工作树已从英文生命周期 Wayland job 去掉该 CJK 字体依赖，并将 P2B/P2C 同 PR workflow concurrency key 合并（`cancel-in-progress:false`）；新 head CI 待跑。

Windows isolated runner 为 Server 2022 build 20348、1024×768、96 DPI/100%；原版设置窗外框 574×374、客户区 558×335。132 项 GDI 文本记录均为逻辑 face `MS Shell Dlg 2` / 8.25pt / weight 400，GDI ascent/descent 中位数 11/2px；注册表映射 Tahoma 不等于已证实实际字体文件。Windows artifact 的 41/41 PNG SHA 已匹配 result 清单。四个英文主菜单截图及五页双语 light/dark 已采集；托盘、清理弹出菜单和必须触发清理才出现的确认/结果窗仍 BLOCKED。Liberation Fonts 只在 CI 候选进程评估，不打包字体、不改生产默认；12 项中文来源差异继续 `DECISION_REQUIRED`。

本机非 GUI 验证：Zamak 0.2.0 check/test/build PASS，30/30 Zamak tests；33 项治理检查、C/Python safety 3/3、Shell syntax、`scripts/build-arch.sh`、`git diff --check` PASS。无本机 GUI/Xvfb/Wayland/Hyprland 交互。生产视觉改善、暗色 10 组 Linux 配对、184 项/栅格回归及最终 exact-head CI 待作业完成；全像素 1:1 仍不可宣称。
**P2C-2：真实截图校准（已完成该轮证据，未通过整体像素等价）** — PR #7 源码 HEAD `20c8fbc8fc80c481965fecbde17aa507eb7247fe`。Windows native + production Xvfb + comparison run `40294897392` PASS；artifacts：Windows `12636924611`、Linux X11 `12638110426`、overlay/heatmap/geometry/SHA manifest `12637747957`。导航选中矩形 10/10 与 Windows 截图完全同位（所有偏差 0 px），树状点线 102/102 精确像素通过。通用几何 172 PASS（≤2 px），12 `NOT_COMPARABLE_NATIVE_STATE`，0 FAIL；页面/控件 raster 12/12 PASS。changed pixels 为 6.8528%–16.8042%，不代表像素等价。相同 HEAD 的生产 Xvfb/XTEST + private Weston/Pixman run `40294897349` PASS，artifacts `12638002197` / `12637667843`。build-and-test push run `40294892563` 与 PR run `40294897341` 均 PASS；本机无 GUI/桌面操作或特权内存动作。

### P2C-2 历史验收状态（2026-10-26，source head `1d1d6562cb27ddb63fb38a2bf71756b5e88d63ee`）

PR #7 的 Xvfb/XTEST + Weston run `40284909745` 两项通过，Windows native baseline `40284909821` 的 Windows job 通过，build/test `40284909782` 通过。对应 artifacts：X11 `12633129180`、Wayland `12633864035`、Windows `12633108874`。Windows/Linux artifacts 的本地对照由 bundled Pillow 12.3.0 运行：172 geometry PASS、10 control-raster PASS、8 source swatch PASS；12 个中间滚动行标记 `NOT_COMPARABLE_NATIVE_STATE`。像素 changed range 7.9548%–18.6990%，因此 1:1 仍未通过。Hosted PR run `40284909821` 与 push duplicate `40284904057` 的 Xvfb 作业均在 25 分钟 timeout 后取消，比较 job 被跳过；workflow 已改为只由 PR 事件触发，并将 Xvfb / comparison timeout 调至 45 / 20 分钟，待新 head Actions 验证。截图 SHA、Win32 控件映射和 GTK 日志在设置报告。本机 Zamak 28/28、33 项治理检查、C safety 3/3、Shell、Arch build 与 diff 检查通过；未启动本机 GUI或操作活动桌面。

## 已具备的代码
- `src/main.zm`: Zamak Canvas 生产主窗口候选、四菜单、双语、禁用清理状态和真实 `/proc/meminfo` 定时采样；不等于完整功能集。
- `src/ui_geometry.zm`: 集中管理从官方 DLU 模板换算的候选尺寸参数，等待 Windows 字体/DPI 校准。
- `src/memory.zm`: `/proc/meminfo` 读取解析。
- `src/policy.zm`: 阈值与 30 分钟冷却原型。
- `native/async_privileged.c`: 异步 pkexec 调用桥。
- `native/memreduct_helper.c`: 三种特权缓存模式白名单。
- C 安全测试 3 项在本轮本机和 GitHub CI 均通过。

## 当前阻塞
- 等待 ChatGPT 复审 PR #6/#7 的生产主窗和五页设置 UI、截图与诊断；PR #7 保持 OPEN / Draft，不合并。
- Windows v3.5.2 主窗及五页双语 light/dark Native 截图已在隔离 Windows Server 2022 runner 取得；Linux 五页双语 light/dark Xvfb 截图、菜单与无副作用清理 dropdown 也有 hosted evidence。184 项资源几何、滚动、导航和控件栅格样本已通过；整页像素等价仍未通过。Windows tray 与不能无副作用采集的清理弹窗/确认/结果仍 BLOCKED；Wayland 只有隔离客户端生命周期，真实 Wayland 输入 NOT RUN。Weston 的 GTK seat critical 和 SIGTERM teardown layer diagnostic 保留，因果未证实。
- 五页设置界面已实现导航和显示；设置持久化、托盘/SNI、Polkit 授权端到端、真实缓存清理、Swap/ZRAM 写入均未执行或实现。除语言、Reset 确认和 Close 外的设置控件保持禁用。

## 阶段表
| 阶段 | 交付 | 状态/依赖 |
|---|---|---|
| P0 | 正式文档、记忆、仓库、安全最小基线 | 完成；PUBLIC 仓库已创建，`main` SHA 已核验 |
| P1 | Zamak `check/test/build` 通过，Linux GUI 在目标桌面打开 | 本机、GUI 与 CI 验收通过；Draft PR 等待复审 |
| P2A | 固定 Windows 3.5.2 UI/UX 规格、交互矩阵、Linux 能力映射及隔离测试 | 源码规格 PR #5 OPEN / Draft，PR #4 隔离 X11/Wayland CI PASS。Windows 源图缺失，像素验收 BLOCKED |
| P2B | 主窗口 UI 高保真实现 | 首个待校准生产候选已在 Draft PR #6；待代码/截图复审；最终像素验收仍等真实 Windows 截图 |
| P2C-1 | 五页设置窗口、双语页面、Reset/Close 与生产 UI 隔离验收 | Draft PR #7；production Xvfb/XTEST、Weston lifecycle、Windows native baseline、Win32 resource geometry 和 overlays 均有隔离 CI 证据；设置 Canvas 与 Windows client 均 558×335；像素级 1:1 未通过 |
| P2C-2 | 基于 Windows 3.5.2 截图校准五页设置界面 | PR #7 同一功能分支 HEAD `20c8fbc8fc80c481965fecbde17aa507eb7247fe`；172 geometry、10/10 双语导航 selection bounds、102 树线像素、12/12 raster PASS；当时 12 个中间滚动状态不可比较；像素差异 6.8528%–16.8042%，故仍不等价；PR / push build-and-test runs `40294897341` / `40294892563` PASS |
| P2C-3 | 字体/控件光栅/中滚动态及来源语言差异收敛 | 最终文档 head `7f1f75a` 下 PR runs `40323243037`、`40323243043`、`40323243029` 全部 PASS；当前 artifacts `12650430810` / `12650680581` / `12649498772`。源码 `60be390` 对比 184 geometry、导航 10、树点线 102、raster 样本 36 均 PASS；10/10 全页率改善但 1:1 NOT PASS；12 项语言差异待产品决策。独立 push 旧 run `40319576201` 启动前 FAIL（具体原因无证据）；后续 PR Xvfb 均 PASS。暗色 Linux/托盘/清理对话框未覆盖 |
| P2 | 可信指标显示/日志/只读探测，安全反馈 | 未开始；依赖 P1 与阶段审查 |
| P3 | 手动一键缓存清理验收、特权隔离与回滚测试 | 未开始；依赖 P2 |
| P4 | 有条件的自动策略、配置持久化、冷却与可观察性 | 未开始；依赖 P3 安全评审 |
| P5 | ZRAM 只读分析和可选管理，避免同现有 Swap 冲突 | 未开始；需 ADR 和用户批准 |
| P6 | Arch PKGBUILD/Release、真实场景对比基准、公开测试版 | 未开始；依赖各前置条件 |

## 决策和后续
- ADR-0001: 采用 Zamak 原生架构。
- ADR-0002: 独立最小权限 helper + 手动缓存回收优先。
- ADR-0003: GitHub + 三层记忆 + 双 AI 审核制。
- 下一行动：完成 Windows 原版截图 runner 的实际结果记录，并将 PR #7 的测试报告/记忆同步提交；交给 ChatGPT 复审 PR #7 与 PR #6。PR #2/#4/#5/#6/#7 均不由 Codex 合并。真实 Windows 像素验收仍未完成，候选不得标为已校准或 1:1 通过。
