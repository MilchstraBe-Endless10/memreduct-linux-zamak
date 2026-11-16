# Zamak 0.2.0 UI 精确绘制可行性记录（P2A-R1）

**历史状态（P2A-R1）：**Canvas 绘制和 Zamak 0.2.0 构建已验证；R6 隔离 Xvfb/XTEST 对探针完成 hover/pressed/release ROI、鼠标命中、菜单、键盘、双语、resize 与 WM close；Weston Headless/Pixman 验证 Wayland socket、GTK Canvas 绘制和正常退出。生产 UI、Windows 2 px 对照、真实 Hyprland 输入、托盘/SNI 与 HiDPI 仍未通过验收。测试探针不改生产主程序，不连接内存读取、helper、Polkit、drop_caches、Swap 或 ZRAM。

## 2026-10-28 第三方界面库复核

本节是针对 P2C 实际截图和用户要求的后续技术评估，不覆盖下方 P2A 历史结论，也不表示已切换生产 UI 工具包。

**状态更新（2026-10-29）：**本节中“下一项值得做 Winelib 单页 POC”的建议已执行。R6 Winelib General 页实测结果与当前推荐见本文后面的“R6 Winelib POC 结果复核与实际前端缺陷修正”。后文的 R6 结果优先于本节的计划性表述。

### 实际证据

- P2C 设置页控件几何为 `184/184 PASS`，最大分量偏差 `1.5 px`；导航 `10/10`、树线 `102/102`、控件栅格 `36/36`。这些是几何/局部栅格测试，不代表整页像素相同。
- 真实 Windows 字体请求为 `MS Shell Dlg 2`、`8.25 pt`，GDI ascent/descent `11/2 px`。P2C 设置页比较使用 Pango `Sans 8 pt`，ascent/descent `10/3 px`；P2B 主窗口记录为 `11/3 px`，两者来自不同 Canvas/字体采集，不混作同一度量。P2C 的 120 条同源设置文本中，25 项宽高差在 `2 px` 内，95 项仍有度量差。另 12 项简体中文来源差异仍为 `DECISION_REQUIRED`。
- Liberation Sans 候选没有进入生产：English 66 个样本中 64 改善、1 退化、1 不变；简中 54 个样本中 12 改善、12 退化、30 不变。它无法在不损害中文结果的条件下解决字体问题。
- Qt 6.12 离屏探针使用 `QWindowsStyle` 并请求 `MS Shell Dlg 2 8.25 pt`，实际匹配到 Liberation Sans；度量为 ascent/descent `10/2 px`、height `12 px`。生成的控件图仍明显偏离 Windows 3.5.2，证明默认“Windows 风格”不是 Win32 GDI/控件的逐像素实现。探针没有改动生产代码。
- 隔离 Qt 6.12 `QProxyStyle` General 页 POC 已按 Windows resource geometry 实测布局，并与同一 Windows 3.5.2 截图配对。请求 `MS Shell Dlg 2 8.25 pt` 后 Qt 实际解析到 Liberation Sans，Qt font metrics ascent/descent/height 为 `9.95/2.33/12.28 px`，Windows GDI 为 `11/2/13 px`。7 条匹配英文字符串中 4 条宽度误差≤2 px；7 条中文字符串中只有 2 条≤2 px，3 条中文标签仍窄 4–10 px。当前配对 PNG 重算的整页 changed-pixel 为 English `12.7155%`、简体中文 `11.9740%`，均 NOT PASS。POC 的操作型复选框仅为比较而启用，未连接任何功能；生产 GTK 对应项按安全要求禁用，两组截图状态不同，因此不能拿两组总像素率直接比较，也不能据此宣布 Qt 优于 GTK。
- 按控件区域看，Qt 在 General group 区与 GTK 相比更接近 Windows（English 14.3056% vs 17.3415%；简中 13.3301% vs 14.8676%），但导航与底部按钮并未同时改善；Qt 导航 English `10.8607%` / 简中 `9.8716%`，GTK 为 `8.3596%` / `7.1564%`。Qt Reset/Close ROI 差异约 `55%`，GTK 分别约 `8.4%/7.7%`（简中 `8.8%/6.5%`）。这些结果来自不同控件实现/状态，只可定位缺口，不构成工具包排名。当前无证据支持直接把生产 Canvas 换成 Qt。
- 有效深色语义配对的页面 changed-pixel 为 `5.4004%–12.6625%`；整页 1:1 仍 `NOT PASS`。主窗 `20.943604%` 的旧 RGB 差异样本未配对动态数据、不可用指标及安全禁用状态，不能作为验收率。
- 当前生产栈已经使用第三方组件：Zamak `std/ui` → libui-ng → GTK3，Canvas 文本由 Pango/Cairo 路径绘制。现有差异不是“完全没用第三方库”，而是这条 Linux Canvas 路径不提供 Win32/GDI 控件树和逐次绘制的字体族选择。
- 同一主窗尺寸的历史截图不是原生整窗 1:1 配对：Linux app-owned capture 为 `337×369`，且不含可核实的 Hyprland 装饰边框；官方 Windows 截图是完整 HWND。历史 `20.943604%` 只作差异示例，不作验收结果。设置页 `184/184` 不能外推到主窗。

### 工具路线

