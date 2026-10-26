# UI/UX 交互追踪矩阵（P2A-R1 审阅稿）

状态：按官方 Mem Reduct v.3.5.2 tag 2f5c55d15f69a2c88859e036b16c6f4c971e1968 重核；本矩阵纳入 P2A 规格 Draft PR 供审阅。完整控件/动作均需支持 English / 简体中文独立切换；中文列描述上游 3.5.2 实际文字，若上游无译文则明示缺失，并另列 Linux 建议翻译。

状态含义：NATIVE=Linux 上实现原生相同动作；EQUIVALENT=语义受限的 Linux 等价；UNSUPPORTED=没有安全/准确的 Linux 等价；NOT_IMPLEMENTED=可评估但项目当前未实现。状态描述当前 Linux 项目，不代表上游 Windows 功能状态。

P2C-4 的隔离 Xvfb/XTEST 套件覆盖四个主菜单、双语文案、View 语言子菜单、Escape/方向键/Enter、菜单分隔槽、勾选及快捷键显示；深色截图分别记录 GTK `Adwaita:light` 与应用自己的 Canvas 暗色调色板，不更改桌面主题。精确 head `6502342489e1e0a6dfc1877f826f89cab1ed0a9b` 的 P2C run `40373093546` Windows、Linux Xvfb/XTEST、视觉比较三项全 PASS；P2B run `40373092535` Xvfb 与 Weston 生命周期两项 PASS，build/safety run `40373092652` PASS。细节和限制见 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`；这些检查不构成完整像素 1:1 或活动 Hyprland 输入验收。

## 1. 主窗口、菜单、快捷键

| Resource ID | 窗口 / 控件类型 | 上游文本 English / 简体中文 | 触发及 3.5.2 处理 | Linux 映射 / 权限 | 状态 | 验证 |
|---|---|---|---|---|---|---|
| IDD_MAIN=100；IDM_MAIN=100；IDS_FILE=3 | 主菜单根项 File | File / 文件 | 点击后显示下级菜单；DlgProc | Canvas 内绘制并命中；English/简体中文；无权限 | NATIVE | M-MENU-01 |
| IDM_MAIN=100；IDS_VIEW=86 | 主菜单根项 View | View / 视图 | 点击后显示下级菜单；DlgProc | Canvas 内绘制并命中；English/简体中文；无权限 | NATIVE | M-MENU-01 |
| IDM_MAIN=100；IDS_SETTINGS=4 | 主菜单根项 Settings | Settings / 设置 | 点击后显示下级菜单；DlgProc | Canvas 内绘制并命中；English/简体中文；无权限 | NATIVE | M-MENU-01 |
| IDM_MAIN=100；IDS_HELP=7 | 主菜单根项 Help | Help / 帮助 | 点击后显示下级菜单；DlgProc | Canvas 内绘制并命中；English/简体中文；无权限 | NATIVE | M-MENU-01 |
| IDM_SETTINGS=155 | File 命令 | Settings… / 设置… | WM_COMMAND→_r_settings_createwindow；F2 加速 | 打开五页设置；无权限 | NATIVE | M-MENU-02、K-01 |
| IDM_EXIT=156 | File 命令 | Exit / 退出 | WM_COMMAND 结束应用 | 正常退出；无权限 | NATIVE | M-MENU-02 |
| IDM_ALWAYSONTOP_CHK=157 | View 勾选项 | Always on top / 窗口始终置顶 | WM_COMMAND 写设置并切换窗口置顶状态 | GTK keep-above 可能受 Wayland compositor 限制；无权限 | NOT_IMPLEMENTED | M-MENU-03 |
| IDM_USEDARKTHEME=159 | View 勾选项 | Use dark theme / 夜间模式 | 写主题设置并重绘 | 切换应用 Canvas 浅/暗调色板并在 View 显示勾选；当前只对本次会话有效；无权限 | NATIVE | M-MENU-03、SHOT-01 |
| IDM_REDUCTCONFIRMATION_CHK=161 | View 勾选项 | Confirm memory cleaning start / 在内存清理前手动确认 | 改 IsShowReductConfirmation | Linux 清理前确认；无权限 | NOT_IMPLEMENTED | M-CLEAN-01 |
| IDX_LANGUAGE=1000 起 | View 子菜单 | Language (Language) / 语言…… (Language)；动态 locale 名称 | DlgProc 动态列 locale；WM_COMMAND→locale apply | English/简体中文运行时切换；动态名称与勾选状态可见；仅会话状态，不保存；无权限 | NATIVE | M-LANG-01 |
| IDM_LOADONSTARTUP_CHK=158 | Settings 勾选项 | Load on system startup / 开机自启 | WM_COMMAND→autorun helper | 只可考虑用户级 XDG Autostart；不装 root 服务 | NOT_IMPLEMENTED | M-SET-01 |
| IDM_STARTMINIMIZED_CHK=160 | Settings 勾选项 | Start minimized / 最小化启动（不显示窗口） | 改启动可见状态 | 桌面启动器与托盘能力相关；无权限 | NOT_IMPLEMENTED | M-SET-01 |
| IDM_SKIPUACWARNING_CHK=162 | Settings 勾选项 | Skip “User Account Control” prompt warning / 跳过“用户账户控制（UAC）”提示 | Windows UAC 专用选项 | 不得映射成跳过 Polkit；无权限 | UNSUPPORTED | M-SET-02 |
| IDM_CHECKUPDATES_CHK=163 | Settings 勾选项 | Periodically check for updates (recommended) / 自动检查更新（推荐） | 更新检查 enable toggle | Linux 更新源/签名与网络行为待设计；无权限 | NOT_IMPLEMENTED | M-UPDATE-01 |
| IDM_WEBSITE=164；IDM_TRAY_WEBSITE=172 | Help / Tray 命令 | Website / 项目网站 | WM_COMMAND 调浏览器 | 打开已审核项目 URL；无特权 | NOT_IMPLEMENTED | M-HELP-01 |
| IDM_CHECKUPDATES=165 | Help 命令 | Check for updates / 检查更新 | 启动检查更新状态机 | 未接入签名更新源时明确提示，不报假成功；无权限 | NOT_IMPLEMENTED | M-UPDATE-01 |
| IDM_ABOUT=166；IDM_TRAY_ABOUT=173 | Help / Tray 命令 | About / 关于…… | WM_COMMAND 显示 About；F1 加速 | 显示独立移植和上游署名；无权限 | NOT_IMPLEMENTED | M-HELP-02 |
| IDS_DONATE=11 | About 对话框资源/链接文案 | Give thanks! / 捐赠...... | 3.5.2 的 About 命令调用共享 helper _r_show_aboutmessage；IDS_DONATE 文案存在于资源表，但其运行时使用、可见位置和链接目标须由 release 截图/实机确认 | 保留独立项目身份与上游署名；不代表 Linux 项目募捐/跳转行为已实现；无权限 | NOT_IMPLEMENTED | M-HELP-02、WIN-SHOT-ABOUT |
| IDA_MAIN=1 | 主窗口加速键 | F1→About；F2→Settings；Alt+F4→Show/Hide | resource.rc ACCELERATORS | 本地快捷键可在窗口焦点内映射；Wayland 不授予全局热键；无权限 | NOT_IMPLEMENTED | K-01 |
| IDA_MAIN 未列出 | 主窗口 Enter/Escape | 3.5.2 固定资源未显式列入 IDA_MAIN；所核文件无显式应用级绑定 | 不把 Win32 对话框默认行为臆测成应用处理器 | 必须通过 Windows release 实机确定；无权限 | NOT_IMPLEMENTED | K-02 |
| IDD_MAIN=100；IDC_LISTVIEW=102；IDS_GROUP_1=18；IDS_ITEM_1..3=21..23 | 主窗口物理内存行 | Physical memory / 物理内存；Usage / 已用；Available / 空闲；Total available / 总计 | _app_timercallback 更新 owner-draw 列表 | P1 已读取 MemTotal/MemAvailable；使用量为 total−available。指标 EQUIVALENT，但原版 3×3 列表布局仍未实现；无权限 | EQUIVALENT | M-METRIC-01、M-MAIN-01 |
| IDD_MAIN=100；IDC_LISTVIEW=102；IDS_GROUP_2=19；IDS_ITEM_1..3=21..23 | 主窗口 Swap 行 | Pagefile / 虚拟内存；Usage / 已用；Available / 空闲；Total available / 总计 | _app_timercallback 更新 owner-draw 列表 | P1 读取 SwapTotal/SwapFree；这是 Swap 类比而非 Windows Pagefile；原版行布局未实现；无权限 | EQUIVALENT | M-METRIC-01、M-MAIN-01 |
| IDD_MAIN=100；IDC_LISTVIEW=102；IDS_GROUP_3=20；IDS_ITEM_1..3=21..23 | 主窗口系统工作集行 | System working set / 系统工作集；Usage / 已用；Available / 空闲；Total available / 总计 | 3.5.2 将 system_cache 作为该组数据源；owner-draw 列表周期刷新 | Linux 没有可验证的全系统等价指标。P1 的 Cached+SReclaimable 是单独标注的缓存估计，不可冒充；无权限 | UNSUPPORTED | M-METRIC-02、M-MAIN-01 |
| IDC_CLEAN=103 | 主窗口 split button | Clean memory / 清理内存 | 主区清理当前 mask；BCN_DROPDOWN 分派下拉；_app_memoryclean | 当前 P1 helper 的 drop_caches 枚举不是八个 Windows 区域的等价实现；特权操作需一次显式授权 | NOT_IMPLEMENTED | M-CLEAN-01、M-CLEAN-02 |
| IDM_CLEAN_WORKINGSET=183；设置列表 IDC_REGIONS=124 第0行 | 清理选项 | Working set / 上游 3.5.2 无简体中文；Linux 建议“工作集” | _app_memoryclean→MemoryEmptyWorkingSets；WM_COMMAND | Linux 无全系统 1:1 working-set trim；不调用 drop_caches 代替；需特权的候选动作不可用 | UNSUPPORTED | M-CLEAN-03 |
| IDM_CLEAN_SYSTEMFILECACHE=184；IDC_REGIONS 第1行 | 清理选项 | System file cache / 上游无简中；建议“系统文件缓存” | _app_memoryclean→SystemFileCacheInformationEx | drop_caches 是广义页缓存驱逐，不等价；不暗接；特权 | UNSUPPORTED | M-CLEAN-03 |
| IDM_CLEAN_MODIFIEDFILECACHE=185；IDC_REGIONS 第5行 | 清理选项 | Modified file cache / 上游无简中；建议“已修改文件缓存” | _app_flushvolumecache | sync 或 drop_caches 均不等价；不实现成相同按钮；可能涉及写回 | UNSUPPORTED | M-CLEAN-03 |
| IDM_CLEAN_MODIFIEDLIST=186；IDC_REGIONS 第2行 | 清理选项 | Modified page list* / 上游无简中；建议“已修改页列表*” | _app_memoryclean→MemoryFlushModifiedList；选中触发额外警告 | Linux 无同结构列表；不映射 drop_caches；候选特权写不可用 | UNSUPPORTED | M-CLEAN-03 |
| IDM_CLEAN_STANDBYLIST=187；IDC_REGIONS 第3行 | 清理选项 | Standby list* / 上游无简中；建议“备用列表*” | _app_memoryclean→MemoryPurgeStandbyList；选中触发额外警告 | Linux LRU 不等于 Windows Standby list；不映射 drop_caches | UNSUPPORTED | M-CLEAN-03 |
| IDM_CLEAN_STANDBYLISTPRIORITY0=188；IDC_REGIONS 第4行 | 清理选项 | Standby list (without priority) / 上游无简中；建议“无优先级备用列表” | _app_memoryclean→MemoryPurgeLowPriorityStandbyList | Linux 无对应优先级列表 | UNSUPPORTED | M-CLEAN-03 |
| IDM_CLEAN_REGISTRYCACHE=189；IDC_REGIONS 第6行 | 清理选项 | Registry cache (win8.1+) / 上游无简中；建议“注册表缓存（Windows 8.1+）” | _app_memoryclean→SystemRegistryReconciliationInformation；旧 Windows 禁用 | Linux 无 Windows Registry cache | UNSUPPORTED | M-CLEAN-03 |
| IDM_CLEAN_COMBINEMEMORYLISTS=190；IDC_REGIONS 第7行 | 清理选项 | Combine memory lists (win10+) / 上游无简中；建议“合并内存列表（Windows 10+）” | _app_memoryclean→SystemCombinePhysicalMemoryInformation；旧 Windows 禁用 | Linux 无同名列表合并操作 | UNSUPPORTED | M-CLEAN-03 |

## 2. 设置窗口控件

| Resource ID | 窗口 / 控件类型 | 上游文本 English / 简体中文 | 触发及 3.5.2 处理 | Linux 映射 / 权限 | 状态 | 验证 |
|---|---|---|---|---|---|---|
| IDD_SETTINGS_GENERAL=101；IDD_SETTINGS_MEMORY=102；IDD_SETTINGS_APPEARANCE=103；IDD_SETTINGS_TRAY=104；IDD_SETTINGS_ADVANCED=105；IDC_NAV=104 | 左侧页面导航 | General / 常规；Memory cleaning / 内存清理；Appearance / 外观；Tray icon / 托盘图标；Advanced / 高级 | 共用 settings shell；页面切换发 RM_INITIALIZE | Zamak Canvas 独立设置顶层，五个子页可通过鼠标和方向键切换；无权限 | EQUIVALENT | S-NAV-01 |
| IDC_RESET=105；IDS_RESET=82；IDS_QUESTION_RESET=83 | 设置壳 Reset 按钮与确认框 | Reset / 重置；Are you really sure you want to reset all application settings? / 确定要将所有设置恢复默认吗？ | settings framework 处理 Reset；先二次确认，取消不保存，确认恢复全部默认值 | 显示原版确认流程；没有持久化配置，确认/取消均不改状态或系统；无权限 | EQUIVALENT | S-RESET-01 |
| IDC_CLOSE=106；IDS_CLOSE=28 | 设置壳 Close 按钮与窗口关闭 | Close / 关闭 | settings framework 关闭设置窗口 | 关闭独立设置顶层并返回主窗口；Escape 与窗口管理器关闭请求也有处理；无权限 | EQUIVALENT | S-CLOSE-01 |
| IDC_TITLE_1=107、IDC_TITLE_2=108、IDC_TITLE_3=109、IDC_TITLE_4=110、IDC_TITLE_5=111、IDC_TITLE_6=112、IDC_TITLE_7=113、IDC_TITLE_8=114、IDC_TITLE_9=115；IDS_TITLE_1..9=29..37；IDS_TITLE_ADVANCED=85 | 设置页分组标题 | General configuration / 选项；Language / 语言；Memory regions to be cleaned / 要清理的内存区域；Memory management / 自动管理内存；Hotkeys (global) / 快捷键（通用）；Style / 个性化；Color indication / 颜色指示；Mouse control / 鼠标控制；Balloon tips / 气泡提示（Windows 通知）；Advanced / 高级 | SettingsProc RM_LOCALIZE 设置标题和冒号 | Canvas 按各官方子页顺序保留字段分组；几何换算仍为 PROVISIONAL；无权限 | EQUIVALENT | S-GEOM-01、S-TEXT-01 |
| IDC_ALWAYSONTOP_CHK=116；IDS_ALWAYSONTOP_CHK=38 | General 复选框 | Always on top / 窗口始终置顶 | SettingsProc 状态加载、WM_COMMAND 更新 | GTK keep-above 可能被 Wayland 拒绝；无权限 | NOT_IMPLEMENTED | S-GEN-01 |
| IDC_LOADONSTARTUP_CHK=117；IDS_LOADONSTARTUP_CHK=39 | General 复选框 | Load on system startup / 开机自启 | autorun helper | 仅用户级 XDG Autostart 可作为候选；无 root 服务 | NOT_IMPLEMENTED | S-GEN-01 |
| IDC_STARTMINIMIZED_CHK=118；IDS_STARTMINIMIZED_CHK=40 | General 复选框 | Start minimized / 最小化启动（不显示窗口） | 持久化启动状态 | 启动器/托盘依赖；无权限 | NOT_IMPLEMENTED | S-GEN-01 |
| IDC_REDUCTCONFIRMATION_CHK=120；IDS_REDUCTCONFIRMATION_CHK=41 | General 复选框 | Confirm memory cleaning start / 在内存清理前手动确认 | 更新 IsShowReductConfirmation | 在安全动作前显示确认；无权限 | NOT_IMPLEMENTED | S-GEN-01 |
| IDC_SKIPUACWARNING_CHK=119；IDS_SKIPUACWARNING_CHK=42 | General 复选框 | Skip “User Account Control” prompt warning / 跳过“用户账户控制（UAC）”提示 | Windows UAC helper，仅特定权限下启用 | 不提供跳过 Polkit 的替代开关 | UNSUPPORTED | S-GEN-02 |
| IDC_CHECKUPDATES_CHK=121；IDS_CHECKUPDATES_CHK=43 | General 复选框 | Periodically check for updates (recommended) / 自动检查更新（推荐） | 更新服务状态切换 | Linux 更新渠道待签名/来源评审；无权限 | NOT_IMPLEMENTED | S-GEN-03 |
| IDC_LANGUAGE_HINT=122；IDC_LANGUAGE=123；IDS_LANGUAGE_HINT=44 | General 语言控件 | Select language: / 选择语言：；选项来自上游 locale 清单 | _r_locale_enum；运行时重新本地化全部窗口 | 设置窗可在 English 与简体中文之间即时切换；不写入持久化配置；无权限 | EQUIVALENT | M-LANG-01 |
| IDC_REGIONS=124，第0–7行 | Memory 区域勾选列表 | 八个区域名称见第1节；3.5.2 main.h 写死英文，中文列均无上游译文 | SettingsProc RM_INITIALIZE 初始化列表；WM_NOTIFY 修改 ReductMask2 | 八项均保留原版顺序并禁用；点击只说明 Linux 能力限制，不修改配置或触发清理 | UNSUPPORTED | S-MEM-01 |
| IDC_AUTOREDUCTENABLE_CHK=125；IDC_AUTOREDUCTVALUE_CTRL=126；IDC_AUTOREDUCTVALUE=127；IDS_AUTOREDUCTENABLE_CHK=51 | Memory 阈值复选框/数值框/微调器 | Clean when above: (%) / 占用达阈值时清理（%）；范围1–99，默认90，开关默认关 | _app_timercallback 达阈值时自动清理；3.5.2 无冷却，后续源码才加30秒冷却 | P1 用户态策略原型存在但 P2 安全验收前不得提供自动特权调用 | NOT_IMPLEMENTED | S-MEM-02 |
| IDC_AUTOREDUCTINTERVALENABLE_CHK=128；IDC_AUTOREDUCTINTERVALVALUE_CTRL=129；IDC_AUTOREDUCTINTERVALVALUE=130；IDS_AUTOREDUCTINTERVALENABLE_CHK=52 | Memory 定时复选框/数值框/微调器 | Clean every: (min.) / 每隔指定时间清理（分钟）；范围1–1440，默认30，开关默认关 | _app_timercallback 检查 StatisticLastReduct | Linux 自动请求/冷却尚未独立审查；不启用 | NOT_IMPLEMENTED | S-MEM-03 |
| IDC_HOTKEY_CLEAN_CHK=131；IDC_HOTKEY_CLEAN=132；IDS_HOTKEY_CLEAN_CHK=53 | Memory 全局热键复选框/热键控件 | Clean memory: / 清理内存：；默认 Ctrl+F1，开关默认关 | _app_hotkeyinit / WM_HOTKEY，仅在 Windows elevation 状态注册 | Wayland portal/compositor 专属绑定；不得声称全局注册 | NOT_IMPLEMENTED | K-03; isolated Windows v3.5.2 native capture, `HKM_GETHOTKEY` → `Ctrl+F1` (run 40319586672) |
| IDC_TRAYUSETRANSPARENCY_CHK=133；IDS_TRAYUSETRANSPARENCY_CHK=54 | Appearance 复选框 | Use transparent background / 使用透明背景（不显示颜色提示） | SettingsProc 写 TrayUseTransparency | 仅影响托盘图标；Linux SNI 图标绘制待实现 | NOT_IMPLEMENTED | S-APP-01 |
| IDC_TRAYSHOWBORDER_CHK=134；IDS_TRAYSHOWBORDER_CHK=55 | Appearance 复选框 | Show border / 显示边框 | 更新托盘绘制状态 | 同上 | NOT_IMPLEMENTED | S-APP-01 |
| IDC_TRAYROUNDCORNERS_CHK=135；IDS_TRAYROUNDCORNERS_CHK=56 | Appearance 复选框 | Round corners / 圆角模式 | 更新托盘绘制状态 | 同上 | NOT_IMPLEMENTED | S-APP-01 |
| IDC_TRAYCHANGEBG_CHK=136；IDS_TRAYCHANGEBG_CHK=57 | Appearance 复选框 | Change background color (instead text and border) / 更改背景颜色（不包括文本及边框） | 阈值色模式切换 | 同上；无权限 | NOT_IMPLEMENTED | S-APP-01 |
| IDC_TRAYUSEANTIALIASING_CHK=137；IDS_TRAYUSEANTIALIASING_CHK=58 | Appearance 复选框 | Use antialiasing / 启用抗锯齿 | 3.5.2 字体初始化固定 ClearType；后续才尊重此项 | Cairo/Pango 与 Windows ClearType 不同；无权限 | NOT_IMPLEMENTED | S-APP-02 |
| IDC_FONT_HINT=138；IDC_FONT=139；IDS_FONT_HINT=59 | Appearance 字体选择按钮 | Font: / 字体：；默认 Lucida Console 8 pt | 调用 Windows font chooser，设置 tray font | Painter.text 无 font family 参数；GTK/Pango bridge 或自绘需评估 | NOT_IMPLEMENTED | S-APP-03 |
| IDC_COLORS=140；IDS_COLOR_TEXT_HINT=60、BACKGROUND=61、WARNING=62、DANGER=63 | Appearance 四行颜色列表 | Text color / 文本颜色；Background color / 正常背景颜色；“Warning level” color / “警告级别”背景色；“Danger level” color / “危险级别”背景色 | NM_CUSTOMDRAW 打开 Color chooser，写四个 tray color 值 | Linux 图标色可自绘；不应更改主窗口色板；无权限 | NOT_IMPLEMENTED | S-APP-04 |
| IDC_TRAYLEVELWARNING_HINT=141；IDC_TRAYLEVELWARNING_CTRL=142；IDC_TRAYLEVELWARNING=143；IDS_TRAYLEVELWARNING_HINT=64 | Tray 数值框 | “Warning level” started at: (%) / “警告级别”阈值： (%)；默认70，范围1–99 | up-down 控件调整阈值，TrayLevelWarning 持久化 | 动态 SNI 图标阈值；无权限 | NOT_IMPLEMENTED | S-TRAY-01 |
| IDC_TRAYLEVELDANGER_HINT=144；IDC_TRAYLEVELDANGER_CTRL=145；IDC_TRAYLEVELDANGER=146；IDS_TRAYLEVELDANGER_HINT=65 | Tray 数值框 | “Danger level” started at: (%) / “危险级别”阈值：（%）；默认90，范围1–99 | up-down 控件调整阈值 | 同上；无权限 | NOT_IMPLEMENTED | S-TRAY-01 |
| IDC_TRAYACTIONSC_HINT=147；IDC_TRAYACTIONSC=148；IDS=66 | Tray 单击标签/下拉 | Single click: / 上游简中仍为 Single click:；候选 Show/Hide、Clean memory、Open task manager / 显示/隐藏、清理内存、打开任务管理器 | TrayActionDc；默认 Show/Hide | SNI 激活协议需实测；任务管理器改为 Linux 系统监视器时须明确文案 | NOT_IMPLEMENTED | S-TRAY-02 |
| IDC_TRAYACTIONMC_HINT=149；IDC_TRAYACTIONMC=150；IDS=67 | Tray 中键标签/下拉 | Middle click: / 中键单击：；同上三个动作 | TrayActionMc；默认 Clean memory | StatusNotifierItem 不保证提供中键事件；依桌面协议 | NOT_IMPLEMENTED | S-TRAY-03 |
| IDC_SHOW_CLEAN_RESULT_CHK=152；IDS=71 | Tray 复选框 | Show memory cleaning results / 显示内存清理结果；默认开 | BalloonCleanResults 控制通知 | freedesktop notification 服务；退出状态需真实展示 | NOT_IMPLEMENTED | M-CLEAN-04 |
| IDC_NOTIFICATIONSOUND_CHK=151；IDS=84 | Tray 复选框 | Enable notifications sound / 开启通知音效；默认开 | IsNotificationsSound 控制通知音 | 音效由桌面策略控制；无权限 | NOT_IMPLEMENTED | M-CLEAN-04 |
| IDC_ALLOWSTANDBYLISTCLEANUP_CHK=153 | Advanced 复选框 | Allow “Standby lists” and “Modified page list” cleanup on autoreduct / 上游简中无译文；建议“允许自动清理备用列表和已修改页列表” | SettingsProc 直接硬编码英文；配置默认关 | Windows-only 列表不等价，自动策略保持禁用 | UNSUPPORTED | S-ADV-01 |
| IDC_LOGRESULTS_CHK=154 | Advanced 复选框 | Log cleaning results into a debug log / 上游简中无译文；建议“将清理结果写入调试日志” | SettingsProc 直接硬编码英文；默认关 | 可考虑隐私最小化日志；当前未实现 | NOT_IMPLEMENTED | S-ADV-02 |

## 3. 托盘菜单、状态和反馈

| Resource ID | 窗口 / 控件类型 | 上游文本 English / 简体中文 | 触发及 3.5.2 处理 | Linux 映射 / 权限 | 状态 | 验证 |
|---|---|---|---|---|---|---|
| IDM_TRAY=101；IDM_TRAY_SHOW=167；IDS_TRAY_SHOW=12 | Tray 根菜单显示/隐藏 | Show/Hide / 显示/隐藏 | WM_CONTEXTMENU 构建；WM_COMMAND 切换窗口 | StatusNotifierItem/AppIndicator 菜单；无权限 | NOT_IMPLEMENTED | T-MENU-01 |
| IDM_TRAY_CLEAN=168；IDS_CLEAN=17 | Tray 菜单清理动作 | Clean memory… / 清理内存… | SOURCE_MANUAL，确认/权限/结果路径 | 每次安全审查后的单一 Linux 动作授权；需要特权时 Polkit | NOT_IMPLEMENTED | M-CLEAN-01、T-CLEAN-01 |
| IDM_WORKINGSET_CHK=175 | Tray 区域勾选项 | Working set / 上游无简中；建议“工作集” | WM_CONTEXTMENU check state；WM_COMMAND 改 ReductMask2 | 无 Linux 等价；不静默勾选后无动作 | UNSUPPORTED | T-REGION-01 |
| IDM_SYSTEMFILECACHE_CHK=176 | Tray 区域勾选项 | System file cache / 上游无简中；建议“系统文件缓存” | 同上 | drop_caches 不是等价项 | UNSUPPORTED | T-REGION-01 |
| IDM_MODIFIEDFILECACHE_CHK=177 | Tray 区域勾选项 | Modified file cache / 上游无简中；建议“已修改文件缓存” | 同上 | 无直接等价 | UNSUPPORTED | T-REGION-01 |
| IDM_MODIFIEDLIST_CHK=178 | Tray 区域勾选项 | Modified page list* / 上游无简中；建议“已修改页列表*” | 同上 | 无直接等价 | UNSUPPORTED | T-REGION-01 |
| IDM_STANDBYLIST_CHK=179 | Tray 区域勾选项 | Standby list* / 上游无简中；建议“备用列表*” | 同上 | 无直接等价 | UNSUPPORTED | T-REGION-01 |
| IDM_STANDBYLISTPRIORITY0_CHK=180 | Tray 区域勾选项 | Standby list (without priority) / 上游无简中；建议“无优先级备用列表” | 同上 | 无直接等价 | UNSUPPORTED | T-REGION-01 |
| IDM_REGISTRYCACHE_CHK=181 | Tray 区域勾选项 | Registry cache (win8.1+) / 上游无简中；建议“注册表缓存（Windows 8.1+）” | 同上；旧 Windows 禁用 | Linux 不适用 | UNSUPPORTED | T-REGION-01 |
| IDM_COMBINEMEMORYLISTS_CHK=182 | Tray 区域勾选项 | Combine memory lists (win10+) / 上游无简中；建议“合并内存列表（Windows 10+）” | 同上；旧 Windows 禁用 | Linux 不适用 | UNSUPPORTED | T-REGION-01 |
| IDM_TRAY_DISABLE_1=169；IDS_TRAY_DISABLE=13；IDX_TRAY_POPUP_1=2000起 | 阈值子菜单 | Clean when above / 清理界限；Disable / 禁用；百分比动态条目 | _app_generate_menu 生成默认 10%–80% 十位步进及当前值邻域；选项写 AutoreductValue | Linux 保持关闭；不自动触发 | NOT_IMPLEMENTED | T-AUTO-01 |
| IDM_TRAY_DISABLE_2=170；IDX_TRAY_POPUP_2=3000起 | 间隔子菜单 | Clean every / 清理间隔；Disable / 禁用；分钟动态条目 | 动态生成并写 AutoreductIntervalValue | Linux 保持关闭 | NOT_IMPLEMENTED | T-AUTO-02 |
| IDM_TRAY_SETTINGS=171 | Tray 设置命令 | Settings… / 设置… | _r_settings_createwindow | 打开同一五页设置；无权限 | NOT_IMPLEMENTED | T-MENU-01 |
| IDM_TRAY_WEBSITE=172；IDM_TRAY_ABOUT=173；IDM_TRAY_EXIT=174 | Tray 帮助/退出命令 | Website / 项目网站；About / 关于……；Exit / 退出 | WM_COMMAND | 同主菜单动作；无权限 | NOT_IMPLEMENTED | T-MENU-01 |
| RM_TRAYICON：WM_LBUTTONDOWN / WM_MBUTTONDOWN / WM_CONTEXTMENU | Tray 单击/中键/右键 | 单击/中键动作来自 IDS_TRAY_ACTION_1..3；右键显示完整菜单 | _app_memoryclean、_r_wnd_toggle、Windows taskmgr.exe、_r_menu_popup | SNI 可提供菜单/激活，但中键和右键能力需逐桌面证明；清理动作需特权 | NOT_IMPLEMENTED | T-CLICK-01 |
| RM_TRAYICON：NIN_KEYSELECT | Tray 键盘激活 | 无独立标签 | 切换主窗口显隐 | 保留可键盘操作；协议依赖 | NOT_IMPLEMENTED | T-KEY-01 |
| _app_iconcreate / _app_timercallback；IDS_GROUP_1..3 | Tray 动态图标和提示 | 物理内存百分比；tooltip 三组名称与百分比 | 使用 70/90 阈值颜色，状态变化时重绘图标 | 动态 SNI 图标/tooltip；system working set 数据语义不可伪造 | NOT_IMPLEMENTED | T-ICON-01 |
| IDS_QUESTION=72；IDS_QUESTION_FLAG_CHK=74 | 清理确认对话框 | Are you sure you want to clean the memory? / 确定要清理内存吗？；Do not ask again / 不再提示 | 手动清理前显示；取消不执行；确认框可保存不再询问 | 明确列出将执行的 Linux 操作；Polkit 再独立授权 | NOT_IMPLEMENTED | M-CLEAN-01 |
| IDS_QUESTION_WARNING=73 | 危险区域警告 | 上游警告启用 Standby/Modified list 可能冻结近一分钟；简中说明可能无响应数分钟 | 启用高风险选项时二次确认 | 不支持选项禁用并说明原因；不能照搬不相干的 Windows 警告 | UNSUPPORTED | M-CLEAN-05 |
| IDS_STATUS_NOPRIVILEGES=76 | 权限拒绝结果 | Required administrator privileges, please run program as administrator! / 需要管理员权限，请以管理员模式运行！ | UAC 被拒绝时错误通知 | Linux 使用 Polkit 拒绝/取消分开反馈，不以 GUI root 运行 | NOT_IMPLEMENTED | M-CLEAN-06 |
| IDS_STATUS_CLEANED=75；BalloonCleanResults；LogCleanResults | 清理完成结果/通知/日志 | Memory was released with %s result. / 内存已释放：%s | _app_memoryclean 计算前后 used memory 差；可发通知/声音/调试日志 | 只报告实际观察值和 helper 状态，不能宣称精确回收量 | NOT_IMPLEMENTED | M-CLEAN-04 |
| IDS_UPDATE_INIT/DOWNLOAD/DONE/ERROR/NO/YES/INSTALL=80/77/78/79/1/2/81 | 更新状态提示/确认框 | Checking releases、下载中、完成、失败、无更新、发现更新；逐键英文/中文见基准资源表 | 更新状态机显示 toast/dialog | 无已审查 Linux 安全更新渠道时明确不可用 | NOT_IMPLEMENTED | M-UPDATE-01 |
| IDC_RESET=105；IDS_QUESTION_RESET=83 | Reset 确认结果 | 见设置壳行 | 取消不改设置；确认恢复默认 | 设置持久化待实现 | NOT_IMPLEMENTED | S-RESET-01 |

## 4. Linux 内存语义映射总表

| Windows 概念 | Linux 数据/候选 | 权限 | 当前结论 |
|---|---|---|---|
| Physical memory | /proc/meminfo MemTotal / MemAvailable | 无 | EQUIVALENT：近似指标，必须标明 used=total−available |
| Pagefile | SwapTotal−SwapFree | 无 | EQUIVALENT：Swap 类比，不等同 Pagefile |
| System working set | 无全系统同义接口 | 无 | UNSUPPORTED |
| System file cache | Cached+SReclaimable 仅为估计；drop_caches 是粗粒度写操作 | drop_caches 需特权 | UNSUPPORTED：不能冒充原版动作 |
| Windows Working set trim | Linux process-level reclaim 不能代表全系统清理 | 可能需要受限权限 | UNSUPPORTED |
| Modified cache/list | sync 或回写不等于 purge 指定 Windows 列表 | 视候选操作而定 | UNSUPPORTED |
| Standby/priority 0 | Linux LRU 无同一优先级列表 | 视候选操作而定 | UNSUPPORTED |
| Registry cache / Combine lists | 无对应对象 | 无 | UNSUPPORTED |
| Automatic cleanup | 条件策略 + 单次明确授权模型待评审 | 可能需要 Polkit | NOT_IMPLEMENTED |
| Tray/global hotkey | SNI/AppIndicator 与桌面 portal/compositor | 无 | NOT_IMPLEMENTED；实机能力待验证 |

## 5. 状态与验证覆盖说明

矩阵跟踪的是官方 3.5.2 的 8 个清理区域、5 个设置页、主/托盘菜单、确认/重置/更新/通知状态及键盘行为。主窗口生产候选的逐控件实现状态、资源 ID、运行截图与 CI 证据见 [P2B-1 生产窗口报告](P2B-1_PRODUCTION_UI_REPORT.md)；其余设置、托盘与清理流程仍按本矩阵中的状态记录。所有截图和交互脚本的 ID、结果与 BLOCKED/NOT RUN 证据定义在 UI_FIDELITY_TEST_PLAN.md。禁止把来源不明的截图当作源图，禁止将 8 个 Windows 操作统一接到 drop_caches。

## 6. 资源符号覆盖审计

已按官方 tag 的 src/resource.h 与 src/resource.rc 复核矩阵：资源头定义 54 个 IDC（含通用 IDC_STATIC=-1）、38 个 IDM 命令、1 个 IDA 加速表和 6 个 IDD 窗口。每个可操作 IDC/IDM/IDA 项均在表格中逐项或按连续控件组明确列出；IDC_STATIC 是多个静态控件共用的占位 ID，不代表一个独立操作。六个窗口 ID 已分别关联主窗与五页设置。资源头另有 88 个 IDS 宏，其中 ID 45–50 的 6 个宏没有 RC 字符串、简中语言键或代码引用；活动 STRINGTABLE 的 82 个文案见基准清单。此项是静态资源符号覆盖检查，不等于 Windows 可执行程序运行时截图验收。

## 7. Windows v3.5.2 只读默认状态快照

这些默认值来自固定 3.5.2 源码，并与隔离 Windows Native 设置页截图交叉核对。Linux 当前 UI 仅呈现该默认快照；它不读取 Windows 注册表、不代表 Linux 用户已保存的偏好，也不会启用对应操作。除 English/简体中文会话语言选择和设置壳动作外，配置控件继续保持禁用。

| 页面 / 资源 ID | Native 默认显示 | 当前 Linux 表现 | 操作状态 |
|---|---|---|---|
| General `IDC_REDUCTCONFIRMATION_CHK=120`, `IDC_CHECKUPDATES_CHK=121` | 勾选；其余页面复选框不勾选 | 勾选状态以灰色只读快照显示 | `NOT_IMPLEMENTED`；点击只解释限制 |
| Memory cleaning `IDC_REGIONS=124` | 行 0、1、4、5、6、7 勾选；行 2、3 不勾选 | 八行按上游默认展示，列表滚动位置不更改默认值 | `UNSUPPORTED`；不映射到 `drop_caches` |
| Memory cleaning `IDC_AUTOREDUCTENABLE_CHK=125`, `IDC_AUTOREDUCTINTERVALENABLE_CHK=128`, `IDC_HOTKEY_CLEAN_CHK=131` | 均不勾选 | 未勾选、禁用 | `NOT_IMPLEMENTED` |
| Appearance `IDC_TRAYCHANGEBG_CHK=136` | 勾选；其他外观复选框不勾选 | 勾选状态只读；`IDC_COLORS=140` 预览 `#FFFFFF`, `#008040`, `#FF8040`, `#EC1C24` | `NOT_IMPLEMENTED`；不编辑/保存托盘颜色 |
| Tray icon `IDC_NOTIFICATIONSOUND_CHK=151`, `IDC_SHOW_CLEAN_RESULT_CHK=152` | 均勾选 | 勾选状态以灰色只读快照显示 | `NOT_IMPLEMENTED`；不启用通知或声音 |
| Advanced `IDC_ALLOWSTANDBYLISTCLEANUP_CHK=153`, `IDC_LOGRESULTS_CHK=154` | 均不勾选 | 未勾选、禁用 | 清理选项 `UNSUPPORTED`；日志 `NOT_IMPLEMENTED` |

