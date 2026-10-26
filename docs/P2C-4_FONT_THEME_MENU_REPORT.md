# P2C-4 — 字体、控件绘制与深色主题校准

状态：P2C-4 字体候选、控件栅格、深色主题和安全菜单证据已完成；PR #7 仍 OPEN / Draft，不合并。最近核对的精确 PR head 为 `37cefdb351b1cfcdacbc6ca24eb8b06fead93cc1`。相对源码 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`，分支只追加 CI 并发组调整和文档/记忆同步；生产 UI、Windows goldens、依赖和验收阈值未改。该 exact-head CI 已全部结束且通过；本报告继续明确列出全页像素差异、来源语言决策和 Weston 关闭期诊断，不将局部 PASS 扩大解释为 1:1 验收。

## 最终 exact-head 验证 `37cefdb`（2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 状态 OPEN / Draft；base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；head `37cefdb351b1cfcdacbc6ca24eb8b06fead93cc1`。生产源码 checkpoint 为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；之后仅调整 P2B/P2C workflow 并发组并同步报告/记忆，未修改 UI、goldens、依赖或验收阈值。检查时没有比 P2C-3R1（head `2c745710210c8794b52a0e9eb19dd1830c00d8d6`）更新的 ChatGPT review；该 review 接受 R1 CI 门槛，但仍明确整体 1:1 UI 为 NOT PASS。
- P2C [Windows 原版 + Linux 生产 Xvfb/XTEST + comparator run `40390082314`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314)：3/3 jobs SUCCESS，Windows 来源/原生截图、Linux 五页中英文及菜单截图、视觉比较步骤均执行。P2B [生产 Xvfb/XTEST + Weston lifecycle run `40390082510`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082510)：2/2 jobs SUCCESS；P2B 中重复的五页设置场景按条件 SKIPPED，同 head 的五页完整验证由 P2C Xvfb job 实际覆盖。PR build/governance/safety [`40390082353`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082353) 与 push build [`40390075275`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390075275) 均 SUCCESS。
- Artifact digest：Windows [`12682119919`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314/artifacts/12682119919) `sha256:9a0463631da7bf4e049f5fe319afc9496f16b0fd2b940fae85363f6c2f845acc`；Linux production X11 [`12684755661`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314/artifacts/12684755661) `sha256:9bfb89a4bb10da11da9b42bc865627e36bc50c1e654ee3212ee14c7086ba0af2`；Windows/Linux visual comparison [`12683006399`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314/artifacts/12683006399) `sha256:8b765ec896abad794b91cbe80194c3959dc98a9c4fabb83cf2ffccdc276437b2`；P2C APT diagnostics `12684970524` / `12683476105`。P2B artifacts：X11 [`12682830560`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082510/artifacts/12682830560) `sha256:d5e4967f92c7feb51defd5623b08b0135abed00ace0a2d17d5a995b22e901fc4`；Wayland [`12682645763`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082510/artifacts/12682645763) `sha256:1c68e2e22f6945514b14efb065da7001b3633d3266795895198c2846b5d86524`；其 APT diagnostics 为 `12682755594` / `12683000436`。
- 对实际下载的 exact-head artifacts 校验：visual `visual-sha256.csv` **83/83**、dark-theme SHA 清单 **70/70**、Linux production screenshot 清单 **44/44** 全部与 PNG 实际字节匹配；Linux source 清单 **10/10** 与源码工作树匹配，主程序/helper 二进制 **2/2** 匹配。主程序 SHA-256 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。完整截图 SHA 与截图本体保存在上述 artifact，不混用其他 run 的文件。
- 最新轻色五页双语 changed-pixel 百分比（Windows 原生客户区与 Linux production Canvas 均 558×335，未缩放）：

| 设置页 | English | 简体中文 |
|---|---:|---:|
| General | 12.0184% | 10.6617% |
| Memory cleaning | 14.8853% | 14.7392% |
| Appearance | 11.5123% | 10.8966% |
| Tray icon | 14.7900% | 13.9287% |
| Advanced | 5.5871% | 5.0741% |

  五页 × 双语的暗色配对截图、并排图、overlay、difference、heatmap 和逐图哈希也都在 visual artifact 的 `dark-theme/` 目录；暗色全页差异率沿用报告下一节列出的数值。几何 **184/184** PASS（最大单项偏差 `1.5 px`）、导航 `10/10`、树线采样 `102/102`、离散控件栅格 `36/36`、中间滚动状态 `2/2` PASS；这些局部测试不等价于像素相等。
