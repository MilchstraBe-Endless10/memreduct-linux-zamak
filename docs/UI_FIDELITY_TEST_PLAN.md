# UI 高保真与交互回归计划（P2A-R1）

状态：源码规格与截图清单固定到官方 Mem Reduct v.3.5.2 tag；真实 Windows 源图采集 BLOCKED，Windows/Linux 对照与像素差异 NOT RUN。该缺口阻止像素验收，但不阻止本源码规格 Draft PR 接受复审；本 PR 不包含 P2B UI 实现。

## 1. 冻结基准与不变量

- 唯一行为/资源基准：Henry++ Mem Reduct v.3.5.2，commit 2f5c55d15f69a2c88859e036b16c6f4c971e1968。
- commit 04402de8b6a8ff9d1c5517c675d87c346655323c 是 tag 后 46 个提交的开发快照，不是发布版依据；其行为差异单列于 UPSTREAM_VERSION_DRIFT_3.5.2_vs_04402de8.md。
- 主窗口和设置页资源尺寸为 DLU；先记录字体、DPI 与窗口客户区，再做同尺寸比较。Linux 主窗与设置窗口须分别遵循官方资源模板的紧凑尺寸，不得因为跨平台实现而扩大为大窗口；336×348 CI 探针尺寸不用于产品 GUI。
- P2 目标界面需完整支持 English 和简体中文。逐项核实翻译键；3.5.2 未翻译的 hard-coded strings 要在 Linux 项目双语资源里补齐，且标注为项目补译。
- 每个原版动作必须映射成真实、可解释且安全的 Linux 动作，或明确禁用。不允许用 drop_caches 假扮工作集、修改页列表、待机列表或 Registry cache。
- 只有源图和目标图均为对应版本的真实截图、环境匹配且做过叠加/差异计算，才能报告像素验收。2 px 是目标阈值，不是当前结果。

## 2. 截图证据记录模板

每个原图必须记录：原版 app version、release executable 来源及 SHA-256、Windows 版本/build、分辨率、DPI 与 scale、主题、语言、客户区宽高、字体/字体平滑、采集日期和采集方式。原始文件不裁剪、不重采样、不覆盖；处理图另存，源文件只读。

| Capture ID | 页面/状态 | English | 简体中文 | Light/Dark | Windows source status |
|---|---|---|---|---|---|
| W-MAIN-EN-L / W-MAIN-EN-D | 主窗口空闲 | 必须 | — | 浅/暗 | BLOCKED |
| W-MAIN-ZH-L / W-MAIN-ZH-D | 主窗口空闲 | — | 必须 | 浅/暗 | BLOCKED |
| W-SET-GEN-EN / ZH | 常规页 | 必须 | 必须 | 原版支持主题分别记录 | BLOCKED |
| W-SET-MEM-EN / ZH | 内存清理页 | 必须 | 必须 | 同上 | BLOCKED |
| W-SET-APP-EN / ZH | 外观页 | 必须 | 必须 | 同上 | BLOCKED |
| W-SET-TRAY-EN / ZH | 托盘图标页 | 必须 | 必须 | 同上 | BLOCKED |
| W-SET-ADV-EN / ZH | 高级页 | 必须 | 必须 | 同上 | BLOCKED |
| W-MENU-ROOT / W-MENU-SUB | File/View/Settings/Help、Language、八项 split dropdown | 双语 | 双语 | 至少核对可用主题 | BLOCKED |
| W-TRAY-ROOT / W-TRAY-REGION | Tray 根菜单与清理区域子菜单 | 双语 | 双语 | 桌面状态记录 | BLOCKED |
| W-TRAY-LIMIT / W-TRAY-INTERVAL | 阈值/间隔子菜单、当前选中态、Disable | 双语 | 双语 | 桌面状态记录 | BLOCKED |
| W-CLEAN-CONFIRM / CANCEL | 清理确认、Do not ask again 与取消路径 | 双语 | 双语 | 浅/暗 | BLOCKED |
| W-CLEAN-RESULT / ERROR / DENIED | 清理结果、失败、权限拒绝状态 | 双语 | 双语 | 浅/暗 | BLOCKED |
| W-RESET / ABOUT / UPDATE | 重置确认、About、无更新/有更新/失败 | 双语 | 双语 | 浅/暗 | BLOCKED |

当前公开索引只包含官方仓库示例 PNG；它不是运行截图。用户参考图片未复制到 PUBLIC 仓库。上述所有真实 Windows capture 字段尚未填入。

## 3. Linux 目标采集

每次 Linux 采集记录：