| 路线 | 适合做什么 | 本项目的限制 | 结论 |
|---|---|---|---|
| Qt 6 Widgets + `QProxyStyle` | 标准窗口/焦点/键盘/可访问性；自定义 checkbox、group box、combo、spin、按钮和 pixel metrics | 默认 `QWindowsStyle` 只是 Qt 在 Linux 的绘制；General 页 POC 尚未降低可比的整页差异；Qt 仍用 Linux 字体匹配及栅格路径 | 可作为原生前端候选；只有逐页实测有改善才迁移，不因控件库名称默认接受 |
| Qt 6 Widgets + `QCommonStyle` | 从零实现一套完整控件绘制与布局规则 | 工程量比 `QProxyStyle` 大；必须补齐 disabled/focus/hover/pressed/RTL/键盘等状态 | 只有代理样式无法覆盖时再考虑 |
| Zamak Canvas / GTK3 | 维持当前小依赖、固定尺寸的精确几何和自绘 | Canvas 不提供 Win32 字体或控件；每个状态、命中区、可访问性和键盘行为由项目维护 | 当前生产基线可继续修几何；不能把它当成“原生控件自动像 Windows” |
| GTK 原生控件 / CSS | Wayland 桌面整合、可访问性和标准交互 | 外观受 Adwaita/用户 GTK 主题控制；CSS 不是 Win32 控件引擎 | 适合普通 Linux 界面，不适合默认 1:1 目标 |
| wxWidgets/wxGTK | 跨平台原生控件 API | 官方 wxGTK 是 GTK port；Linux 仍继承 GTK 渲染差异 | 不解决当前核心差异 |
| Wine/Winelib | 用 `winegcc`/`wrc` 编译上游 Win32 C/RC 资源，最大化复用原窗口消息和控件树 | 需先审计上游 API、WINE 库与 GPL 依赖；打包体积、运行时、Wayland 窗口/输入以及 Wine 控件/字体差异都需实测；不保证与 Windows GDI 像素相同 | 下一项值得做的最小 POC：仅编译一个官方设置页并测量；未过双语/控件/Wayland门槛前不替换生产前端 |

### Qt 隔离 POC 的可复现结果

POC 使用 Qt 6.12 `QApplication`、`QWindowsStyle` + 自定义 `QProxyStyle`，固定 `558×335` 客户区，采用 Windows Server 2022、96 DPI 的官方 v3.5.2 General 截图。控件位置直接读取该运行 artifact 的 `windows-settings-geometry.csv`，没有根据 Linux 页面反推坐标。离屏绘制采用 `QT_QPA_PLATFORM=offscreen`，没有打开 Hyprland 窗口或发送桌面输入。

| locale | Qt 实际字体 | Qt ascent/descent/height | GDI ascent/descent/height | changed pixels | verdict |
|---|---|---:|---:|---:|---|
| English | Liberation Sans | 9.95 / 2.33 / 12.28 px | 11 / 2 / 13 px | 12.7155% | NOT PASS |
| 简体中文 | Liberation Sans + Qt 字形 fallback | 9.95 / 2.33 / 12.28 px | 11 / 2 / 13 px | 11.9740% | NOT PASS |

Qt 对某些 English 标签宽度更接近 GDI，例如 `Always on top` 为 Qt `68.39 px` / GDI `68 px`，`Load on system startup` 为 `114.19/113 px`；较长标签仍有 `6.53 px` 差异。简中 `窗口始终置顶` 为 `65.91/72 px`，`在内存清理前手动确认` 为 `109.84/120 px`。总像素差仍包括 glyph raster、控件边缘与主题，以及不能伪装启用的产品状态。

这次 POC 说明 Qt 有足够的钩子让我们逐项绘制控件，但 Qt 默认 Windows style 和一页 QProxyStyle 调整本身不够。底部按钮与导航 ROI 也否定了“换成 Qt 控件就自然更像 Windows”的假设。若产品把 1:1 作为硬要求，下一项研究应比较 Wine/Winelib 重用 Win32 资源与继续逐项重画的总代价；在新路线通过双语逐页对照前，不迁移生产前端。

### 字体结论

Microsoft 的 `MS Shell Dlg 2` 是逻辑字体名，Microsoft 文档说明其在对应 Windows 系统上映射到 Tahoma；同一文档提醒映射随 Windows 版本变化。Windows 字体文件通常不允许随应用重新分发。Linux 上即使用户另行合法安装了 Tahoma，FreeType/Pango 或 Qt 与 GDI 的 hinting、抗锯齿、字体 fallback 和基线仍可能不同。因此：不要靠偷偷打包 Tahoma、缩小字号或整体缩放伪造“相同字体”；应记录实际解析字体、逐字符串字宽/基线/字高和实际 PNG。

### 推荐的下一轮实现拆分

1. 不迁移整套 UI 到 Qt 默认控件；Qt General POC 只是原生候选样本，当前 changed-pixel 未 PASS。
2. 如果 1:1 优先级高于纯 Linux 原生控件，先做 Winelib/Wine 单页复用试验，核实授权、打包、Wayland 窗口/输入和字体解析；与 Qt 自绘路线按相同截图、resource ID、locale、DPI 和状态比较。
3. 若选择 Qt，必须使用 `QProxyStyle`/自绘控件逐个覆盖 checkbox、group box、combo、spin、按钮、TreeView、禁用/焦点/按下状态；用同一份 Windows GDI 和 Linux QFont/Pango 采集验证文本与 glyph，不通过时保留当前前端。
4. 无论 UI 使用 GTK 还是 Qt，内存采样与动作层都保持独立；避免在 C++ 和 Zamak 两份实现同一解析/策略。继续用临时文件和故障注入验证 helper，不访问真实内核写入。

验证工具建议：Qt 6 Widgets/Qt Test、CMake、`QFontInfo`，现有 Windows GDI 采集、Linux Xvfb/隔离 Weston、ImageMagick/OpenCV 的几何/文字/热图对照。Qt 官方将 `QProxyStyle` 用于调整平台样式，将 `QCommonStyle` 用于完整自定义；微软文档说明 DLU 必须由真实 dialog font 映射。Wine 是 Win32 兼容层，不是原版 Windows 二进制或 GDI 的逐像素保证。