- Weston 在独立 Headless/Pixman 下连接、绘制；测试客户端退出码 `0`，主窗与设置窗均释放。输入设备验收 NOT RUN。关闭客户端后由脚本对 Weston 发送 SIGTERM，`weston.log` 出现 `caught signal 15` 和 `BUG: finalizing a layer with views still on it.`；尚无证据确定根因，因此本报告将其作为 compositor 关闭期诊断保留，不能解释为 clean compositor shutdown，也没有据此归咎应用。
- Linux 环境：GitHub-hosted Ubuntu 24.04、Xvfb `1024×768×24`、GTK `3.24.41`、96 DPI/scale 1、`Adwaita:light`；Linux 暗色由应用 Canvas palette 切换。Windows：Server 2022 build 20348、1024×768、96 DPI/100% scale。原版仍固定 v.3.5.2 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 整体 Windows/GTK 像素同一性仍 **NOT PASS**；Windows 专有字体没有打包；12 项上游简中显示差异仍 `DECISION_REQUIRED`。Windows tray、原生清理弹出菜单及真实清理确认/结果弹窗仍 BLOCKED；未执行清理、Polkit、Swap/ZRAM、root GUI 或活动 Hyprland 操作。

## 前一 CI checkpoint `dc1e112`（2026-10-27，历史记录）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 仍 OPEN / Draft，Base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。以下 exact-head CI 对应分支提交 `dc1e112ba27bb1b89bcdd4b3c55ddf1465695e10`；该提交只更新两个 GUI workflow 的 concurrency group，生产源码、Windows 基准、比较阈值及测试数量均未改变。后续报告/记忆同步不改产品代码。
- P2C run [40382203136](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40382203136) 在此 head 上 **3/3 jobs PASS**：Windows 3.5.2 来源与截图、Linux 生产设置 Xvfb/XTEST、几何/视觉/暗色比较均执行成功。Artifacts：Windows `12679133143` (`sha256:1417e252b1d837c509670b73524ea9b9058eec0055104cfe5da478beb242e66a`)、Linux Xvfb `12677879049` (`sha256:26fbc176837d7bce5fde49256f8d0f6744ddd235d1adfe13ea3a3e0972019805`)、比较 `12679860675` (`sha256:72d78940a992fae4c60efd0157bc4f7d9d9c4fcc702ed097b577e5e4ff5e6d8d`)。比较 job 报告 Windows crop 与 Linux Canvas 均为 `558×335`、截图 10/10；控件矩形 184/184、离散栅格 36/36、导航选中框 10/10、树点线像素 102/102、中间滚动 2/2 均 PASS。暗色/亮色几何一致；10 组暗色截图生成配对证据，整体像素同一性明确 `NOT PASS`。比较 artifact 包含逐图 SHA-256 清单与 Overlay/Difference/Heatmap。
- P2B run [40382203135](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40382203135) 首次 attempt 的 Xvfb APT 安装失败；仅重跑失败 job 后，attempt 2 **P2B Xvfb 与 Weston 两个 job 均 PASS**。Xvfb artifact `12681997882` (`sha256:eb2ec95f941c81e179787a72dc6c1f9368ddd2fb7b6a75574b0c314cf285d5fd`)，Wayland artifact `12681398424` (`sha256:e9a57f49ccf67ba376b703d6e0e4d1db548aaea2d1b872c251aa572e1966c801`)，APT attempt 1/2 artifacts `12681383785` / `12680778845`。PR #7 上重复的设置页步骤按条件 `SKIPPED`；同 head 的 P2C run 已实际执行完整五页双语测试。
- attempt 1 APT 日志：Ubuntu 24.04.5 runner image `20261012.334.1`；`apt-get update` 成功、抓取 12.6 MB；安装 `fonts-noto-cjk` 61.2 MB 包时出现 `Ign`/重试，38 分钟时限到达并返回 `124`。未见 DNS、APT 锁或包不存在证据。可确认故障为包下载超时，底层网络/镜像/runner 原因未证实。attempt 2 同一字体包随 101 MB 总依赖下载在 4 秒内完成，APT 退出 `0`，XTEST 包括中文切换与 WM close（退出码 `0`）通过。该重试只证明本次恢复成功，不证明长期网络稳定。
- Build/governance/safety run [40382203131](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40382203131) PASS。本机非 GUI 回归：Zamak `0.2.0`、check、test（30/30）、build、Arch build、治理 33 项、Python/C 安全测试 3/3、Shell 语法、`git diff --check` 均 PASS。没有启动本机 GUI、操作 Hyprland/桌面输入、清理内存、调用 Polkit、修改 Swap/ZRAM 或写内核参数。

