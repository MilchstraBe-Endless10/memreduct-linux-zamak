# Windows Mem Reduct 3.5.2 UI 基准（P2A-R1 审阅稿）

状态：源码与资源清单固定到官方 v.3.5.2 tag；Windows 原生截图集仍为 BLOCKED，视觉差异测试 NOT RUN。本文件是可供 P2A 代码复审的规格，不表示像素验收通过。

## 1. 权威版本与来源

- 上游仓库：Henry++ Mem Reduct，https://github.com/henrypp/memreduct
- 官方发布标签：v.3.5.2
- 标签直接指向 commit：2f5c55d15f69a2c88859e036b16c6f4c971e1968（已通过 GitHub tag ref API 核对）。
- 固定后续对照快照：04402de8b6a8ff9d1c5517c675d87c346655323c。GitHub compare 确认它在 3.5.2 标签之后 46 个提交，不能作为 3.5.2 发布版行为依据。
- 官方标签的 src/app.h 与 VERSION 均为 3.5.2；官方 v.3.5.2 release 发布于 2025-04-01，包含 setup、portable、PDB、签名和 SHA-256 清单资产。
- 后续快照的 src/app.h 已是 3.5.3，但 VERSION 文件仍写 3.5.2。这是后续开发树的版本元数据漂移，不是 3.5.2 发布版冲突。
- 英文默认资源位于 src/resource.rc 与少数 main.h/main.c 硬编码文本；官方标签没有 English.ini。简体中文资源位于 bin/i18n/Chinese (Simplified).ini。
- 逐文件变化、菜单/控件/事件影响及本次翻译审计见 [上游版本差异报告](UPSTREAM_VERSION_DRIFT_3.5.2_vs_04402de8.md)。

固定链接：

