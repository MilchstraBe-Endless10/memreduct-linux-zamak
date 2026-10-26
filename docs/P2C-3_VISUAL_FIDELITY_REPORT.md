# P2C-3 — Mem Reduct 3.5.2 原生视觉差异收敛

## 固定基准与当前证据

- 上游唯一基准：Henry++ Mem Reduct `v.3.5.2`，tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 当前被测源码（本轮视觉微调前）：`b4b5de5c383bebab3c4ebb9e784cf56f577d5907`。本报告中的现有截图/像素率是这个源码 HEAD 的基准，不代表后续未提交绘制改动。
- Windows 原生采集：[run 40312777621](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40312777621)，artifact `12645980196`。Windows job PASS；设置页及主菜单截图、字体/资源矩形记录和滚动状态来自该 artifact。
- Linux 生产 X11：[run 40312777567](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40312777567)，artifact `12645029581`。Xvfb/XTEST 与生产窗口测试 PASS；同一源码 SHA `b4b5de5`。
- Windows：GitHub-hosted Windows Server 2022，1024×768，96 DPI / 100% 缩放，light theme；设置外框 574×374，客户区 558×335，截图采集日期 2026-10-26 UTC。Linux：GitHub-hosted Ubuntu 24.04，GTK 3.24.41，Xvfb 1024×768×24，Adwaita light，96 DPI，设置 Canvas 558×335。
- Windows 原始窗口图只依 Win32 实测客户区原点 `(8,31)` 裁剪为 558×335；没有拉伸、缩放、移位或重采样。源图及派生图 SHA-256 在比较器 `visual-sha256.csv` 中逐张记录。
- 该轮 Windows 原生已采集五页 EN/zh-CN、五页双语 dark、主窗 light/dark 与 File/View/Settings/Help 菜单。托盘菜单因 Windows UI Automation 未暴露通知区按钮而 `BLOCKED`；清理区域菜单的安全展开路径未找到而 `BLOCKED`；确认/结果弹窗因必须触发真实清理而 `BLOCKED`，没有执行清理。

## 几何与滚动状态

Windows 资源 ID 矩形与 Linux 生产 Canvas 矩形按当前同源截图逐项比较：**184/184 PASS**，每个 `x/y/w/h` 分量最大绝对偏差 1.5 px，0 FAIL。该 184 项包括此前未采集的 `IDC_REGIONS` 中间滚动行；不是控件光栅或整页像素通过。558×335 客户区及已有控件外框保持不变。

`IDC_REGIONS=124` 已在 EN/zh-CN 的 top / middle / bottom 三个滚动位置采集 Windows 行矩形及 Linux 对应滚动截图。中间位置英文文字墨迹框的 x 起点偏差为 +1 px，宽度差随字体而变（6–17 px）；简体中文对应位置还存在原版未翻译英文与 Linux 中文译文的文本/字形差异，部分 y 起点相差 2 px。不得把文字墨迹框误记为控件矩形差异。

## P2C-2 到本轮原始变化像素率

以下当前值是源码 `b4b5de5` 的 exact-source Windows/Linux 配对；全页变化率仅供前后回归，`COMPARE_ONLY; NOT PIXEL PASS`。

| 页面 | 语言 | P2C-2 | 当前基准 | 变化百分点 | 趋势 |
|---|---|---:|---:|---:|---|
| General | EN | 13.8234% | 13.8234% | 0.0000 | 不变 |
| General | zh-CN | 12.2297% | 12.4924% | +0.2627 | 回退 |
| Memory cleaning | EN | 16.7341% | 16.7341% | 0.0000 | 不变 |
| Memory cleaning | zh-CN | 16.6041% | 16.5565% | -0.0476 | 改善 |
| Appearance | EN | 14.5183% | 14.5183% | 0.0000 | 不变 |
| Appearance | zh-CN | 13.8961% | 14.0117% | +0.1156 | 回退 |
| Tray icon | EN | 16.8042% | 16.5950% | -0.2092 | 改善 |
| Tray icon | zh-CN | 15.8268% | 15.8396% | +0.0128 | 回退 |
| Advanced | EN | 7.3921% | 7.3921% | 0.0000 | 不变 |
| Advanced | zh-CN | 6.8528% | 6.8528% | 0.0000 | 不变 |

整体为 6.8528%–16.7341% 不同像素，仍明显不等价。P2C-2 数值与当前数值各来自其对应 run 的截图配对；只有相同采集条件可比，不能把某一个局部改善写成全页通过。

## 字体与文本

