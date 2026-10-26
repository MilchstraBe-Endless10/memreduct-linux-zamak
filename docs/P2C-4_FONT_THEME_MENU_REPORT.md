# P2C-4 — 字体、控件绘制与深色主题校准

状态：暗色 Canvas 调色板修正已在工作树完成，尚未提交。当前远端 `eea82a8c2cafb16b25acf3d319e084335548356d` 的 Windows 来源采集通过，但 Linux 截图 job 在 APT 安装阶段超时；因此修正后的 Linux 暗色截图、Liberation 字体候选和 Windows/Linux 比较仍未运行。不能据此判定本轮视觉校准通过。

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

候选字体精确结果：当前 HEAD 的隔离 Liberation Sans 实验步骤尚未开始。此前 `eea82a8` 的修复仅调整测试进程的 Fontconfig alias / 实际 Pango family 断言；在 exact-head 度量与截图比较完成前，不修改生产字体或宣称候选有改善。

## 深色主题与菜单

Linux CI 将 GTK 主题固定为 `Adwaita:light` 并单独记录；通过生产程序 View 菜单切换应用自己的 Canvas 调色板。它不修改 GitHub runner 全局 GTK 主题，也不代表 GTK 原生暗色主题。Windows 配对截图由隔离 Windows 2022 runner 通过原版 `IDM_USEDARKTHEME=159` 产生。

依据旧生产截图/Windows 3.5.2 原生暗色截图（run `40355264229`，只作为修复前证据）逐像素取样，修正背景 `#242424→#171717`、导航 `#303030→#171717`、选中底色 `#3C5066→#333333`、GroupBox 边线 `#707070→#404040`、字段面 `#303030→#404040`、按钮面 `#3A3A3A→#272727`、树状点线 `#9A9A9A→#6D6D6D`、滚动条轨道/滑块 `#F0F0F0/#D7D7D7→#676767/#4D4D4D`。窗口背景目标是实测 Windows 的 `#171717`。首个 General 页面未勾选复选框内点为 Windows `#FFFFFF`、Linux 修复前 `#303030`；本次仅在应用暗色状态把复选框面改为白色，Light 模式仍使用既有 `#F4F4F4`，保留通过的 light 栅格基线。所有未实现设置仍按安全边界显示为禁用态，故其文字/勾选灰度差异归类为安全状态差异，不伪装成已启用。

以上只描述修复目标和实现差异；当前工作树的颜色改动还未由 hosted 生产截图确认。修复后 dark/light 必须保持相同生产几何，所有暗色 PNG、GDI/Pango 度量与差异率都要从同一最终 run 及其 SHA manifest 对应。

生产 XTEST 新覆盖 File / View / Settings / Help 在 English 和简体中文下的展开截图、语言子菜单两种选择状态、清理区域下拉外观、分隔行不参与鼠标/键盘命中、Escape 关闭，以及暗色五页双语截图。所有清理项仍禁用；截图路径不触发清理。暗色状态下 Reset / Close 焦点截图正在补入后续精确 head。

深色截图覆盖、几何复核、Overlay / Difference / Heatmap 与 SHA：当前 Linux 和比较作业尚未运行或失败，须在包含本次调色板修正的新 head 重新执行，并用同一 run 与 PNG 哈希闭环后更新。

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

## 当前 exact-head 状态（2026-10-26）

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