- [v.3.5.2 tag](https://github.com/henrypp/memreduct/tree/2f5c55d15f69a2c88859e036b16c6f4c971e1968)
- [VERSION](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/VERSION)
- [src/app.h](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/src/app.h)
- [src/resource.rc](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/src/resource.rc)
- [src/resource.h](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/src/resource.h)
- [src/main.c](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/src/main.c)
- [src/main.h](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/src/main.h)
- [官方随仓库图片](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/images/memreduct.png)
- [简体中文资源](https://github.com/henrypp/memreduct/blob/2f5c55d15f69a2c88859e036b16c6f4c971e1968/bin/i18n/Chinese%20(Simplified).ini)

## 2. 简体中文/英文完整性审计

对官方标签中的 UTF-16 资源文件重新解析：

- resource.rc：82 个活动 IDS 字符串。resource.h 共定义 88 个 IDS 宏；ID 45–50 的 6 个旧宏无 RC 字符串、无简中键且全仓库代码无引用，不算活动文案。
- Chinese (Simplified).ini：82 个键，和活动资源键 82/82 对齐；缺键 0、额外键 0。
- 80/82 的中文值与英文资源值不同；以下 2 个键仍为英文：IDS_TRAYACTIONSC_HINT（Single click:）与 IDS_UPDATE_SPEED（Speed）。
- 另有 10 个运行时显示文本不属于这 82 个活动字符串：src/main.h 中 8 个内存区域名写死英文；SettingsProc 中 2 个高级页标签写死英文。3.5.2 简体中文运行时仍会显示这些英文文本。
- 用户补充的产品要求是 Linux 版支持完整简体中文和 English。Linux 资源需补齐上述缺口；规格中的原文栏区分“上游实际简中”与“项目建议翻译”，不能伪称上游 3.5.2 已完整翻译。

### 3.5.2 活动资源字符串逐键核对

下表为 resource.rc 英文原文与 Simplified Chinese.ini 实际值。上游简中值保持原样，包括尚未翻译的英文值。

| Resource key | ID | English resource | Simplified Chinese value |
| IDS_UPDATE_NO | 1 | No updates available. | 当前已是最新版本。 |
| IDS_UPDATE_YES | 2 | Update available, download and install them? | 有更新版本，是否下载并安装？ |
| IDS_FILE | 3 | File | 文件 |
| IDS_SETTINGS | 4 | Settings | 设置 |
| IDS_EXIT | 5 | Exit | 退出 |
| IDS_LANGUAGE | 6 | Language | 语言...... |
| IDS_HELP | 7 | Help | 帮助 |
| IDS_WEBSITE | 8 | Website | 项目网站 |
| IDS_CHECKUPDATES | 9 | Check for updates | 检查更新 |
| IDS_ABOUT | 10 | About | 关于...... |
| IDS_DONATE | 11 | Give thanks! | 捐赠...... |
| IDS_TRAY_SHOW | 12 | Show/Hide | 显示/隐藏 |
| IDS_TRAY_DISABLE | 13 | Disable | 禁用 |
| IDS_TRAY_POPUP_1 | 14 | Clean areas | 清理区域 |
| IDS_TRAY_POPUP_2 | 15 | Clean when above | 清理界限 |
| IDS_TRAY_POPUP_3 | 16 | Clean every | 清理间隔 |
| IDS_CLEAN | 17 | Clean memory | 清理内存 |
| IDS_GROUP_1 | 18 | Physical memory | 物理内存 |
| IDS_GROUP_2 | 19 | Pagefile | 虚拟内存 |
| IDS_GROUP_3 | 20 | System working set | 系统工作集 |
| IDS_ITEM_1 | 21 | Usage | 已用 |
| IDS_ITEM_2 | 22 | Available | 空闲 |
| IDS_ITEM_3 | 23 | Total available | 总计 |
| IDS_SETTINGS_GENERAL | 24 | General | 常规 |
| IDS_SETTINGS_MEMORY | 25 | Memory cleaning | 内存清理 |
| IDS_SETTINGS_APPEARANCE | 26 | Appearance | 外观 |
| IDS_SETTINGS_TRAY | 27 | Tray icon | 托盘图标 |
| IDS_CLOSE | 28 | Close | 关闭 |
| IDS_TITLE_1 | 29 | General configuration | 选项 |
| IDS_TITLE_2 | 30 | Language | 语言 |
| IDS_TITLE_3 | 31 | Memory regions to be cleaned | 要清理的内存区域 |
| IDS_TITLE_4 | 32 | Memory management | 自动管理内存 |
| IDS_TITLE_5 | 33 | Hotkeys (global) | 快捷键（通用） |
| IDS_TITLE_6 | 34 | Style | 个性化 |
| IDS_TITLE_7 | 35 | Color indication | 颜色指示 |
| IDS_TITLE_8 | 36 | Mouse control | 鼠标控制 |
| IDS_TITLE_9 | 37 | Balloon tips | 气泡提示（Windows 通知） |
| IDS_ALWAYSONTOP_CHK | 38 | Always on top | 窗口始终置顶 |
| IDS_LOADONSTARTUP_CHK | 39 | Load on system startup | 开机自启 |
| IDS_STARTMINIMIZED_CHK | 40 | Start minimized | 最小化启动（不显示窗口） |
| IDS_REDUCTCONFIRMATION_CHK | 41 | Confirm memory cleaning start | 在内存清理前手动确认 |
| IDS_SKIPUACWARNING_CHK | 42 | Skip "User Account Control" prompt warning | 跳过“用户账户控制（UAC）”提示 |
| IDS_CHECKUPDATES_CHK | 43 | Periodically check for updates (recommended) | 自动检查更新（推荐） |
| IDS_LANGUAGE_HINT | 44 | Select language: | 选择语言： |
| IDS_AUTOREDUCTENABLE_CHK | 51 | Clean when above: (%) | 占用达阈值时清理（%） |
| IDS_AUTOREDUCTINTERVALENABLE_CHK | 52 | Clean every: (min.) | 每隔指定时间清理（分钟） |
| IDS_HOTKEY_CLEAN_CHK | 53 | Clean memory: | 清理内存： |
| IDS_TRAYUSETRANSPARENCY_CHK | 54 | Use transparent background | 使用透明背景（不显示颜色提示） |
| IDS_TRAYSHOWBORDER_CHK | 55 | Show border | 显示边框 |
| IDS_TRAYROUNDCORNERS_CHK | 56 | Round corners | 圆角模式 |
| IDS_TRAYCHANGEBG_CHK | 57 | Change background color (instead text and border) | 更改背景颜色（不包括文本及边框） |
| IDS_TRAYUSEANTIALIASING_CHK | 58 | Use antialiasing | 启用抗锯齿 |
| IDS_FONT_HINT | 59 | Font: | 字体： |
| IDS_COLOR_TEXT_HINT | 60 | Text color | 文本颜色 |
| IDS_COLOR_BACKGROUND_HINT | 61 | Background color | 正常背景颜色 |
| IDS_COLOR_WARNING_HINT | 62 | "Warning level" color | “警告级别”背景色 |
| IDS_COLOR_DANGER_HINT | 63 | "Danger level" color | “危险级别”背景色 |
| IDS_TRAYLEVELWARNING_HINT | 64 | "Warning level" started at: (%) | “警告级别”阈值： (%) |
| IDS_TRAYLEVELDANGER_HINT | 65 | "Danger level" started at: (%) | “危险级别”阈值：（%） |
| IDS_TRAYACTIONSC_HINT | 66 | Single click: | Single click: |
| IDS_TRAYACTIONMC_HINT | 67 | Middle click: | 中键单击： |
| IDS_TRAY_ACTION_1 | 68 | Show/Hide | 显示/隐藏 |
| IDS_TRAY_ACTION_2 | 69 | Clean memory | 清理内存 |
| IDS_TRAY_ACTION_3 | 70 | Open task manager | 打开任务管理器 |
| IDS_SHOW_CLEAN_RESULT_CHK | 71 | Show memory cleaning results | 显示内存清理结果 |
| IDS_QUESTION | 72 | Are you sure you want to clean the memory? | 确定要清理内存吗？ |
| IDS_QUESTION_WARNING | 73 | Switching-on "Standby list" and "Modified page list" may freeze your computer for almost a minute or so, are you sure to enable this option? | 选中 "备用内存" 与 "已修改内存" 可能导致在清理内存时电脑无响应数分钟。你确定要启用吗？ |
| IDS_QUESTION_FLAG_CHK | 74 | Do not ask again | 不再提示 |
| IDS_STATUS_CLEANED | 75 | Memory was released with %s result. | 内存已释放：%s |
| IDS_STATUS_NOPRIVILEGES | 76 | Required administrator privileges, please run program as administrator! | 需要管理员权限，请以管理员模式运行！ |
| IDS_UPDATE_DOWNLOAD | 77 | Downloading update... | 下载更新中... |
| IDS_UPDATE_DONE | 78 | Downloading update finished. | 更新已完成。 |
| IDS_UPDATE_ERROR | 79 | Update server connection error. | 更新服务器连接失败。 |
| IDS_UPDATE_INIT | 80 | Checking for new releases... | 检查新版本... |
| IDS_UPDATE_INSTALL | 81 | Update available, do you want to install them? | 有更新可用，是否下载并安装? |
| IDS_RESET | 82 | Reset | 重置 |
| IDS_QUESTION_RESET | 83 | Are you really sure you want to reset all application settings? | 确定要将所有设置恢复默认吗？ |
| IDS_NOTIFICATIONSOUND_CHK | 84 | Enable notifications sound | 开启通知音效 |
| IDS_TITLE_ADVANCED | 85 | Advanced | 高级 |
| IDS_VIEW | 86 | View | 视图 |
| IDS_USEDARKTHEME | 87 | Use dark theme | 夜间模式 |
| IDS_UPDATE_SPEED | 88 | Speed | Speed |

## 3. 截图索引与证据等级

本表只索引可公开核验的上游资源和测试证据，不附带用户私人截图或本机绝对路径。散列用于复核上游图像资源字节；用户提供的参考图不作为真实 3.5.2 基准，也不复制到 PUBLIC 仓库。

| ID | 图片与尺寸 | SHA-256 | 证据内容 | 可作像素基准 |
|---|---|---|---|---|
| W-REF-01 | 官方源码 `images/memreduct.png`，338×400，RGBA；固定 tag URL | d165c86f8398161be6abdfac8557829f5d45bfbcbee12a87a71e13a38f78ce38 | 俄文暗色主窗口示意，数值区域被遮挡；没有 Windows、DPI、缩放、语言或采集方法元数据 | 否。仅为上游仓库配图，不是真实运行截图 |
| CI-PROBE-R6 | 独立 Canvas 探针截图，最终 PR artifacts 见 [GUI run 40192531809](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40192531809)；同 SHA 的 push run `40192528601` | PR 最终 head `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1`；artifact SHA 清单：探针源码 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`；图像和二进制另有逐文件散列 | 仅显示模拟占位 UI；X11/XTEST 验证 hover→pressed ROI 差异 8004 像素、released 状态、菜单、双语、几何/resize 与 WM close；Wayland artifact 验证 Weston Headless/Pixman socket、Canvas draw 和正常退出。不是 Mem Reduct Linux 产品窗口 | 否 |

两张用户提供的参考图未复制到此仓库。它们缺少权威版本和采集环境元数据，因此不能用作发布版源图或像素验收证据。R6 CI 截图也只是隔离探针图；其 336×348 top-level 与 312×324 Canvas 几何是输入测试夹具，不能作为生产窗口尺寸或 Windows DLU 换算。

### 真正 Windows 3.5.2 源图缺口

GitHub 官方 release 确实公开了 memreduct-3.5.2-setup.exe、portable 7z、签名和 SHA-256 清单；本机没有 Windows 环境，wine/wine64/7z/7zz 不可用，因此本轮无法运行 release exe 并采集源图。未从仓库配图伪造窗口状态。

下面每项真实 Windows 截图均为 BLOCKED，采集字段（应用文件散列、Windows 版本、分辨率、DPI/缩放、主题、语言、日期、采集方式）均缺：

- 主窗口 English / 简体中文；浅色 / 深色。
- 常规、内存清理、外观、托盘图标、高级五页；English / 简体中文。
- File、View、Settings、Help 根菜单与语言子菜单。
- 清理分割按钮和八项下拉菜单；托盘根菜单、区域/阈值/间隔子菜单。
- 清理确认/取消、结果通知、拒绝授权/错误、Reset 二次确认、About、更新结果。

因此没有来源和目标同态截图，没有叠加图/差异图；2 px 验收为 NOT RUN。W-REF-01 和 CI-PROBE-R5 均不能替代真实 Windows 运行截图。

## 4. 布局、控件与菜单基准

资源坐标为 DLU（Dialog Units），不是物理像素。像素换算必须用冻结系统字体与 DPI 测量，不能把 DLU 数字直接写成像素值。

- 主窗口 IDD_MAIN=100：224×214 DLU；标题 Mem Reduct；8 pt Ms Shell Dlg；IDC_LISTVIEW=102 位于 (8,8,208,174)，无边框报告列表；IDC_CLEAN=103 位于 (8,192,208,14)，BS_SPLITBUTTON。
- 主列表是 3 组×3 行：Physical memory、Pagefile、System working set；每组显示 Usage、Available、Total available。3.5.2 的第三组界面名为 System working set，但数据来源是 system_cache；Linux 不能按文字名伪造同名指标。
- 五个页资源 IDD_SETTINGS_GENERAL/MEMORY/APPEARANCE/TRAY/ADVANCED = 101–105，各 260×168 DLU，8 pt Ms Shell Dlg。**这是每个子页面模板的尺寸，不是含左侧导航、重置/关闭按钮和窗口边框的完整设置外框尺寸。**设置壳尺寸须依据完整窗口资源/布局测量，不能将 260×168 DLU 直接套到外框并裁切内容。
- 3.5.2 resource.h 明确分配 IDC_NAV=104、IDC_RESET=105、IDC_CLOSE=106。按钮未写在各页面 resource.rc 模板中，是 settings shell 控件；这不表示没有控件 ID。
- 根菜单资源 IDM_MAIN=100，四个根项由 IDS_FILE、IDS_VIEW、IDS_SETTINGS、IDS_HELP 文案定位；File 为 Settings、Exit；View 为置顶、暗色主题、清理前确认、Language；Settings 为开机启动、最小化启动、Skip UAC、定期更新；Help 为 Website、Check for updates、About。
- 3.5.2 加速资源 IDA_MAIN=1：F1→About、F2→Settings、Alt+F4→显示/隐藏。源资源中没有 Alt+字母 mnemonic；列出的源文件也没有明确 Enter/Escape 加速映射，必须以真实程序验证后才能承诺。
- 菜单与设置中的八个清理区域不是一个动作；顺序在设置列表、主窗口下拉、托盘子菜单间有差异，见交互矩阵。
- 224×214 DLU 主窗口与 260×168 DLU 设置页模板是原版官方资源尺寸，不是像素；P2B 可先实现一个**待校准的紧凑候选界面**，用集中、可替换的 DLU→逻辑像素转换参数管理整体几何，并按原版比例约束，不得把估算值散落硬编码到控件或放大为大窗口。这只允许进行工程验证，不代表 Windows 原生像素验收。设置页面 260×168 DLU 是单页模板，完整设置外框需另行测量。R5/R6 CI 中 336×348（GTK Canvas 312×324）是独立探针窗口，只用于测试输入、绘制和生命周期，不是 Windows 实测尺寸或生产 UI 目标。正式像素目标须按 v.3.5.2 字体与 DPI 实测换算。

## 5. 五页设置字段与默认行为

### General / 常规

Always on top、Load on system startup、Start minimized、Confirm memory cleaning start、Skip UAC warning、Periodically check for updates、Select language。Language 使用上游 locale 列表。默认确认开，其余依据配置初值；Skip UAC 是 Windows-only。

### Memory cleaning / 内存清理

八个清理区域；Clean when above (%) 数值范围 1–99、默认 90、开关默认关；Clean every (min.) 范围 1–1440、默认 30、开关默认关；Global hotkey 输入，默认 Ctrl+F1，开关默认关。上游 3.5.2 在进程有管理员权限时启用自动清理和全局热键；Linux 版不得在安全策略验收前接通自动特权动作。

区域标签在 3.5.2 main.h 写死英文。确认清理列表还遗漏 Modified file cache；该差异在后续 commit ae7bb84190 才补上，正式 P2A 以 3.5.2 源码行为为基准并记录此缺陷。

### Appearance / 外观

该页控制托盘文字/图标，而不是主窗口主题：Use transparent background、Show border、Round corners、Change background color、Use antialiasing；Tray Font 默认 Lucida Console 8 pt；四个颜色项 Text、Background、Warning、Danger。主窗口暗色主题由 View 菜单单独切换。

### Tray icon / 托盘图标

警告阈值默认 70%、危险阈值默认 90%；单击动作默认 Show/Hide，中键默认 Clean memory，第三选项 Open task manager；Show memory cleaning results 和 notifications sound 默认打开。SNI/Wayland 不保证中键动作。

### Advanced / 高级

Allow Standby/Modified cleanup on autoreduct 和 Log cleaning results。两条标签在 3.5.2 的 SettingsProc 中写死英文，因此中文 Windows 实例也显示英文；3.5.3 后续新增 IDS 资源才可本地化。

## 6. 指标与 Linux 语义

| Windows 3.5.2 显示/操作 | Linux 可观测或候选 | P2A 结论 |
|---|---|---|
| Physical memory | /proc/meminfo MemTotal、MemAvailable；项目现有数据代码以 total−available 估用量 | 近似等价指标，公式与舍入须标注 |
| Pagefile | SwapTotal−SwapFree | 可比的 Swap 占用，不是 Windows pagefile；不执行 swapoff |
| System working set | 上游该行读 system_cache 字段；Linux 没有同一全系统 Windows working-set 指标 | 不伪造；若另显示 Cached/SReclaimable，须改标签并说明估算定义 |
| Working set trim | Linux per-process reclaim 不等于全系统 Windows MemoryEmptyWorkingSets | 不支持 1:1 |
| System file cache | drop_caches 可粗粒度驱逐页缓存 | 不是 Windows SystemFileCacheInformationEx 等价项，不得暗中接到 Windows 名称上 |
| Modified file cache / Modified page list | sync、写回与特定 Windows 列表操作不同 | 不等价 |
| Standby / priority 0 | Linux LRU 与 Windows Standby list 结构不同 | 不等价 |
| Registry cache / Combine memory lists | Linux 无对应 Windows Registry cache 或列表合并接口 | 不适用 |
| 自动清理 | 可做用户态条件策略，但权限和恢复影响须独立评审 | 本阶段关闭；不自动触发任何特权动作 |

所有逐控件权限、完成状态、原版处理函数和验证方法见 [交互矩阵](UI_INTERACTION_MATRIX.md)。Canvas/GTK/Wayland 技术结果见 [Zamak UI 可行性报告](ZAMAK_UI_FEASIBILITY.md)，截图采集和差异复现流程见 [高保真测试计划](UI_FIDELITY_TEST_PLAN.md)。

## 7. P2B 主窗口开发拆分

1. 用真实 Windows 3.5.2 截图和冻结字体/DPI 将主窗口 DLU 转换为客户区像素；记录窗口装饰与内容区的区别。
2. 按 IDC_LISTVIEW 和 IDC_CLEAN 资源矩形建立单一绘制/命中区域模型，保持紧凑比例与原版间距。
3. 实现原版三组统计排布；Physical memory 与 Swap 使用有来源说明的 Linux 指标，System working set 不等价时显示明确限制。
4. 实现四个根菜单、Split Button 主区/下拉区、清理区域菜单和中英文独立切换；每项事件对应矩阵中的原版资源 ID/处理函数。
5. 以真实中英双语截图验证控件文字、焦点、鼠标状态、菜单边界和窗口关闭；将像素对照限制在同尺寸客户区，报告字体/装饰差异。
6. 任何 Linux 不支持的清理区域保持明确禁用/解释，不得映射为通用 `drop_caches`；P2B 不实现特权清理或自动策略。