主要来源：

- [Qt Widgets Styling Approaches](https://doc.qt.io/qt-6/qtwidgets-styling-approaches.html) 与 [QProxyStyle](https://doc.qt.io/qt-6/qproxystyle.html)
- [Microsoft About Dialog Boxes](https://learn.microsoft.com/en-us/windows/win32/dlgbox/about-dialog-boxes)、[MapDialogRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapdialogrect) 与 [MS Shell Dlg / MS Shell Dlg 2](https://learn.microsoft.com/en-us/windows/win32/intl/using-ms-shell-dlg-and-ms-shell-dlg-2)
- [Microsoft font redistribution FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq) 与 [Tahoma family information](https://learn.microsoft.com/en-us/typography/font-list/tahoma)
- [WineHQ](https://www.winehq.org/)、[Linux kernel `drop_caches` documentation](https://docs.kernel.org/admin-guide/sysctl/vm.html#drop-caches)
- [Liberation Fonts project](https://github.com/liberationfonts/liberation-fonts) 与 [Google Fonts Arimo](https://fonts.google.com/specimen/Arimo)。它们提供可再分发候选，但其公开 metric-compatibility 目标是 Arial/Times/Courier 等，不是 Tahoma；不能把它们称为 Tahoma 克隆。

Figma 可辅助检查状态稿，但既有官方 RC 与真实截图才是尺寸和状态依据；不做视觉重新设计。

## 2026-10-28 上游 Win32 资源与 Wine Tahoma 字体验证

### 原版具体怎么构建界面

固定上游 `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 的 `src/resource.rc` 使用 `DIALOGEX` 资源、控件 ID、Dialog Units 和标准 Win32 控件类，包含 TreeView、GroupBox、Button/CheckBox、ComboBox、Edit/UpDown、ListView/ScrollBar 等。`IDD_MAIN` 是 `224×214 DLU`；五个设置子页模板各为 `260×168 DLU`。Windows 按实际 dialog font 的平均字符宽和高度执行 DLU→pixel 转换，不能把 DLU 直接乘固定缩放值当成跨字体/跨 DPI 的真值。

资源文件声明 `FONT 8, "Ms Shell Dlg"`；已采集运行时 `WM_GETFONT` 则返回 `MS Shell Dlg 2`、8.25 pt、weight 400。两条证据分别是资源声明和运行时逻辑字体名；Microsoft 文档说明这些名称会按 Windows 版本/语言映射，不能把逻辑别名、注册表 substitution 与最终加载的物理字体混为一谈。原版的结构依据是资源模板和 Win32 控件行为，不是截图上手工估计后用统一 Canvas 缩放。

### 可合法测试的 Tahoma 度量兼容候选

Microsoft 不允许应用把 Windows 自带 Tahoma 字体文件作为普通应用资源重新分发。Wine 项目维护了 Wine Tahoma 字体替代物，源字体文件的声明采用 LGPL-2.1-or-later；其目标是匹配 Tahoma 的字体度量，不是 Microsoft 字体文件，也不承诺字形轮廓和栅格与 Windows GDI 相同。

本轮从 Wine 9.0 上游字体源在隔离目录临时加载 Regular/Bold，未安装到系统、未复制进仓库。SHA-256：Regular `90f1abad4da0ce03da5d2277ee75a9600ce812d37c90eab76e0838ffa7937e55`；Bold `c29342ba99a04f983193756f01eb6de946fd4d33ba0bb7061aff11e70871b6f2`。Qt 6.12 `QProxyStyle` General 页比较器在私有 Fontconfig 配置下把 `MS Shell Dlg 2` 解析到该 Tahoma 字体；窗口、截图尺寸和控件状态沿用既有 558×335 General 页 POC。

| 字体映射 | Qt 实际字体 | ascent / descent / height | `General configuration:` advance | 中文标签 advance | English / 简中 changed-pixel |
|---|---|---:|---:|---:|---:|
| 上一候选 Liberation Sans | Liberation Sans | 9.953 / 2.328 / 12.281 px | 108.063 px | 109.844 px | 12.7155% / 11.9740% |
| Wine Tahoma 私有 alias | Tahoma (Wine font file) | 9 / 2 / 11 px | 107.406 px | 109.844 px | 12.7256% / 11.9820% |
| Windows 3.5.2 GDI | `MS Shell Dlg 2` 逻辑字体 | 11 / 2 / 13 px | 原始采集值见 Windows 字体 artifact | 原始采集值见 Windows 字体 artifact | 比较对象 |

改用 Wine Tahoma 后，两个全页 changed-pixel 数值都略有上升；简中 advance 没变化，继续走 CJK fallback。当前 POC 因此没有支持将 Wine Tahoma alias 切入正式界面。结果说明可分发字体候选能缩小许可证阻碍，但没有解决字体基线、CJK fallback、字形栅格或 Win32 控件绘制差异；整页 1:1 仍为 **NOT PASS**。原始 Windows、Liberation Sans 和 Wine Tahoma 三方对照图保存在独立 R4 研究 artifact，不覆盖旧截图。

### 推荐的前端技术路线

| 路线 | 使用的第三方组件 | 适用范围 | 本项目结论 |
|---|---|---|---|
| Qt 6 Widgets + `QProxyStyle` | Qt Widgets / QPainter / QFont / Fontconfig | 保留标准窗口、键盘、可访问性；针对 Win32 checkbox、GroupBox、按钮、Combo、TreeView 重绘并覆写 metric | 适合作为有边界的前端原型。Qt `QWindowsStyle` 或 Fusion 名称不会调用 Windows GDI，也不会自动复刻 Win32；已有和本轮 POC 均未降低整页差异。 |
| Qt 6 Widgets + `QCommonStyle` | Qt Widgets / QPainter | 从头定义全部绘制、状态和尺寸 | 只有 `QProxyStyle` 逐控件覆盖后仍缺少控制权时采用；工作量更大，必须同时覆盖 focus/hover/pressed/disabled、菜单、键盘与 CJK fallback。 |
| Wine/Winelib + 上游 Win32 资源 | Wine USER32/GDI/COMCTL32、Wine Wayland driver | 复用 Win32 resource 和标准控件，是重用原版控制树最直接的路线 | 对严格复用原版控件最有希望；Wine 9.22 起 Wayland driver 进入默认配置，11.15 的已报告启动回归在 11.16 标记修复，当前 Arch 包为 11.20。仍需实测 driver/DPI/输入/控件绘制；Wine GDI 不等于 Windows GDI，运行时也较重。未过单页截图、Wayland 生命周期、打包及许可门槛前不迁移正式产品。 |
| 当前 Zamak Canvas + Pango/GTK | Zamak `std/ui`、libui-ng、GTK3、Pango/Cairo | 固定尺寸客户区自绘和当前内存采样集成 | 继续保留运行基线和已通过几何；如要补字体 family 控制，先用单页 Pango/Qt 双语实测，不把自绘文字 API当成系统原生 Win32 控件。 |

Qt 官方建议只改少量原生样式时用 `QProxyStyle`，完整自定义时从 `QCommonStyle` / `QStyle` 实现；样式表适用于快速试验，不适合大量生产控件状态。若产品将像素 1:1 作为硬验收，优先先验证 Winelib 能否复用资源和控件行为；若目标必须是纯 Linux 原生应用，则选 Qt Widgets 自定义 style 作为前端候选，并让 Zamak/现有 C 内存模块继续作为唯一后端，避免重复实现解析和安全策略。两条路线都必须用同一 Windows 来源、分辨率、DPI、页面 ID、状态与语言作 A/B 测试。

### 本轮可验证范围

- Zamak 主程序增加 `Cached` 与 `SReclaimable` 的独立数据字段，并验证 `/proc/meminfo` 单位；Production UI 和安全 helper 均通过本地构建链。
- 独立 Canvas 探针在 Zamak 0.2.0 下为 `12/12` PASS。真实 Wayland 输入、Xvfb/Openbox 与 Weston 在本机本轮没有运行；没有将之前 runner 或旧截图算成本轮输入测试。
- 本机有真实 Hyprland 环境变量，但本项目全局桌面安全规则禁止本轮查询合成器 IPC、注入输入或启动一个会覆盖用户桌面的 UI 进程。故本轮 **REAL_HYPRLAND_INTERACTION_NOT_RUN**；此前隔离候选截图仍仅作历史几何参考。

主要来源：

- [Mem Reduct v3.5.2 `resource.rc`](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/src/resource.rc)
- [Microsoft DIALOGEX resource](https://learn.microsoft.com/en-us/windows/win32/menurc/dialogex-resource)、[MapDialogRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapdialogrect)、[MS Shell Dlg mapping](https://learn.microsoft.com/en-us/windows/win32/intl/using-ms-shell-dlg-and-ms-shell-dlg-2)
- [Microsoft font redistribution FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq)、[Wine Tahoma source and embedded LGPL notice](https://github.com/wine-mirror/wine/blob/8be6c6f4283aabf9f9248f1242e8cd984757d03e/fonts/tahoma.sfd)
- [Qt 6.12 Styling Approaches](https://doc.qt.io/qt-6/qtwidgets-styling-approaches.html)、[QProxyStyle](https://doc.qt.io/qt-6/qproxystyle.html)
- [Wine 11.0 release notes](https://list.winehq.org/hyperkitty/list/wine-releases@list.winehq.org/message/UL6L2GJ55VYUJ5KUMBZ3TZSXRFJ52QG6/)、[Wine Wayland driver development](https://www.collabora.com/news-and-blog/news-and-events/a-wayland-driver-for-wine.html)

## 1. 核验环境和可复现材料

- Toolchain: Zamak 0.2.0。
- R1 曾在本地 Wayland/GTK 环境验证 Canvas 绘制，但旧探针尺寸被平铺，不作为窗口几何或产品截图证据。其本机截图和绝对路径不纳入此 PUBLIC 规格 PR。
- 当前隔离 CI 探针源码由 [Draft PR #4](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/4) 跟踪；fixture SHA-256 为 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。CI 实测的探针是 top-level 336×348、Canvas 312×324；仅为独立测试夹具，不是 DLU 实测换算或生产窗口尺寸。最终 PR head 为 `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1`；GUI PR run `40192531809`、build PR run `40192531948` 及对应 push runs `40192528601`/`40192528611` 均 PASS。

## 2. 技术路径比较

| 路径 | 可验证能力 | 1:1 风险/限制 | P2A 结论 |
|---|---|---|---|
| std/ui 原生控件 | Window、Label、Button、Choice、Spinbox、Checkbox、Group、布局组件 | 由 GTK/libui-ng/Pango 样式、字体度量、主题与 compositor 控制；默认边框和间距不等同 Win32；菜单树/窗口装饰受平台影响 | 适合对话框和系统整合；主窗精确控件不能直接以默认样式验收 |
| Zamak Canvas/Painter | 原生窗口客户区内路径、矩形、线、颜色、文字、裁剪和变换；on_draw/on_mouse/on_key 回调；p.width/height 可读；点状态后 redraw | Painter.text 接受字号/粗体/颜色，但无 font-family 参数；文字走系统字体；键盘事件要求 Canvas 先获得焦点；无原生无障碍控件语义 | 可控绘制客户区几何，具备做主窗口像素布局的技术基础；尚未证明最终2px精度 |
| Canvas 自绘菜单 | 可绘菜单行、勾选、禁用/高亮和鼠标命中区域 | 必须自己做键盘导航、焦点/esc、快捷键、屏幕阅读器和子菜单边界；不能假装系统菜单行为 | 可实现，但要独立验收完整交互与可访问性 |
| Canvas split button | 主区与箭头可用独立矩形命中并分别绘制 pressed/hover 状态 | std/ui Button 未提供 Win32 BS_SPLITBUTTON 同款控件；R5 Xvfb/XTEST 已通过实际事件验证探针主区和箭头路由，R6 补充 pressed/release ROI 与几何断言 | 技术路径可行；仍不表示正式主窗口已实现 |
| 最小 C/GTK 桥 | 可补 Pango 字体族、GTK窗口属性、菜单/快捷键、通知与特定桌面接口 | 增加平台特定代码，需清晰 FFI 和生命周期边界；Wayland会限制全局状态 | 仅对 Canvas 缺口做小桥接评估，不把整个 UI 改成 C |
| StatusNotifierItem | 外部桌面协议可提供 tray item/menu；机器上存在 Ayatana AppIndicator 库 | std/ui 无直接 tray API；当前 D-Bus 会话访问返回 credential error；Hyprland session 的 watcher、右键/中键/通知均未证明 | 只记录为候选，GTK/DBus/SNI 实机验收 NOT RUN |
| HiDPI/坐标 | Canvas 用当前绘图区宽高；当前显示 scale=1 | 未核实 2× scale 的 logical/physical conversion；本探针请求338×395，Hyprland平铺到941×1030，故固定窗口尺寸没有被实测 | 需要在浮动/可控客户区窗口、scale1/2条件下建立采集用例 |
| 字体/图标 | 系统字体能绘出双语字符；颜色和线宽按 Canvas API 绘制 | 当前截图无Windows同条件源；不能量化抗锯齿、字宽、基线和图标像素差。Ms Shell Dlg 在 Linux 没有一对一映射 | 文本位置可调，glyph raster 需单独量化；不可仅靠Canvas编译宣称视觉等价 |

## 3. 已做探针与结果边界

早期 R1 探针请求 338×395，绘制菜单文字、双语统计占位文案、分隔线和 split button；数值使用破折号，未连接系统操作。它只证明绘制路径可用。该尺寸来自用户参考图猜测且被 compositor 平铺，已被后续 3.5.2 DLU 基准纠正。生产主窗必须保持官方紧凑尺寸：`IDD_MAIN=224×214 DLU`；每个 `IDD_SETTINGS_*` 的 `260×168 DLU` 是子页面模板，不是含导航/底部按钮/边框的整个设置窗口尺寸。

R6 的 GitHub-hosted Xvfb/XTEST 测试以 336×348 顶层、312×324 GTK Canvas 实际注入鼠标/键盘并验证菜单、状态、locale、窗口 resize 与关闭。按钮 ROI 的 hover-only→held-press 和 pressed→release 像素差分别为 8004。WM close 使用隔离 Openbox + XTEST Alt+F4，日志包含 `CI_WINDOW_CLOSE` 与 `CI_APP_RETURNED`，进程退出码 0 且窗口消失。Weston Headless/Pixman 验证 Wayland socket、window/draw 和事件循环退出，但没有 `wl_seat`；因此不代表 Wayland 鼠标/键盘验收。每次客户端日志可见一次 `gdk_seat_get_keyboard` critical；与缺少输入 seat 的相关性尚未证明因果。

## 4. 对 P2 实施的技术门槛

1. 生产候选可先按源码 DLU 建立集中、可替换的临时转换参数，保留官方紧凑比例；待取得真实 Windows v3.5.2 源图和采集 metadata 后，再转换为冻结 DPI/字体下的像素参考。候选开发不代表最终像素验收。
2. 主客户区采用 Canvas 或窄 GTK 自绘桥；不以 Gtk 默认控件外观声称 Win32 复刻。标题栏由 Hyprland 管理，作为系统差异单列。
3. 以一份命中区域模型同时驱动绘制与鼠标判定；为 split button 做 hover、pressed、键盘和禁用状态测试。
4. 选择 Linux 字体并测量字宽、基线、中文字体 fallback；Canvas API 当前没有显式 font-family 参数，如差异超过标准则调查小型 Pango C bridge。
5. 对菜单逐项验证鼠标、键盘焦点、方向键、Enter、Escape、语言切换、勾选/禁用、可访问性。
6. StatusNotifierItem 只有在 Hyprland 实际 watcher 下验证注册、图标变化、右键菜单、单击、中键、通知之后才可标为可用。
7. 屏幕缩放至少 scale 1 和 scale 2；窗口尺寸要真实固定为客户区像素，另报告server decoration，不修改长期桌面配置。
8. 不把原版清理选项接入真实 Linux写操作；无特权探针和 mock only。

P2A技术结论：Canvas 是合理的主窗口绘制候选；原生 Zamak/GTK 负责窗口生命周期和系统整合，最小桥只补字体/SNI/窗口属性缺口。几何像素准确性与完整操作可行性仍待真实源图和交互验证。完整流程见 UI_FIDELITY_TEST_PLAN.md。

## 2026-10-29：路线复核与并行实现

### “窗口太大”与当前实测范围

- R6 候选修复了 Hyprland 平铺时窗口可被拉大的问题：GTK 主窗和设置窗固定尺寸，不再随平铺分配拉伸 Canvas；主窗移除多余的 GTK 内容内缩。修复后的有界 Wayland 运行主窗退出码为 0，没有 GTK warning/critical。主窗 `337×369` 是 GTK 客户区，设置页客户区 `558×335` 与已固定的 Windows 3.5.2 基准相同；不应为了视觉印象缩小设置页。
- 该 Linux 主窗不是可直接声称 1:1 的配对：Windows 参考记录的是 96 DPI 下完整外框 `352×407`，Linux 记录的是客户区；旧 changed-pixel `20.9436%` 还含动态内存值、Linux 不支持的 System working set、安全禁用按钮和不同字体栅格，不能用作有效通过率。
- 当前命令环境观测到 `XDG_SESSION_TYPE=wayland`、`WAYLAND_DISPLAY=wayland-1`、`GDK_SCALE=2`；已存的 R6 运行日志报告 GTK scale factor 1，并且 `GDK_SCALE=1/2` 两次启动得到相同 GTK 因子与逻辑尺寸。没有测量 compositor 装饰框的物理像素，故不能把“大窗口”归咎于某一个缩放变量，也没有读取或修改 Hyprland 配置。
- 仍然确定的问题：设置页同源文本只有 `25/120` 的宽高差在 2 px 内，`95/120` 超出；深色有效配对 changed-pixel `5.4004%–12.6625%`；12 项上游简中来源差异保持 `DECISION_REQUIRED`；整页 1:1 仍 **NOT PASS**。设置矩形 `184/184` PASS 只证明几何。

### 其他项目采用的技术路线以及本项目结论

Wine 官方 Winelib 指南明确将它定位为“在 Unix 上重新编译 Windows 应用”的开发工具：可复用 Win32 API 实现、资源和消息文件，并用 `winegcc` 与 `wrc` 编译；复杂源代码可能仍需移植修改。原版 Mem Reduct 3.5.2 是 C + Win32 `.rc`/`DIALOGEX`，所以 Winelib 能直接保留原控件树、资源 ID、菜单和对话框模板，是当前最有希望减少重复重画工作的第三方路线。它不是 Microsoft Windows：Wine 的 GDI、主题控件、字体栅格和窗口管理仍可能有差异，不能承诺像素 1:1。

Qt 6 Widgets 是第二路线：官方将 `QProxyStyle` 用于局部覆写绘制/度量，将 `QCommonStyle` 用于完整样式。当前 Qt 6.12 单页 POC 仍有 `12.7155%` English / `11.9740%` 简中 changed-pixel；加入可再分发的 Wine Tahoma 字体后分别为 `12.7256%` / `11.9820%`，变化更差。因此不迁移到默认 Qt 控件，也不把已有字体 POC 结果包装成改善。

当前 GTK3/Pango/Cairo 是已构建的运行基线；`GtkStyleContext` 会合并应用和主题 provider，GTK 文档还说明用户级 CSS 优先级可盖过应用级样式。加一段 CSS 不能重建 Windows GDI 字体/原生控件。Zamak `Painter.text` 没有 font-family 参数；任何字体改动都必须先通过 Pango 实际解析、120 项字符串度量、英中截图和原控件状态比较。

截至 2026-10-29，WineHQ 最新开发版本是 11.20；Wine 9.22 的官方发布说明称 Wayland driver 已进入默认配置。WineHQ Bugzilla 60152 中 Wine 11.15 的启动回归由上游于 11.16 标记修复，因此不能继续把 11.20 的 Wayland driver 泛称为“仍属 experimental”。本机 Arch 仓库查询到 `wine 11.20-1`，依赖项包含 Wayland；但本机未安装 Wine/Winelib、Xvfb 或 Weston，尚未运行 Winelib POC。POC 仍须记录实际加载的 driver、客户端日志、DPI/缩放、焦点和键鼠结果。

## 2026-10-29 用户反馈后的问题清单与实施路线

用户指出窗口观感过大、部分位置不齐，并要求停止以局部几何通过替代整体 1:1。复核固定的 v3.5.2 证据后，当前状态如下：

| 区域 | 可复核结果 | 仍存在的问题 |
|---|---|---|
| 设置页尺寸/控件框 | 客户区 558×335；184/184 控件矩形在 2 px 内，最大单分量 1.5 px；导航 10/10、树线 102/102、离散栅格采样 36/36 | 外框和局部样本通过不能证明字体、控件皮肤和整页相同。 |
| 设置页文字 | 同源英文/简中 120 项里仅 25 项宽高差≤2 px；95 项仍超 2 px。Windows 请求 `MS Shell Dlg 2`、8.25 pt，GDI ascent/descent 11/2 px；Linux Pango `Sans` 8 pt，设置页 ascent/descent 10/3 px | 字宽、基线、字形轮廓、抗锯齿及 CJK fallback 均有差异。Liberation Sans 与 Wine Tahoma 候选都没有在全页比较中改善到足以采用。 |
| 设置页整页像素 | 20/20 主题/语言/页面语义状态有效；深色 changed-pixel 为 5.4004%–12.6625%，浅色为 5.0741%–14.8853% | 五页全都不是 1:1；这些百分比是差异指标，不是通过率。 |
| 原版简中内容 | 12 项来源文案保持 `DECISION_REQUIRED` | 文字内容不同的像素变化不应归因于字体，也不能未经所有者决定改译文。 |
| 主窗口 | GTK 客户区日志为 337×369，尺度因子 1；GDK_SCALE=1/2 两次旧截图分辨率相同。Windows 主窗图的截图边界和 Linux app-owned surface 不是同一种窗口边界 | 历史 20.9436% 图像差异不是有效配对。未测到含 Hyprland 装饰的物理外框，因此用户感受到的“大”目前没有足够证据归因于具体 scale/decoration；不能假装已排除或擅自缩小官方 DLU 尺寸。 |
| 主窗口渲染/交互 | 已修复启动时错误的 File 焦点框；GTK 实际 Wayland 有界启动与退出为 0 | 与 Win32/GDI 相比仍有字体、菜单/按钮 raster、标题装饰差异。真实 Hyprland 键鼠、主窗缩放及完整页面现场交互未验收。 |

### 选择的开发路线

1. **前端不再围绕当前 Canvas 继续猜字体/重画标准控件。**当前栈 `Zamak std/ui → libui-ng → GTK3/Pango/Cairo` 适合紧凑的 Linux 本地窗口，但标准控件和 GDI 文字不是同一实现。Qt `QProxyStyle` 是成熟的逐控件自绘路线，不过已有 Qt 6.12 单页和 Wine Tahoma 字体候选未降低整页 changed-pixel；不能因为换成 Qt 就宣称接近 1:1。
2. **下一次前端实施应先验证 Winelib，而不是再加一个平行 Qt 演示。**用固定上游 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 的 `resource.rc` / `DIALOGEX`、原 resource ID 和标准 Win32 控件，由 `wrc` 编译资源、`winegcc` 编译一页 General 设置原型。调用 Wine `USER32/GDI/COMCTL32`，先只比较一页、单语、96 DPI 的正常/禁用/焦点状态；通过后再扩展五页和 192 DPI。Wine 本机运行时已确认可从 Arch 仓库获取 `11.20-1`，但当前环境未安装，POC 状态仍 NOT RUN。
3. **窗口尺寸先用物理证据校准，不手动缩小。**比较 Windows `DIALOGEX` 在 96/192 DPI 下真实客户区，与 GTK/Winelib 自己报告的 client allocation、Wayland buffer scale 和截图边界；单独记录 compositor decorations。旧 `GDK_SCALE=1/2` 截图都为 337×369，证明只改环境变量没有解决观感差异。
4. **内存核心与 UI 并行，但通过稳定接口隔离。**已完成 `/proc/meminfo` 的 kB 单位校验和 Cached/SReclaimable 分离；C helper 限定单个 `pagecache` enum，采用固定生产路径并对短写返回“不确定”；Python 测试使用临时普通文件与错误注入。当前 GUI 不调用 helper，内存清理、Polkit 与 Swap/ZRAM 仍未启用。后续前端只调用读数/结果接口，不能重复解析 `/proc`、执行 shell 或提升整个 GUI。

### POC 验收门槛

- 使用官方 `resource.rc` 和同一个 v3.5.2 Windows 基准；按页面 ID、语言、主题、选中/禁用/焦点状态配对。
- 先做 General 页小原型，输出窗口/控件 rect、字体解析结果、字宽/基线和 overlay/difference/heatmap；客户区外框须包括 client 与 decoration 的分离测量。
- 同时测试 Wine Wayland 与 XWayland，记录 Wine 11.20 实际 driver、输入、关闭重开、字体映射、GTK/Wayland 日志及打包增量。
- 不带入 Windows 专有字体文件；Wine Tahoma 等候选须遵守上游许可证，并实测中英 fallback。
- 只有 Winelib 原型的同状态整页结果显著优于当前 GTK/Canvas，且运行/打包/交互门槛通过，才逐页迁移。否则保留现有生产分支并继续拆解实测差异，不把 1:1 写成已完成。
- 内存核心继续独立验证；真实清理、Polkit、自动模式、Swap/ZRAM 和 root GUI 需要单独产品/安全批准与真实测试环境授权。

### 按两条线并行推进

1. **界面线：**先用官方 `resource.rc` 做一个设置 General 页 Winelib POC，保留原始 resource ID 和控件状态；同条件再与 Qt `QProxyStyle` 版本比较。门槛包括英/简中、96/192 DPI、禁用/焦点状态、字体映射、Wayland/XWayland driver、打包体积、无障碍和窗口关闭。达到门槛前保留现有 GTK 生产路径；不得将试验控件混入已通过的 PR #7。
2. **内存线：**保留 Zamak `/proc/meminfo` 采样与语义映射；C helper 只接受固定 `pagecache`，测试以临时文件注入错误。核心可独立做类型化结果和 mock 生命周期，不需要等 UI 选型；GUI 继续禁用真实清理入口，直到 ADR、安全审查和另行批准通过。
3. **集成线：**UI 只展示采样器与类型化操作结果，不重复解析 `/proc`、不直接获得 root；后端没有通过安全审查前，前端也不暴露操作按钮。这样两条线可以同时开发，互不伪造验收。

### 本轮本地代码回归（无远端写入）

- `scripts/ci/build-production-gui.sh`：治理 34 项 PASS、C/Python 安全测试 11/11、Shell 4 项、Zamak 0.2.0 check PASS、主项目 43/43 PASS、生产 GUI/helper build PASS。
- `scripts/ci/build-canvas-probe.sh`：独立 Canvas 探针 12/12 PASS。
- `scripts/build-arch.sh`：Zamak check/test/native build PASS。安装器没有运行。
- 全部 shell 脚本语法和对 `origin/main` / 工作树的 `git diff --check` PASS。
- 本轮没有启动用户活动桌面窗口或注入键鼠；上述 R6 Wayland 截图是本地历史证据，不代表当前修改有新 GUI CI。PR #7 没有推送、改 Base、Ready 或合并；main 未修改。

主要资料：

- [WineHQ Winelib User’s Guide](https://dl.winehq.org/wine/docs/en/winelib-guide.html)
- [Wine 11.20 release](https://www.winehq.org/news/2026101601)、[Wine 9.22 Wayland driver default configuration](https://www.winehq.org/news/2024112201)、[WineHQ Bugzilla 60152](https://bugs.winehq.org/show_bug.cgi?id=60152)
- [Qt QProxyStyle](https://doc.qt.io/qt-6/qproxystyle.html)、[Qt styling approaches](https://doc.qt.io/qt-6/qtwidgets-styling-approaches.html)
- [GTK3 GtkStyleContext](https://docs.gtk.org/gtk3/class.StyleContext.html)、[Mem Reduct v3.5.2 resource.rc](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/src/resource.rc)

## 2026-10-29 R6 Winelib POC 结果复核与实际前端缺陷修正

### 目前界面的具体问题

- 设置页 120 条同源文字中只有 25 条宽高差在 2 px 内，95 条仍超过；整页浅色 changed-pixel 为 5.0741%–14.8853%，深色为 5.4004%–12.6625%。`184/184` 控件矩形通过不能代表字体、控件皮肤或整页一致。
- 主窗口配对仍受截图边界和状态影响：Linux 图不带 compositor 装饰，内存值实时变化，System working set 不可用，清理按钮保持禁用。旧 `20.9436%` 不是有效的 1:1 指标。主窗口文字/控件位置仍需按同状态截图逐项核对。
- 生产 GTK 设置页里很多尚未实现的选项确实呈禁用灰度；它们目前没有对应的 Linux 功能。把它们涂成启用色会误导用户，不能为了像素率这样做。

### Winelib 原资源小样的实测结果

固定上游 `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 的资源子集、Winelib C/RC POC、私有 Wine prefix、Xvfb 截图和比较 CSV 已在隔离研究 artifact 中。上游 `resource.rc` SHA-256 为 `8cfe7e4494e50e45a6b1a61c5bda4ec7bb01c7b46359530af2d99f7f6c9e2356`，`resource.h` 为 `de019ba1356cc312d26e1c0fb2aa7cd2c07425d096e3ab3e17d8303e109657ad`。这是 General 设置页壳，不是完整原版移植，也没有接内存 helper。

R6 相对 R5 修正了两项状态错配：背景采样改为 Windows 的 RGB 240，导航焦点放到同一复选框。修正后 General 页 558×335 客户区 changed-pixel 是英文 `13.2531%`、简中 `12.2725%`，背景样本为 `0/2500` 像素不同。R5 的英文 `79.8711%` / 简中 `79.4244%` 混有背景和焦点状态错误，不能作为工具改进率。R6 仍有 GroupBox `21.7213%/20.5864%`、按钮 `37.2570%/37.6336%` 的局部差异。其控件状态与生产 GTK 图不同，不用这些整页数字做工具包公平排名。

Winelib 英文 `WM_GETFONT` 返回逻辑别名 `Ms Shell Dlg`，TEXTMETRIC 高度/上升/下降为 `13/11/2 px`，与 Windows GDI 记录的垂直度量 `13/11/2 px` 对齐；这不表示 Wine 使用了 Windows GDI 或相同字形。简中使用 Noto Sans CJK SC 才能正常显示，度量为 `16/13/3 px`；移除 CJK fallback 会出现方框缺字。Wine 的 Common Controls v6 manifest 日志报告 `Microsoft.Windows.Common-Controls 6.0.0.0` assembly 缺失；按钮、GroupBox、字宽和抗锯齿仍有差异。Windows 专有字体不打包。

### 技术路线决定

- 如果首要目标是复用原版对话框资源和控件树，`winegcc` + `wrc` + USER32/GDI/COMCTL32 是最值得继续验证的第三方路线；它能复用布局资源，不会自动提供 Windows 的像素渲染。
- Qt 6 Widgets + `QProxyStyle` 提供标准控件、焦点和无障碍接口；已有 General 页和 Wine Tahoma 候选约 12% changed-pixel，没有证据支持仅切换工具包就达到 1:1。GTK/libui-ng/Pango 当前生产基线构建稳定，但 Painter 字体族控制有限。
- 本轮没有把 Winelib/Qt 小样冒充成生产前端。下一步应使用同页面 ID、文本、输入状态和 DPI 扩展 Winelib 五页样本，先定位 Common Controls 与字体差异，再决定是否迁移。当前机器没有 Wine、winegcc、wrc 或 Xvfb，本轮没有重跑桌面小样或安装系统依赖。

### 本轮实际修复与内存核心并行验证

- 修复主窗口语言/主题菜单更新时没有通知已打开设置窗重绘的问题。Dark、English、简中变化现在会重绘第二窗口，并新增 Zamak 状态断言。
- 加入 fake `pkexec` 后端退出状态回归：失败码 `1`、不确定写入码 `3` 与一般子进程状态 `126` 由异步桥原样返回，三种情况均只启动一次。没有运行真实 Polkit 或写入内核。
- 最新本机回归：治理 34 项、C/Python 12/12、Zamak 44/44、生产 GUI/helper build PASS；Canvas 探针 12/12、Shell syntax、`git diff --check` PASS。Xvfb/Openbox/Weston/Wine 当前缺失，因此本轮隔离 GUI 复测 `NOT RUN`；远端 PR #7 未改，没有新 GitHub Actions。

R6 的并排图、Overlay、Difference、Heatmap、比较 CSV、字体输出和 PNG SHA-256 保存在独立研究 artifact，原图未覆盖。整页 1:1 仍 **NOT PASS**；真实 Wayland 键鼠仍 **NOT RUN**。
