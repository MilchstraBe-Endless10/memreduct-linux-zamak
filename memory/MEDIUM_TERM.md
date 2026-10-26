# 中期记忆 / Roadmap State

更新时间：2026-10-26；仅已见到的证据，未经推测。

## 当前阶段
**P1：Zamak 编译验收** — 本机 Zamak 0.2.0 check/test/build、Arch 原生构建、Hyprland GUI 基础验收及 Draft PR Actions 均通过。PR #2 仍 OPEN / Draft，等待 ChatGPT 复审和用户合并决定。

**P2A：源码规格与隔离 UI 测试** — 源码规格位于独立 Draft PR #5；CI 测试位于 PR #4。PR #5 R7 修正 head `d89d74fde433da1863b4de0223f974c824fb590e`，checks 2/2 PASS；R6 几何说明、最终 CI 证据与候选实现门槛已修正。PR #4 最终 head `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1` 的 GUI PR run `40192531809`、build PR run `40192531948` 均通过，push runs 分别为 `40192528601`、`40192528611`；保留原 9 项事件路由测试并新增 3 项探针测试，12/12 PASS。P2C Windows-hosted run 后续取得 v3.5.2 主窗及五个设置页的双语 Native 截图和设置窗实测尺寸；完整菜单/托盘/清理对话框/深色主题截图、Hyprland 输入、全套几何对照和 2 px 验收仍 BLOCKED / NOT RUN。`IDD_MAIN=224×214 DLU` 是主窗模板；每个 `IDD_SETTINGS_* =260×168 DLU` 仅是设置子页模板，不是含导航和底部按钮的完整外框。

**P2B-1：生产主窗口候选** — 独立 Draft PR #6（base 为 PR #5 修正规格分支），代码 tested head `a6df90389a69fc9cad12272c2daa2f273c834385`。PR GUI run `40199653180`、build-and-test `40199653440` 和对应 push runs `40199649320` / `40199649284` 全通过。生产 Xvfb/XTEST 截图、菜单、双语、内存采样、close 与私有 Weston 生命周期已有证据；控件状态见 P2B-1 report。仍是集中参数化的未校准估算窗口，不是像素验收。

**P2B-2：主窗口结构和采样可靠性** — PR #6 已推送最终 Draft head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，base 为 PR #5 `d89d74fde433da1863b4de0223f974c824fb590e`。最新 ChatGPT 的 P2B-2 scoped review 认可三组纵向布局、菜单边界、采样失效和隔离 GUI 证据；Windows 原生截图/2px 和 Hyprland 输入仍未验收。

**P2C-1：五页设置界面** — Draft PR #7，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。生产源码测试 checkpoint `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`；完整 Actions 在文档 head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df` 全部 PASS（之后仅报告/记忆内容需再同步）。最近修复移除 libui-ng GTK 内容 holder 的 12 px border；Xvfb 实测设置 Canvas 558×335，与隔离 Windows v3.5.2 runner 实测 client area 558×335（Windows outer 574×374，96 DPI / 100%）一致。生产设置窗五页、English/简体中文、资源控件映射及禁用状态详见 `docs/P2C-1_SETTINGS_UI_REPORT.md`。PR GUI run `40269534094` 的 Xvfb/XTEST 与 Weston Headless/Pixman 作业均 PASS，Artifacts：X11 `12627279867`、Wayland `12627990285`；PR build run `40269534153` PASS；PR Windows provenance/native screenshot run `40269534205` PASS，artifact `12627309561`。Windows-hosted runner 采集主窗及五页×两种语言；release/tag/资产哈希、setup PGP 签名、binary version 均核验。本机对代码 checkpoint `d4f1ca9` 的 Zamak 0.2.0 `check/build`、22/22 test、33 项 project verify、3 项 C tests、Arch build、shell syntax 与 `git diff --check` PASS。Windows/Linux 截图客户区几何在指定环境匹配，但没有裁剪叠图和像素差异计算，1:1 / 2 px 仍 NOT RUN；深色主题、菜单/托盘/清理对话框仍未采集。设置持久化、托盘/SNI、自动清理、通知和特权功能未实现或禁用。

**P2C-2：真实截图校准（进行中）** — PR #7 最新源码 HEAD `20c8fbc8fc80c481965fecbde17aa507eb7247fe`。Windows native + production Xvfb + comparison run `40294897392` PASS；artifacts：Windows `12636924611`、Linux X11 `12638110426`、overlay/heatmap/geometry/SHA manifest `12637747957`。导航选中矩形 10/10 与 Windows 截图完全同位（所有偏差 0 px），树状点线 102/102 精确像素通过。通用几何 172 PASS（≤2 px），12 `NOT_COMPARABLE_NATIVE_STATE`，0 FAIL；页面/控件 raster 12/12 PASS。changed pixels 为 6.8528%–16.8042%，不代表像素等价。相同 HEAD 的生产 Xvfb/XTEST + private Weston/Pixman run `40294897349` PASS，artifacts `12638002197` / `12637667843`。build-and-test push run `40294892563` 与 PR run `40294897341` 均 PASS；报告与短期记忆已记录当前证据，没有本机 GUI/桌面操作或特权内存动作。

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
- Windows v3.5.2 主窗及五页双语 Native 截图已在隔离 Windows Server 2022 runner 取得；light-theme overlay/difference 与几何 2 px 比较已有 hosted evidence。全局像素等价仍未通过，字体/抗锯齿和原生控件视觉仍有差异；深色主题、托盘/菜单/清理对话框截图、Wayland 输入和未匹配的中间滚动状态仍缺。Weston 的 GTK seat critical / shutdown layer diagnostic 继续保留，不推断根因。
- 五页设置界面已实现导航和显示；设置持久化、托盘/SNI、Polkit 授权端到端、真实缓存清理、Swap/ZRAM 写入均未执行或实现。除语言、Reset 确认和 Close 外的设置控件保持禁用。

## 阶段表
| 阶段 | 交付 | 状态/依赖 |
|---|---|---|
| P0 | 正式文档、记忆、仓库、安全最小基线 | 完成；PUBLIC 仓库已创建，`main` SHA 已核验 |
| P1 | Zamak `check/test/build` 通过，Linux GUI 在目标桌面打开 | 本机、GUI 与 CI 验收通过；Draft PR 等待复审 |
| P2A | 固定 Windows 3.5.2 UI/UX 规格、交互矩阵、Linux 能力映射及隔离测试 | 源码规格 PR #5 OPEN / Draft，PR #4 隔离 X11/Wayland CI PASS。Windows 源图缺失，像素验收 BLOCKED |
| P2B | 主窗口 UI 高保真实现 | 首个待校准生产候选已在 Draft PR #6；待代码/截图复审；最终像素验收仍等真实 Windows 截图 |
| P2C-1 | 五页设置窗口、双语页面、Reset/Close 与生产 UI 隔离验收 | Draft PR #7；production Xvfb/XTEST、Weston lifecycle、Windows native baseline、Win32 resource geometry 和 overlays 均有隔离 CI 证据；设置 Canvas 与 Windows client 均 558×335；像素级 1:1 未通过 |
| P2C-2 | 基于 Windows 3.5.2 截图校准五页设置界面 | PR #7 同一功能分支 HEAD `20c8fbc8fc80c481965fecbde17aa507eb7247fe`；172 geometry、10/10 双语导航 selection bounds、102 树线像素、12/12 raster PASS；12 个中间滚动状态不可比较；像素差异 6.8528%–16.8042%，故仍不等价；PR / push build-and-test runs `40294897341` / `40294892563` PASS |
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
