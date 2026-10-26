# P2C-3 — Mem Reduct 3.5.2 原生视觉差异收敛

## 固定基准与本轮精确证据

- 上游：Henry++ Mem Reduct `v.3.5.2`，tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 本轮被测生产源码 SHA：`60be390eb0ff6c18564f35abcd23d36406c6d3f8`。PR #7 仍 OPEN / Draft，Base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。
- Windows 原生基准、Linux Xvfb 生产截图和比较器：[Actions run 40319586672](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40319586672)，三项 job 均 PASS。Windows artifact `12648157361`，Linux X11 artifact `12648453073`，Overlay / heatmap / diff / SHA artifact `12648552362`。
- Zamak build / governance / safety：[run 40319586751](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40319586751) PASS。独立生产 Xvfb + Weston 生命周期流程：[run 40319586679](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40319586679) 截至 2026-10-26 12:19 UTC 仍为 `IN_PROGRESS`：Weston Headless/Pixman job PASS；Xvfb job停在 runner-only GUI/build dependency 安装步骤，GitHub 尚未提供该运行 job 的日志。该 Xvfb 项记为 `NOT COMPLETE`，不将整条 run 报作 PASS。独立 exact-source native/Xvfb/comparison run `40319586672` 的生产 Xvfb/XTEST 作业已 PASS，可作为当前截图和几何证据；两条 run 分开记录。
- 官方 Release 来源、tag SHA、portable exe SHA-256、安装包 PGP 签名、版本、Windows 原生窗口和截图元数据见 Windows artifact。Portable exe SHA-256：`dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`；其 Authenticode 状态为 `NotSigned`，不与 setup 的 PGP 签名混为一谈。

Windows 使用 GitHub-hosted Windows Server 2022、1024×768、96 DPI / 100% scale、light theme；设置窗口外框 574×374，客户区 558×335，Win32 实测客户区屏幕起点 `(8,31)`。Linux 使用 GitHub-hosted Ubuntu 24.04、GTK 3.24.41、Xvfb 1024×768×24、96 DPI、scale 1、Adwaita light，Canvas 558×335。原始 Windows 客户区仅按实测原点裁剪，不拉伸、不缩放、不平移、不重采样。采集/对照日期为 2026-10-26 UTC。

## 控件几何、滚动和导航

- 资源 ID 控件几何：**184/184 PASS**，0 FAIL；所有 `x/y/w/h` 分量最大绝对偏差 1.5 px。已通过的 558×335 客户区与控件外框保持不变。
- 这 184 项包含先前缺失的 `IDC_REGIONS=124` 中间滚动状态：Windows 与 Linux 在 English、简中均采集 top / middle / bottom 位置和对应行矩形。原先 12 项 `NOT_COMPARABLE_NATIVE_STATE` 现已全部实测，不再排除；中间滚动图像另作 compare-only。
- `IDC_NAV=104` 当前 native 截图选中行边界独立提取后再与 Linux 比较，10/10 PASS；保留历史测量作为回归基线。102/102 个树状连接点与 native `#6D6D6D` 样本相同。该验证不是全 TreeView 像素等价声明。
- 36/36 控件 raster 定点检查 PASS，覆盖两种语言的页面底色、复选框 ROI 与 Reset/Close 按钮 6 个颜色/边缘样本。它们是离散取样，不表示按钮或控件整块光栅一致。

## P2C-2 到 P2C-3 全页像素变化率

每个配对使用同一显示设置和 558×335 客户区，原图未拉伸或重新采样。变化像素率是比较指标，全部仍标为 `COMPARE_ONLY; NOT PIXEL PASS`。

| 页面 | 语言 | P2C-2 | P2C-3 | 变化百分点 | 趋势 |
|---|---|---:|---:|---:|---|
| General | EN | 13.8234% | 12.0184% | -1.8050 | 改善 |
| General | zh-CN | 12.2297% | 10.6815% | -1.5482 | 改善 |
| Memory cleaning | EN | 16.7341% | 14.8853% | -1.8488 | 改善 |
| Memory cleaning | zh-CN | 16.6041% | 14.7542% | -1.8499 | 改善 |
| Appearance | EN | 14.5183% | 11.5123% | -3.0060 | 改善 |
| Appearance | zh-CN | 13.8961% | 10.9998% | -2.8963 | 改善 |
| Tray icon | EN | 16.8042% | 14.7900% | -2.0142 | 改善 |
| Tray icon | zh-CN | 15.8268% | 14.0288% | -1.7980 | 改善 |
| Advanced | EN | 7.3921% | 5.5871% | -1.8050 | 改善 |
| Advanced | zh-CN | 6.8528% | 5.0420% | -1.8108 | 改善 |