Windows 可访问的原生控件 `WM_GETFONT` 返回逻辑 face `MS Shell Dlg 2`；克隆该 `LOGFONT` 后取得 8.25 pt、GDI 字高 -11 px、字重 400、`TEXTMETRIC` 高 13 px、ascent 11 / descent 2。`GetTextFaceW` 仍返回逻辑别名 `MS Shell Dlg 2`，不能据此声称已知实际字体文件。本轮本机化前 artifact 没有注册表替代映射字段；本地采集器改为把 GDI 返回别名与 `FontSubstitutes[MS Shell Dlg 2]` 分开记录，待新 Windows job 核验替代值。

Linux GTK/Pango 为系统 `Sans`，英文解析为 DejaVu Sans，简中回退 Noto Sans CJK SC；生产 Canvas 调用字号 8 pt，Pango ascent 10 / descent 3。60 个英文相同文案宽度配对的 Linux/Windows 比值中位数为 1.146，范围 1.022–1.212。字体度量和文本抗锯齿仍是明确差异。没有复制、打包或提交 Windows 专有字体；Zamak Canvas `Painter.text` 没有按调用选择字体家族的参数，因此本轮不盲目减字号或全局换字体，避免破坏垂直度量和控件空间。

Windows `IDC_HOTKEY=132` 空白是原生控件未分配快捷键的有效状态，采集器现通过 `HKM_GETHOTKEY` 区分 `UNASSIGNED`，API/键码映射失败则显式失败；不能把空值静默当作采集成功文本。

## 控件外观与待验证增量

几何通过项不再改动。本地候选改动只涉及外观/文字墨迹：

- Reset/Close 使用原生截图实测的 system-face `(240,240,240)`、raised edge `(173,173,173)` 和 face `(225,225,225)` 绘制；对应 6 个采样点由当前 Win32 控件矩形定位，覆盖两按钮与两种语言。采样器必须同时核对 native 与 Linux 像素，整按钮光栅仍不作等价声明。
- `IDC_REGIONS` 标签 inset 从 22 px 改为 21 px，对齐实测的英文墨迹框 +1 px 起点差；不改变列表控件矩形。
- 比较器把 `IDC_LANGUAGE=123` 运行时当前项纳入来源矩阵，并分离 GDI alias、Windows 字体替代映射字段；Windows `IDC_HOTKEY=132` 采集错误不再静默吞掉。

上述绘制/采集器增量尚未由新 HEAD 的 Xvfb 截图与 GitHub Actions 验证。对应代码合并前应以新 CI 输出核对按钮像素样本、五页双语 changed-pixel 率、文本度量和 SHA 清单；任何失败需保留为 FAIL 并修正，不能沿用本节基准代替新证据。

## 中英文来源差异

[P2C-3 来源语言矩阵](P2C-3_SOURCE_LANGUAGE_MATRIX.md) 当前记录 11 项简中界面文案（8 个内存区域、2 个 Advanced 标签、`Single click:`）与 Windows locale 名称 `Chinese (Simplified)` / Linux “简体中文”这一项显示差异。前 11 项的简中原版在 Windows 实际仍显示英文；不是字体问题。Linux 暂保留已要求的完整简中翻译。原版逐字忠实与完整简中本地化之间的选择需要产品决策，未暗自修改。

## 测试 / 当前门槛

- 当前本机未提交候选：Zamak 0.2.0 `check` PASS、29/29 tests PASS、`build` PASS；33 项项目治理检查 PASS；Python/C 安全测试 3/3 PASS；8 个 Shell 脚本 `bash -n` PASS；C bridge/test 源码 `cc -fsyntax-only` PASS；Arch build `scripts/build-arch.sh` PASS；`git diff --check` PASS。
- 隔离 runner 对源码 `b4b5de5` 的生产 Xvfb/XTEST 与 Weston lifecycle run `40312777567` PASS（X11 artifact `12645029581`、Wayland artifact `12646550670`）；Weston 只验证 headless client 生命周期，不代表真实 Wayland 输入。保留 GTK seat critical 与 layer teardown 诊断，不臆断根因。
- Windows run `40312777621` 原生采集 job PASS、artifact `12645980196`；该 run 的 Linux screenshot job在 apt 安装依赖阶段达到 45 分钟超时后 CANCELLED，截图步骤和 comparison job SKIPPED。不得称此 run 的全 workflow/比较 job PASS。另有相同源码 SHA 的独立生产 Xvfb artifact可供配对比对。
- 当前本地按钮/文本微调还未截图测试；待推送后以 exact-head Windows、Xvfb、comparator、Weston 与 build/test workflows 重新验收并替换上文暂存状态。
- 像素级整体 1:1：`NOT PASS`；Hyprland 输入、真实清理、Polkit、Swap/ZRAM 写入：`NOT RUN`。