- Omarchy/Arch 版本、Hyprland 版本、会话类型，监视器像素分辨率与 scale。
- Zamak 版本、commit、构建散列、进程 UID、GTK 与字体配置。
- English / 简体中文、浅/暗设置、实际客户区尺寸、窗口是否浮动或平铺。
- 原始 PNG、SHA-256、日期、抓图工具与命令；不得把含其他窗口/隐私的桌面全屏截图公开。

P2A-R1 的独立技术探针曾请求 338×395，活动桌面将其铺到 941×1030；该历史图像不作为公开 UI 资产或尺寸基准。后续隔离 CI 探针按官方 224×214 DLU 与 96-DPI base-unit 估算请求紧凑的 336×348 窗口，实测 GTK Canvas 为 312×324。该估算仍不是 Windows 实测换算值。R5 的 Xvfb/Wayland 证据位于 [GitHub Actions run 40187424007](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40187424007)（artifact 保留 14 天），只显示模拟占位 UI。

CI 探针不读取 `/proc`、不调用 helper/Polkit、不执行清理、不修改 Swap/ZRAM。X11/XTEST 输入测试不能代表 Wayland/Hyprland 输入；Wayland Headless 只验证私有 socket、窗口创建、Canvas draw 和事件循环退出。StatusNotifierItem/SNI 仍需目标桌面实测。

## 4. 对照和交互验收步骤

1. 在干净 Windows 虚拟机或实体机安装官方签名/哈希验证的 3.5.2 release；记录 executable SHA 与 Windows 构建。按上表逐条捕获真实 UI；结果/拒绝场景只用可控隔离环境，不做主机破坏性清理。
2. 固定 Windows scale/DPI、分辨率、主题、语言、字体平滑；另记录主窗口客户区像素。五个设置页分别按其 260×168 DLU 模板记录换算结果，不能用主窗口或 CI 探针大小替代。
3. 在 Linux 目标窗口固定客户区尺寸和同语言/主题；确认 Wayland 原生 surface，不从 XWayland 截图冒充。
4. 保存源图原件及散列。将副本裁剪到同一客户区，不缩放；生成 50% alpha overlay 与 absolute difference；记录命令、工具版本、输出散列。
5. 对每个 resource ID 测试坐标与状态；每个 visible control 的 x/y/w/h 最大误差 ≤2 px。字体抗锯齿、字体回退、窗口装饰分别报告。
6. 重跑英文/简中；检查主菜单、五页设置、确认框、通知、更新/错误、托盘菜单不混语、不截断。
7. 用 mock/dry-run 验证确认取消、Polkit 取消/拒绝、helper missing/nonzero、成功状态和日志；不写真实 drop_caches，不动 Swap/ZRAM、不装 root 服务。
8. 验证主菜单、split-button 主区/箭头、八个禁用/支持态、F1/F2/Alt+F4、Enter/Escape（待 Windows 实机确认）、重置、设置持久化、托盘点击/中键/键盘及通知。
9. 在 scale 1 与更高 DPI/scale 各跑一次。Wayland 下 keep-above、startup、minimize 和 SNI 中键能力按实际 compositor 记录，不虚构成功。

## 5. 用例与当前状态

