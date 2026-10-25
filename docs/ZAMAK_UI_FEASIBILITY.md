# Zamak 0.2.0 UI 精确绘制可行性记录（P2A-R1）

状态：Canvas 绘制和 Zamak 0.2.0 构建已验证；R6 隔离 Xvfb/XTEST 对探针完成 hover/pressed/release ROI、鼠标命中、菜单、键盘、双语、resize 与 WM close；Weston Headless/Pixman 验证 Wayland socket、GTK Canvas 绘制和正常退出。生产 UI、Windows 2 px 对照、真实 Hyprland 输入、托盘/SNI 与 HiDPI 仍未通过验收。测试探针不改生产主程序，不连接内存读取、helper、Polkit、drop_caches、Swap 或 ZRAM。

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