## P2C-4 生产源码 checkpoint 证据（2026-10-27 核对）

- PR：[#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7)，分支 `feat/issue-3-p2c-settings-ui` → `feat/issue-3-p2b-main-window`；Base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；本轮生产源码与工作流 HEAD `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。固定 Windows 基准 v.3.5.2 / `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- P2C [Windows native + Linux Xvfb/XTEST + comparison run 40373093546](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546)：三个 job 全部 SUCCESS，且必需步骤均执行。Artifacts：Windows [`12673463315`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546/artifacts/12673463315)，digest `sha256:22eaadc99b82745dc548af7036bca0bafe7f0c0b38ae5956553d9976dcbfddf1`；Linux 生产 X11 [`12674390489`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546/artifacts/12674390489)，digest `sha256:f0b49b1577dec13c436cb967c65793ed7388ab0030e1fddc644e7636cc64d2f2`；几何/视觉/暗色/字体比较 [`12673073193`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546/artifacts/12673073193)，digest `sha256:0897e9e15d2a5c8eff9616b6d1eef730fc55e2d454be28238bf39a66810388a3`。P2C Xvfb APT diagnostics artifact `12674160933`，Pillow comparison diagnostics `12672174873`。
- P2B [生产窗口 Xvfb/XTEST + Weston Headless/Pixman run 40373092535](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373092535) 两个 job 均 PASS；artifacts：X11 `12674265667`、Wayland `12673517780`。该 run 中因 P2C workflow 已覆盖设置页，重复的设置页 X11 step 按条件 SKIPPED；完整五页双语测试实际在同 HEAD 的 P2C run 执行并 PASS。PR build/governance/safety [run 40373092652](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373092652) 与 push build [run 40373081011](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373081011) 均 SUCCESS。
- 当前 P2C Linux 证据核验：Windows 截图清单 **41/41**、Linux PNG 清单 **44/44**、暗色比较 SHA 清单 **70/70**、视觉比较 SHA 清单 **83/83** 均匹配。Linux 设置客户区保持 `558×335`；截图未经缩放或拉伸。Windows 环境为 Windows Server 2022 build 20348，1024×768，96 DPI / 100% scale；Linux 为 Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、96 DPI、scale 1、GTK theme `Adwaita:light`。Linux 暗色状态由应用 View 菜单切换 Canvas palette，不修改 GTK 或桌面主题。
- 当前 build artifact 记录源码 SHA：`src/main.zm` `6d8408d46360757616f85609c85acf02534b0090a679d74bd4ed5c6175c34d8f`；`src/settings_ui.zm` `f821f36117ab5ba0ec757c7596d991dc980d8d805aa1b804952c3f9d2a17c920`；`src/ui_geometry.zm` `a2fd4af1de7b414d97dbe7c871d4048cc7dfab1095499f3998a0916eaba6e087`；`native/gui_window.c` `63c313bed8205e14266148b262064900dacf838e03c1aa0134dc2facff6f6326`。同 SHA 构建的生产二进制 SHA-256 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。

## 字体度量与隔离候选结论

- Windows 原生控件 `WM_GETFONT`/GDI 报告逻辑 face alias `MS Shell Dlg 2`、8.25 pt、weight 400，典型 ascent/descent `11/2 px`。注册表 substitution 显示 Tahoma，但没有证据证明 GDI 实际加载的物理字体文件。Linux 默认 Pango `Sans` 8 pt / weight 400，英文字形匹配 DejaVu Sans、简中匹配 Noto Sans CJK SC，ascent/descent `10/3 px`。两边 ascent+descent 都是 13 px，但基线相对分配差 1 px；导出的 GTK text origin 不是 Win32 GDI baseline 的直接等价测量，因此不宣称基线零差异。GDI 字符串宽度与平均字宽已记录，Pango/GDI 没有独立的 tracking/character-spacing 调节量或等价报告列；当前差值是整串字形 advance 的度量结果，没有用任意字距补偿。Canvas 当前 API 没有每次 `Painter.text` 调用独立指定字体 family 的参数。
- Liberation Sans 只在隔离 Xvfb 进程通过临时 Fontconfig alias 测量并渲染候选截图，没有复制/提交字体文件，也没有改变生产默认字体。120 个同源控件度量样本（English 66、简中 54）中，English **64 改善 / 1 退化 / 1 不变**；简中 **12 改善 / 12 退化 / 30 不变**。宽度在 ±2 px 内的样本由默认 English 5/66 增至候选 56/66；简中由 4/54 增至 6/54。该候选不能同时稳定改善两种语言。
- 候选全页差异改善很小：English 对 Windows 的变化率只下降约 `0.0578–0.4093` 个百分点；简中变化率约从 `-0.0032` 到 `+0.0022` 个百分点（候选有略微回退或几乎不变的页）。因此没有足够的双语全页证据采用 Liberation Sans，也没有引入字体专用 C/GTK bridge；维持 DejaVu Sans + Noto CJK fallback，不缩放窗口或缩小字号。该实验只证明候选进程的度量变化，不证明字体具备 Windows 字体的完整字形/光栅等价。

## 深色主题对照

依据已采集 Windows v.3.5.2 原生暗色截图，production Canvas 暗色 palette 已对齐背景、导航、选中态、GroupBox 边线、字段、按钮、树状点线、滚动条及暗色 checkbox 面；浅色 palette、几何与生产字体均未改。Linux `GTK_THEME=Adwaita:light` 是隔离 runner 的固定主题，深色由应用自身 Canvas 状态实现，并非 GTK 原生暗色主题。

十组 558×335 的 Windows 原生 / Linux 生产配对截图已生成，并各有并排图、50% overlay、difference、heatmap 和 SHA。每组结论为 `CAPTURE_AND_COMPARE_PASS; PIXEL_IDENTITY_NOT_PASS`：

| 设置页 | English changed pixels | 简体中文 changed pixels |
|---|---:|---:|
| General | 10.2327% | 51.6086% |
| Memory cleaning | 12.6625% | 73.2932% |
| Appearance | 11.6311% | 54.3027% |
| Tray icon | 11.2534% | 54.6584% |
| Advanced | 5.9434% | 46.0889% |

这些变化率是整页原始差异，叠加字体/抗锯齿、Win32 与 GTK 控件栅格、平台状态，以及 12 项 Windows 简中运行时来源差异；没有把全部像素差异归因到字体。源语言差异仍 `DECISION_REQUIRED`。全像素 1:1 不通过，也没有因客户区尺寸一致而推定视觉等价。

## 主菜单与安全截图边界

- Linux 生产 XTEST 在 English 与简体中文下实际展开 File / View / Settings / Help，检查菜单文字/顺序、复选和快捷键显示、分隔槽跳过、语言子菜单、Enter、方向键、Escape 及焦点/窗口生命周期；安全的清理区域下拉只用于展示，清理项仍禁用，未调用任何清理动作。五页双语 light/dark 截图以及暗色 Reset/Close 焦点截图均已生成。
- Windows 原生 runner 取得四个主菜单英文截图。Runner UI Automation 未暴露托盘按钮；Windows 清理分割按钮没有安全 ExpandCollapse 路径；确认/结果弹窗需先触发真实清理。因此 Windows tray、Windows 清理区域 popup、确认/结果弹窗仍 `BLOCKED`，没有点击 Invoke 或发送清理命令。Linux safe clean-dropdown 截图不等于 Windows 原生 popup 验收。
- Weston 仅通过私有 Headless/Pixman socket 验证生产客户端连接与退出生命周期；没有 Wayland 鼠标/键盘输入，也没有活动 Hyprland 验收。日志中的 GTK seat critical / compositor shutdown diagnostic 原样保留，未归因或屏蔽。

历史 `40363399854` APT 超时及当时 P2B Weston 失败记录保留在后文作为事件历史；它们不代表本轮 exact-head 状态。其根因未确认。当前 exact-head 有界 APT update/install 均成功，不足以证明未来 runner 网络永远稳定。

## 固定范围

- 上游基准：Mem Reduct v.3.5.2，tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- PR #7：`feat/issue-3-p2c-settings-ui` → `feat/issue-3-p2b-main-window`，Draft，禁止合并。
- P2C-3R1 已验收基线：HEAD `2c745710210c8794b52a0e9eb19dd1830c00d8d6`。
- 本轮首个验证提交：`a58745f9b5ed029a00f1ce61d21e5348ea2dcaec`。它保留原设置客户区 558×335、184 项资源几何阈值、导航/树线/栅格样本与中文来源决策。
- 生产字体默认值未改变。Liberation Sans 仅在独立 Xvfb 测试进程中通过临时 Fontconfig 配置作为候选；只复制字体包许可说明到 CI artifact，不提交字体文件。

## 已知字体证据与待核差异

P2C-3 同源文本比较有 120 项：25 项宽高差在 2 px 内，95 项仍存在度量差异；其中英文文本宽度 Linux/Windows 比值中位数为 1.1469（66 个样本）。

Windows `WM_GETFONT` 报告逻辑别名 `MS Shell Dlg 2`、8.25 pt、weight 400；GDI ascent/descent 为 11/2 px。注册表 `FontSubstitutes` 指向 Tahoma，但现有证据不能证明 GDI 实际加载的物理字体文件。Linux Canvas 为 8 pt `Sans` / weight 400；英文常见匹配 DejaVu Sans，简中为 Noto Sans CJK SC，Pango ascent/descent 为 10/3 px。GDI 与 Pango 的字符串度量、字高和字形栅格分开记录；上下文文字锚点不当作 Win32 基线的直接等价值。

`std/ui` 的 `Painter.text` 暴露字号和粗细，但没有逐次字体家族参数。候选测试通过进程级 Fontconfig 映射评估可分发的 Liberation Sans；此方法会影响该测试进程内所有 Canvas 文本，不代表生产设置已生效。是否应增加受控字体桥接取决于候选字体的 GDI 度量和实际全页比较结果。

候选字体精确结果与限制见上方“字体度量与隔离候选结论”。`eea82a8` 对测试进程的 Fontconfig alias / Pango family assertion 修正已在 exact-head 候选作业执行；生产字体未改。

## 深色主题与菜单

Linux CI 将 GTK 主题固定为 `Adwaita:light` 并单独记录；通过生产程序 View 菜单切换应用自己的 Canvas 调色板。它不修改 GitHub runner 全局 GTK 主题，也不代表 GTK 原生暗色主题。Windows 配对截图由隔离 Windows 2022 runner 通过原版 `IDM_USEDARKTHEME=159` 产生。

依据旧生产截图/Windows 3.5.2 原生暗色截图（run `40355264229`，只作为修复前证据）逐像素取样，修正背景 `#242424→#171717`、导航 `#303030→#171717`、选中底色 `#3C5066→#333333`、GroupBox 边线 `#707070→#404040`、字段面 `#303030→#404040`、按钮面 `#3A3A3A→#272727`、树状点线 `#9A9A9A→#6D6D6D`、滚动条轨道/滑块 `#F0F0F0/#D7D7D7→#676767/#4D4D4D`。窗口背景目标是实测 Windows 的 `#171717`。首个 General 页面未勾选复选框内点为 Windows `#FFFFFF`、Linux 修复前 `#303030`；本次仅在应用暗色状态把复选框面改为白色，Light 模式仍使用既有 `#F4F4F4`，保留通过的 light 栅格基线。所有未实现设置仍按安全边界显示为禁用态，故其文字/勾选灰度差异归类为安全状态差异，不伪装成已启用。

当前 palette 修正确由 exact-head hosted 生产截图确认；dark/light 共用已验证的生产几何，暗色 PNG、GDI/Pango 度量与差异率均关联本报告列明的同一批 Actions artifacts 与 SHA manifest。

生产 XTEST 覆盖 File / View / Settings / Help 在 English 和简体中文下的展开截图、语言子菜单两种选择状态、清理区域下拉外观、分隔行不参与鼠标/键盘命中、Escape 关闭，以及暗色五页双语截图。所有清理项仍禁用；截图路径不触发清理。暗色 Reset / Close 焦点截图已随本轮同 head artifact 采集。

深色截图、几何复核、Overlay / Difference / Heatmap 与 SHA 已在同一精确 head 生成；十组全页差异率仍是 compare-only，不代表像素验收通过。

## 冻结视觉验收与剩余差异

沿用 P2C-3R1 精确证据：控件几何 184/184 PASS、最大分量偏差 1.5 px；导航选中框 10/10 PASS；树线像素样本 102/102 PASS；控件栅格 36/36 PASS；中间滚动状态 2/2 已采集。P2C-4 不改变这些阈值、Windows 原图或简体中文来源矩阵中的 12 个 `DECISION_REQUIRED` 项。

P2C-3R1 十组全页变化率为 5.0420%–14.8853%，只可作为历史趋势背景。完整像素 1:1 仍 `NOT PASS`。P2C-4 新截图和 candidate 指标必须按各自 artifact/PNG SHA 记录，不可把不同 run 的截图混成一组。

| 差异类别 | 当前处理 |
|---|---|
| 几何 | 冻结已通过的 184 项；深色模式比较同源矩形。 |
| 字体/字形 | GDI/Pango 度量和候选截图分别比较；不缩放界面、不缩小生产字号。 |
| 抗锯齿/控件栅格 | 与坐标和文字内容分开报告；离散栅格样本不代表整控件相等。 |
| 安全禁用状态 | 尚未实现或需特权的动作保持禁用。 |
| 中文来源差异 | 12 项保持 `DECISION_REQUIRED`，不混入字体差异率解释。 |
| 平台不可等价 | GTK/Pango 与 Win32/GDI 控件绘制不能承诺像素完全相同。 |

## 安全边界

本机仅执行治理、静态、Zamak/C 测试和构建，没有启动本机 GUI/Xvfb/Wayland、访问 Hyprland IPC 或注入本机输入。GitHub 测试仅使用临时 runner、Xvfb/XTEST、Wayland headless 生命周期；没有 root GUI、Polkit、drop_caches、Swap/ZRAM 或内核写入。

## 历史状态快照（P2C-4 初次验证阶段；由上方 exact-head 结果覆盖）

- PR #7 仍 OPEN / Draft；branch `feat/issue-3-p2c-settings-ui`，base `feat/issue-3-p2b-main-window`，HEAD `eea82a8c2cafb16b25acf3d319e084335548356d`，与 `origin` 一致。
- P2C run [40363399854](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40363399854) 的 Windows job 已 PASS；artifact `12670040771`，GitHub artifact digest `sha256:74ca2a4a52afcde5fe04334918587a77b1fc6c469ffce03916dbb9920aa616c6`。release tag/source、官方 release 资产、setup detached PGP signature 和 3.5.2 版本均由 runner 核验。Windows Server 2022 Datacenter build 20348，1024×768，96 DPI / 100% scaling；配置使用 Light Apps theme。实测设置窗口外框 574×374、客户区 558×335。GDI 记录 `MS Shell Dlg 2` 逻辑字体，8.25 pt / weight 400 / ascent 11 / descent 2 px；注册表替代为 Tahoma，但尚不能证明进程实际加载的物理字体文件。
- 该 Windows artifact 的 `result.txt` 列出 41 个截图哈希，本地逐项 SHA-256 核对 41/41 匹配。包含主窗与五页 EN/简中 light/dark 截图、主菜单四项 EN 截图及两种语言的 Memory cleaning 中段滚动截图。tray menu 未被 runner UI Automation 暴露；clean-region menu 的安全展开模式不可用；清理确认/结果窗口若无清理则无法打开，所以三类均保持 `BLOCKED`，未触发清理。
- 同一 P2C run 的 Linux job 截至 17:03 UTC 仍在 `Install runner-only build and X11 dependencies`。APT update 已完成，安装步骤开始于 16:53:35 UTC；GitHub 对在运行 job 暂未提供日志，因此**APT 停滞根因未确认**。候选字体、Linux 生产截图、暗色对照器和 Overlay/Heatmap 尚未执行，不能标 PASS。
- 当前 HEAD 的 PR `build-and-test` run [40363344887](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40363344887) 已 PASS：治理、Shell、C/Python 安全、Zamak 版本/check/test/native build 全部成功。另有同 SHA push build run `40363337339` PASS。P2B 生命周期 run `40363344884` 尚 pending，队列中上一 HEAD 的 Weston job `40355264157` 仍运行；不取消正在生成验收证据的作业。
- 本机无 GUI 的本轮验证：Zamak 0.2.0；project verification 33 项 PASS；Python/C 单测 3/3 PASS；Zamak 30/30 PASS；Zamak check/build PASS；`scripts/build-arch.sh` PASS；全部 shell 脚本 `bash -n` PASS；`git diff --check` PASS。未运行本机 Xvfb/Wayland/桌面输入，也未启动 GUI 或访问活动 Hyprland。

**准确限制：**以上成功项只覆盖各自列明的 HEAD/job。Linux 截图/字体实验/暗色比较/Weston 生命周期尚无本轮 exact-head 结果；这些仍为 `IN_PROGRESS`，不是 PASS。最终证据须在当前最终 HEAD 重跑并记录各 artifact / PNG SHA。

## 2026-10-26 17:44 UTC 状态更正

本节覆盖上方较早查询的 job 状态。P2C exact-head run `40363399854` 已结束 FAILURE：Windows 原版来源 job PASS，Linux Xvfb job 在 APT 依赖安装处 FAILURE，Windows/Linux comparator 因 Linux 依赖 job 失败而 SKIPPED。诊断 artifact `12672670397`（digest `sha256:8a3a5b0912138113ef9a9af85ad28c888e5e99cd2933a45be58cf5f9f7fefcd2`）记录 `apt_exit_code=124`，APT 子命令达到 38 分钟硬超时。APT 日志把 `fonts-noto-cjk` 61.2 MB deb 标为尚未完成下载项，并显示一次 `Ign` 后重试；没有 DNS、锁或缺包证据。故障表现为大字体包下载阶段超时，底层网络/镜像根因未确认。字体候选、Linux 生产截图、深色配对图及比较 artifacts 均未生成。

Windows artifact `12670040771`（digest `sha256:74ca2a4a52afcde5fe04334918587a77b1fc6c469ffce03916dbb9920aa616c6`）的 41/41 PNG SHA 与 `result.txt` 相符，但只证明 Windows 3.5.2 参考资料。原始 artifact 已保存在用户指定产物目录 `报告与研究/Mem Reduct Linux/P2C-4/run-40363399854/`。同 SHA build run `40363344887` 及 push build run `40363337339` 均 PASS。

P2B exact-head run `40363344884` 在本次查询（17:44 UTC）仍处 APT 安装步骤，尚未有可报告的最终状态；不取消该验收作业。工作树的暗色颜色修改仍未提交，字体与生产字号未改，新的 exact-head 截图/比较验证尚待完成。

17:50 UTC 复查：同一 P2B run 的 production Xvfb/XTEST job 已 PASS。APT 安装耗时约 35 分钟；主窗口隔离输入测试通过，设置页重复场景按 PR 条件 SKIPPED（由 P2C workflow 覆盖）。Weston job 仍运行在其 APT 安装步骤，故 Wayland lifecycle 尚未判定。该 run 使用的还是推送头 `eea82a8...`，没有本工作树的暗色 palette 修正，不可作为本轮截图验收。

17:52 UTC 最终状态：P2B run `40363344884` 整体 FAILURE。Xvfb job PASS，APT dependency artifact `12673795414`，生产主窗 XTEST artifact `12673540822`；Weston job 因同一 APT 38 分钟超时 FAILURE，诊断 artifact `12672539453`（digest `sha256:1b7bd7c60786698250248e7196b8921e911431376b9f7661dcdf91110ccdcafb`）。Xvfb APT 成功约用 35 分钟，Wayland apt 日志同样在 `fonts-noto-cjk` 61.2 MB 下载项停止。这个共现不证明下载拥塞由并发导致，但支持减少重复和无用依赖；当前工作树移除仅做英文生命周期验证的 Weston job 对 `fonts-noto-cjk` 依赖，并令 P2B/P2C workflow 共享 PR concurrency group、关闭运行中取消。两项 workflow 改动都必须由新 head Actions 再验证。