| Test ID | 内容 | 当前结果 | 证据/限制 |
|---|---|---|---|
| SRC-01 | 官方 v.3.5.2 tag SHA / VERSION / app.h | PASS | tag/ref API 与两个版本文件一致 |
| LOC-01 | 82 活动 IDS 与 Chinese.ini 键审计 | PASS | 活动字符串 82/82 有简中键；80 个实际翻译，2 个仍为英文；另查到 resource.h 的 6 个未使用 IDS 宏（45–50），不计入活动键；10 个运行时 hard-coded 英文另列 |
| SRC-02 | 与后续 04402 快照差异审计 | PASS（摘要） | 46 commits；精确文件项见差异报告 |
| WIN-SHOT-ALL | Windows 正版截图采集 | BLOCKED | 当前主机无 Windows/Wine；现有图片都缺真实采集元数据 |
| WAYLAND-POC-CHECK | Canvas probe zam check | PASS | CHECK OK p2a_canvas_probe |
| WAYLAND-POC-BUILD | Canvas probe zam build | PASS | 输出 SHA 2030151c58b6cdb1ccc9ef35e9f931f2831822a8fc4b8cba6cc6086881d495e8 |
| WAYLAND-POC-RENDER | Canvas app 在 Hyprland/Wayland显示 | PASS（仅绘制） | xwayland=false，截图已保存；compositor 平铺为941×1030 |
| CI-X11-INPUT | 隔离 Xvfb/XTEST hover、按下/释放 ROI、按钮/箭头命中、菜单、Tab/Enter/Escape、双语与 resize | PASS（探针级） | PR run `40191456330` 和 push run `40191454446` 均通过；hover-only→held-press ROI 改变 8004 像素，pressed→release 也为 8004；不代表产品 UI 或 Wayland 输入 |
| CI-X11-CLOSE | 独立 GTK 窗口收到真实 WM close，回调与进程退出 | PASS（X11 探针级） | 隔离 Xvfb + Openbox 下 XTEST Alt+F4；出现 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`、退出码 0、窗口消失。`xdotool windowclose` 走 `XDestroyWindow`，不适合作为 GTK WM_DELETE 验收 |
| CI-X11-GEOMETRY | 336×348 top-level、312×324 Canvas、命中区域/弹出菜单、双语布局及 resize | PASS（探针级） | 固定 GTK 测试夹具的逻辑尺寸，不是 Windows 3.5.2 像素尺寸；生产窗口仍按 224×214 DLU 和 260×168 DLU 模板换算 |
| CI-WAYLAND-LIFECYCLE | Weston Headless/Pixman socket、GTK Canvas 创建绘制、事件循环退出 | PASS（生命周期） | PR run `40191456330` 与 push run `40191454446` 均通过；一次 `gdk_seat_get_keyboard` critical，Weston 未公告 `wl_seat`。关系符合 headless 输入 seat 缺失的可能解释，但因果未证实；未屏蔽诊断 |
| WAYLAND-POC-INPUT | 在 Wayland/Hyprland 中对生产窗口注入并验证真实鼠标/键盘事件 | NOT RUN | Weston Headless 没有输入 seat；X11/XTEST 结果不能替代 Wayland/Hyprland 实测 |
| GTK-SNI | Tray 注册、菜单、中键、通知 | NOT RUN | Ayatana 库安装版本已读；当前 session D-Bus 无法从 shell 验证 |
| PIXEL-OVERLAY | Windows/Linus overlay/diff、2 px 几何 | NOT RUN | 没有可信 Windows 源图，也没有实现后的目标窗口 |
| DPI-2X | 高 scale/HiDPI 复测 | NOT RUN | 本轮只记录scale 1 |
| PRIV-CLEANUP | 真实清理、权限调用、Swap/ZRAM | NOT RUN（按安全要求） | 无任何真实特权写操作 |

## 6. 报告格式

每个 case 记录：ID；上游 resource ID/函数；版本；语言/主题；OS/分辨率/DPI/scale；客户区尺寸；源/目标图片名与 SHA；最大几何偏差；overlay/diff 路径；鼠标/键盘结果；是否仅 mock；PASS/FAIL/BLOCKED/NOT RUN；复现步骤。

有任一必需 Windows 源图缺失、环境字段缺失、未形成 overlay/diff，整体像素验收就不能 PASS。P2A 当前规格完成不代表截图或 Linux UI 已验收。

## 7. P2A CI 证据边界

- R6 在 GitHub-hosted Ubuntu 24.04、Zamak 0.2.0 下保留原 9 项事件路由测试并新增 3 项紧凑探针测试，12/12 通过。Xvfb/XTEST 真实经过图形服务器发送鼠标和键盘事件，验证 hover-only 与 held-press 的同坐标按钮 ROI 差异、释放、按钮/下拉命中、菜单、双语、resize 和 WM close。
- WM close 使用隔离 Xvfb 中启动的 Openbox 与 XTEST Alt+F4；日志记录 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`，退出码为 0 且窗口消失。源码核查显示 `xdotool windowclose` 实际执行 `XDestroyWindow`，属于直接销毁 X 窗口，不是正常 WM_DELETE 请求，因此不用于通过验收。
- Weston Headless/Pixman 通过唯一私有 Wayland socket 建立 GTK Canvas 并绘制 312×324，测试退出路径返回且进程退出码 0。每次日志有一次 `gdk_seat_get_keyboard` critical，Weston 未公告 `wl_seat`。两者相符但不证明根因；没有隐藏 GTK diagnostics。该 lane 不测试鼠标/键盘输入。
- R6 证据：PR run [40191456330](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456330)、push run [40191454446](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454446) 均成功并上传 artifacts；build-and-test PR/push runs [40191456339](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456339) / [40191454361](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454361) 均成功。探针源码 SHA-256 为 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。Artifact 保留期限 14 天；其中的 PNG 是模拟探针截图，不是原版或产品截图。
- CI 窗口 336×348（Canvas 312×324）仅是测试夹具；生产 GUI 必须保持官方紧凑资源尺寸按真实字体/DPI转换，不得放大。真实 Windows release 图、Linux 产品目标图、overlay/diff 和 2 px 几何验收仍 BLOCKED / NOT RUN。