本轮 10/10 双语页面的变化像素率低于 P2C-2，范围仍为 **5.0420%–14.8853%**。改动中 Reset/Close 颜色和边缘来自实际 Win32 像素采样；内存区域文字原点左移 1 px。该改善不等于 UI 已达到像素级 1:1。

## 字体、文本与热键

Win32 运行时 `WM_GETFONT` 的 LOGFONT 为逻辑字体别名 `MS Shell Dlg 2`：8.25 pt、-11 px、400 weight；克隆 LOGFONT 后 GDI 文本度量高 13 px、ascent 11 / descent 2。`GetTextFaceW` 仍报告别名；Windows `FontSubstitutes[MS Shell Dlg 2]=Tahoma` 是独立注册表替代映射，并不能证明 GDI 实际加载的具体字体文件。

Linux GTK/Pango 样式为 `Sans`、8 pt、400 weight，英文 fallback 为 DejaVu Sans，简中 fallback 为 Noto Sans CJK SC，Pango ascent 10 / descent 3。66 个英文配对文本宽度中，Linux/Windows 比值中位数 1.1469，范围 1.0217–1.2121。当前 Windows ascent/descent 与 Pango 的垂直度量、字体宽度、抗锯齿和字形轮廓仍可见不同。Zamak Canvas `Painter.text` 没有 per-call family 参数；本轮没有盲目缩小全局字号，也没有复制、打包或提交 Windows 专有字体。

比较器统计 132 行文本记录：120 项原版/Linux 相同来源文本，12 项简中来源差异；同源文本中 25 项宽高差在 2 px 内，95 项仍有文字度量差异。Text metrics、控件几何、原生控件光栅和颜色状态分别记录，不相互替代。

Windows `IDC_HOTKEY=132` 现在按 Win32 `HKM_GETHOTKEY` 返回布局解析：低字节为 VK、高字节为 `HOTKEYF_*` 修饰位。实际 Runner 读到 `Ctrl+F1`（English、zh-CN 两条记录），与原版画面相符。上次失败是采集器颠倒了解析字节；修复后的 Windows workflow 已成功。空热键保留为 `UNASSIGNED`，API/键码错误不会静默吞掉。

## English / 简体中文来源差异

[P2C-3 来源语言差异矩阵](P2C-3_SOURCE_LANGUAGE_MATRIX.md) 和 artifact `source-language-diff.csv` 逐项记录 12 项差异：8 个内存清理列表文本、2 个 Advanced 文案和 `Single click:` 这 11 项在官方简中资源中没有翻译、Windows 中文运行时仍显示英文；另有 locale 名称 `Chinese (Simplified)` / `简体中文` 的显示差异。Linux 保留项目要求的简中翻译。12 项目前均为 `DECISION_REQUIRED`：严格复刻运行时英文，还是优先完整简中支持，需要产品决定；本轮没有悄悄改写标签。

## 五页双语截图索引与 SHA-256

以下是配对采集的 Windows 原生设置页 PNG 与 Linux 生产 Xvfb Canvas PNG 的 SHA-256。全部原始图、窗口客户区图、Overlay、差异热图、逐控件 CSV 和环境信息也在 artifact `12648552362` 的 `visual-sha256.csv` 中。