默认值截图不宣称为 Linux 已保存配置。对应资源矩形、像素叠加、差异图与 2 px 判断以 [P2C-1 设置报告](P2C-1_SETTINGS_UI_REPORT.md) 中精确 Actions head 的证据为准；不同 CI runs 生成的截图不得混用。

简体中文原版显示与 Linux 当前翻译不同的内容包括 8 个内存清理区域、2 个 Advanced 标签、仍显示英文的 `Single click:`，以及语言下拉的 `Chinese (Simplified)` / `简体中文` 名称差异。它们按资源 ID 记录于 [P2C-3 上游简中显示差异矩阵](P2C-3_SOURCE_LANGUAGE_MATRIX.md)，作为内容差异而非字体误差；产品决定前保留独立简体中文界面。

P2C-2 renderer verification at source head `20c8fbc8fc80c481965fecbde17aa507eb7247fe` confirms the page surface has no added child-dialog frame, resource-defined GroupBoxes remain, four Appearance swatches match the source defaults in both languages, and source-default checkmarks remain disabled/read-only. Disabled-option feedback does not alter saved state or trigger a system action. For `IDC_NAV=104`, the five native selected-row fills use `#D9D9D9`, selection bounds are measured separately per locale, and the tree guide uses native `#6D6D6D` pixels; the current Windows/Linux comparison passes all 10 selection bounds with 0 px delta and all 102 sampled guide pixels. Exact-head screenshots, SHA-256 manifest, overlays, heatmaps, and CI artifact IDs are indexed in [P2C-1/P2C-2 settings evidence](P2C-1_SETTINGS_UI_REPORT.md). The overall geometry comparison is 172 PASS within 2 px, 0 FAIL, with 12 intermediate memory-list scroll rows marked `NOT_COMPARABLE_NATIVE_STATE`; whole-image changed-pixel results still differ and do not establish pixel equivalence.

Exact-head calibration at source `06be74b740e8906e64d837d8ac77e0255bea1081` samples the shared GroupBox light-theme edge at client `(156,20)`: Windows and Linux are both RGB `(220,220,220)` in English and Simplified Chinese. Windows/Linux comparison run `40290615170` passes all 12 sampled raster checks and all 8 appearance swatches. Its geometry table has 172 PASS within 2 px, 0 FAIL, and 12 Linux intermediate-scroll rows marked `NOT_COMPARABLE_NATIVE_STATE`. This does not establish pixel equivalence; text/font raster differences remain visible. Exact screenshots, SHA-256 values, overlays, heatmaps, logs, and artifact IDs are indexed in [P2C-1/P2C-2 settings evidence](P2C-1_SETTINGS_UI_REPORT.md).

## 9. P2C-3 原生视觉差异采集补充

- 固定基准仍为 v3.5.2 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。英文 `IDC_REGIONS=124` 后缀按固定源码保持 `win8.1+` / `win10+`；简中差异见 [P2C-3 来源语言矩阵](P2C-3_SOURCE_LANGUAGE_MATRIX.md)。
- Windows 侧从 `WM_GETFONT`/`LOGFONT`、GDI `TEXTMETRIC`/文本范围、`LVM_GETITEMTEXTW`、Win32 控件文本与 ComboBox 当前项读取实际值；Linux 生产 Canvas 导出 Pango/字体回退和布局度量。字体、文本、几何及控件绘制分开记录。Windows 的 `GetTextFaceW` 仍报告逻辑别名 `MS Shell Dlg 2`，不能将该值当作已证实的物理字体文件。
- 源码 `b4b5de5` 的 exact-source 截图配对已将 `IDC_REGIONS` 顶部/中间/底部 EN/zh-CN 资源 ID 行纳入比较：184/184 控件/行几何 PASS，最大绝对分量偏差 1.5 px。文字墨迹框偏差单独归类；简中原版未翻译差异不算几何误差。
- 主菜单、清理区域弹出菜单、尝试托盘上下文菜单及来源命令暗色主题截图由隔离 Windows runner 采集。真实清理确认/结果对话框若必须执行内存清理才能出现，保持 `BLOCKED`，不触发动作。
- 当前配对基准来自 Windows run `40312777621` artifact `12645980196` 与同源码 Xvfb run `40312777567` artifact `12645029581`；逐页变化率、字体宽度差、原生中文显示和截图状态见 [P2C-3 视觉报告](P2C-3_VISUAL_FIDELITY_REPORT.md)。P2C-3 候选按钮光栅/文字墨迹修正仍待新 HEAD CI 核验；全页 changed-pixel 比例只作前后比较，不构成 1:1 验收。