| 页面 | 语言 | Windows 原生 PNG SHA-256 | Linux 生产 PNG SHA-256 |
|---|---|---|---|
| General | EN | `360eab0e198c81e549cad1763db7b28ca1298abcb7a32bf2e810729345d9abd8` | `d37809048277aec70cffe15c37252336c7876d8afa52721653c70262ca7c4533` |
| General | zh-CN | `aa818cffa6116fad00378601a6a701ec774266025ca3cc69327919db16066bcf` | `e9e97b3f89fb6dd9ef2f14f417bb2736858fd5126bbb91d84158f71d5449e580` |
| Memory cleaning | EN | `803daa22d32cf11a8a35ff04196cfc12a320dfc201f052385e58af7e151579e1` | `0f8ab8ec2b15fa6010c29a0d70815f080d5ef66a7193f016b51f3ec18f9450e4` |
| Memory cleaning | zh-CN | `5f197f60c8fc7123b074787aa587a5d78745f1328214eee87faa550e7e4cc04b` | `d7f90995da1b9131557f784edf07da498bfd92420e11bc52b58c6c6b9b1c92e5` |
| Appearance | EN | `fee5b187e727991d244e72ff8034eae47e5cfcf638d3165e5b63c4a7763ea075` | `648ba05739ab08305e591601c3317b0cc5282b56d92cdecd3349be58af5e4800` |
| Appearance | zh-CN | `56e4235ade8b5f6907179bfb33750863027305819a6a2c4651356550af3b615e` | `4bc39e23c8f9da2ad3a61c96227b3457e49a157a9ecb85e498ef1d44bf7c3541` |
| Tray icon | EN | `5c52bf2ae77ad7ff6050ab003ed54cdf22176c69cfe03f5c3ce4881eeff5764d` | `de1973f7f7bb9fec3f96f8cac2297d7f92222b957bd6e72d7dc4702d410f96d5` |
| Tray icon | zh-CN | `4b5bd79aab2cdf316d2161c9629ce37d4a691601f49c0d90646d10fb64db7517` | `a8829ee93dfbe10ee9a3bbb7a6e0fc7ecf73efd9e85cd93ebb66488338c83127` |
| Advanced | EN | `720a31310bb8a1f013cb3c040ba21b6ee387424e09de45b693b26c7ac0b122c5` | `856e19212f5eaf76ceb4d9770c0ea291d8c3ffe98cc18a2818abacda48009536` |
| Advanced | zh-CN | `002ffa02933cfee0234b8fa7e94693619fe6cbaa107e12c253c278f5a243b40f` | `b098f0938644decfdc42754c2d495d0e349d28a5a21987fde2943744241cf34a` |

## 截图覆盖缺口与安全边界

- Windows Runner 已采集 File / View / Settings / Help 菜单及主窗、五页 EN/简中 light/dark 参考。Linux 本轮五页图为 Adwaita light；dark Linux 五页对照尚未运行。
- Tray 菜单仍 `BLOCKED`：Runner UI Automation 没有暴露 Mem Reduct 通知区按钮。清理区域菜单仍 `BLOCKED`：尚无不会触发清理的安全展开路径。清理确认与结果对话框仍 `BLOCKED`：打开它们需要触发真实清理，本轮没有执行。
- 简中完整翻译与 Windows 3.5.2 实际未翻译文本的冲突需产品决定。字体物理文件身份、Win32/Gtk 全控件绘制、文本抗锯齿与禁用状态外观仍存在平台差异。
- 没有运行真实清理、Polkit、内核写入、Swap/ZRAM 操作、root GUI 或用户活动桌面/Hyprland。Weston 仅用于隔离 Headless 客户端生命周期，不代表 Hyprland 或 Wayland 输入验收；日志诊断按 Runner 原文保留，不推断崩溃根因。

## 本机验证与结论

- Zamak 0.2.0：`zam check .` PASS；`zam test .` **29/29 PASS**；`zam build .` PASS；`scripts/build-arch.sh` PASS。
- `python3 scripts/verify_project.py` 33 项 PASS；Python/C 安全单测 3/3 PASS；8 个 shell 脚本 `bash -n` PASS；比较器 `py_compile` PASS；`git diff --check` PASS。
- 本机没有启动生产 GUI、Xvfb、Wayland 或活动桌面。`windows-native-baseline.ps1` 在 GitHub Windows Runner 上执行；本机 PowerShell 不可用。
- 总体视觉 1:1 / 全控件 2 px 光栅验收：**NOT PASS**。184 个资源矩形的 2 px 初步几何目标：**PASS**。文字、全页像素与原生控件渲染仍需继续收敛。