## 10. P2C-4 字体、暗色控件与菜单证据

- 固定基准仍为 Windows Mem Reduct v.3.5.2 / `2f5c55d15f69a2c88859e036b16c6f4c971e1968`；P2C exact-head run `40373093546`、生产 Xvfb run `40373092535`、build/safety run `40373092652` 均关联提交 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。
- 暗色主题包含五页 × English/简体中文十组 Windows/Linux 558×335 配对；全部有原始截图、并排、overlay、difference、heatmap 和 SHA。changed-pixel 比例：General `10.2327% / 51.6086%`、Memory cleaning `12.6625% / 73.2932%`、Appearance `11.6311% / 54.3027%`、Tray icon `11.2534% / 54.6584%`、Advanced `5.9434% / 46.0889%`（English / 简中）。十项均为 `CAPTURE_AND_COMPARE_PASS; PIXEL_IDENTITY_NOT_PASS`。Linux 是应用 Canvas 暗色配色，GTK 主题仍固定 `Adwaita:light`；简中差异率也包含原版来源文本与 Linux 完整本地化的差异，不能解释成字体问题。
- 本轮隔离 Liberation Sans 候选测试：English 66 个同源控件样本 64 改善、1 退化、1 不变；简中 54 个样本 12 改善、12 退化、30 不变。candidate 英文全页差异只改善 0.0578–0.4093 个百分点，简中基本不变或轻微回退；未改变生产字体、字号或窗口几何，未把 Windows 专有字体打包。GDI 仍报告逻辑 alias `MS Shell Dlg 2`，物理字体文件身份未证实；Pango 默认匹配 DejaVu Sans / Noto Sans CJK SC。
- 同 head 对照回归：Win32 资源矩形 `184/184 PASS`，最大分量偏差 `1.5 px`；导航当前选中区域 `10/10 PASS`；树状点线像素 `102/102 PASS`；离散控件栅格 `36/36 PASS`。这几类局部验收不表示全页像素一致；总体暗色变化率仍超过像素相等目标。
- XTEST 实际展开 Linux File / View / Settings / Help 英中菜单及 View 语言子菜单，执行键盘导航、Enter/Escape、分隔槽跳过和安全的 clean dropdown 外观测试；清理菜单项保持禁用。Windows runner 只采集到四个英文主菜单；tray menu 未被 UI Automation 暴露，清理 popup 没有安全展开路径，确认/结果弹窗需要真实清理才能打开，均保持 `BLOCKED`。
- 当前截图 artifact 和哈希清单在 [P2C-4 报告](P2C-4_FONT_THEME_MENU_REPORT.md) 索引；`12` 项简中来源差异仍 `DECISION_REQUIRED`，不擅自替换 Linux 当前完整中文翻译。没有执行清理、Polkit、Swap/ZRAM、root GUI 或活动桌面操作。
