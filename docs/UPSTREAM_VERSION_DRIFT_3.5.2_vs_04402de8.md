# 上游版本差异报告：Mem Reduct 3.5.2 发布标签与后续开发快照

日期：2026-10-25。用途：纠正 P2A 上游行为基准。权威发布源码固定为官方 tag v.3.5.2，commit 2f5c55d15f69a2c88859e036b16c6f4c971e1968。旧调查快照 04402de8b6a8ff9d1c5517c675d87c346655323c 仅用于分析发布后差异，不能再用作 3.5.2 规范。

## 1. 版本身份与审计方法

- 上游仓库：Henry++ Mem Reduct，https://github.com/henrypp/memreduct
- GitHub tag v.3.5.2 直接解析到 2f5c55d15f69a2c88859e036b16c6f4c971e1968；官方 Release 于 2025-04-01 发布。
- 固定后续源码快照为 04402de8b6a8ff9d1c5517c675d87c346655323c；Git 对比确认 tag 后有 46 个提交。
- 本次用官方仓库 Git tree 和 GitHub compare 重新核查指定文件；UTF-16 简中资源按 BOM 解码后重新解析英文资源键、简中键和各值，不沿用此前 92/92 结果。
- 官方 release assets 包括 setup、portable、PDB、setup 签名与 SHA-256 清单。没有在本轮运行 release executable；本机没有 Windows/Wine，因而这不是截图采集证据。
- 固定链接：[3.5.2 tag](https://github.com/henrypp/memreduct/tree/2f5c55d15f69a2c88859e036b16c6f4c971e1968) · [release](https://github.com/henrypp/memreduct/releases/tag/v.3.5.2)

## 2. 指定文件逐项差异

| 文件 | 3.5.2 tag | 后续 04402de8 | 对 P2A 规格的影响 |
|---|---|---|---|
| src/app.h | APP_VERSION=3.5.2、APP_VERSION_RES=3,5,2,0，版权年 2025 | 两个版本字段升为 3.5.3，版权年 2026 | 证明后续树已不是 3.5.2；冻结标签源码，不从后续实现反推发布版行为。 |
| VERSION | memreduct=3.5.2；语言包 URL 带时间戳 1743321307 | 应用版本仍为 3.5.2；语言包 URL 时间戳改为 1785854700 | 单独看 VERSION 会误判开发树版本；必须同时核对 app.h/tag。 |
| src/resource.rc | 224×214 DLU 主窗、五页各 260×168 DLU；File/View/Settings/Help 菜单、托盘菜单、F1/F2/Alt+F4 accelerator 和 split button 均在此版本 | 精确 diff 仅在 STRINGTABLE 追加 12 行（10 个新 IDS）；对话框模板、坐标、菜单树、accelerator 和 split-button 声明未改变 | 几何/静态菜单结构可直接以 tag 为准；托盘动态菜单和处理逻辑仍需看 tag 的 main.c。 |
| src/resource.h | 设置壳符号 IDC_NAV=104、IDC_RESET=105、IDC_CLOSE=106；其余控件和菜单 ID 为 tag 数值 | 这三个符号被移除；后续控件与 IDM 命令数值整体前移 3；保留的 IDS 定义数值不变，新增 IDS 89–98；其中 IDS 45–50 是未在 RC、语言文件或代码中引用的遗留宏，不计入活动字符串 | 矩阵必须保留 tag 的资源 ID。RC 模板没有定义 NAV/RESET/CLOSE；移除宏本身不能证明通用设置壳可见按钮被删除。 |
| src/main.c | 3.5.2 发布交互处理；与后续比较约 +388 / -574 行，包含重排与 API 调整 | 46 提交间的用户可见行为差异见第 3 节 | P2A 事件/菜单基准只采纳本列 tag 行为；不要把后续修复当成 3.5.2 原行为。 |
| src/main.h | 8 个内存区域标题为硬编码英文；清理 mask 使用带下划线名称 | 移除 8 个标题宏；重命名 mask 宏但 8 个 bit 值、默认 mask 和 freeze mask 不变；加入 30 秒自动清理 cooldown 常量 | 3.5.2 中文运行会显示英文区域名；冷却机制是后续行为，不属于 tag。 |
| bin/i18n/Chinese (Simplified).ini | 82 个键、82/82 覆盖；80 个值与英文不同 | 增至 92 键；新增 10 个翻译项，并翻译原先仍为英文的两个值 | 项目双语要求必须自己补齐，不能声称 3.5.2 已覆盖全部运行时英文。 |
| images/memreduct.png | SHA-256 d165c86f8398161be6abdfac8557829f5d45bfbcbee12a87a71e13a38f78ce38 | SHA-256 完全相同 | 官方仓库 PNG 可留作插画参考，但无 Windows/采集元数据，不是冻结运行截图。 |

3.5.2 resource IDs 参见基准与交互矩阵；后续数值重排不改变原版控件/命令含义，但若误用会造成矩阵追踪错位。

## 3. main.c 中确认的可见交互变化

| 后续提交 | 经源码确认的变化 | 3.5.2 原行为与基准处理 |
|---|---|---|
| ae7bb84 — display modified file cache before reduct | 清理确认摘要新增已勾选的 Modified file cache | 3.5.2 确认摘要未列该区域；P2A 按 tag 记录遗漏，不复制后续行为。 |
| 566b333 — display ccleanup result in tray popup | 命令行清理结果现在遵循 BalloonCleanResults：开时托盘通知，关时模态框 | 3.5.2 命令行来源始终显示模态结果框。 |
| 45cb2b7 — tray/autoclean/settings fixes | 动态阈值菜单新增上限参数与边界处理；自动清理阈值及托盘 Warning/Danger 百分比 getter 从 1–99 改为 0–100；使用率自动清理加入 30 秒 cooldown；Tray antialias 设置开始影响字体质量；修正 tray mask DC 背景模式；Advanced 两个标签改用 locale；移除阻断零值颜色的检查 | 这些是后续修复。3.5.2 的阈值/菜单边界、自动清理重复触发、抗锯齿设置效果、颜色 0（黑色）选择行为和两项硬编码高级标签都按发布源码记录。 |
| f828e75 — localize memory region names | 新增 8 个内存区域资源键，设置列表、菜单、清理摘要读取语言资源 | 3.5.2 的 8 个清理区域名称来自 main.h 英文宏；中文运行仍出现英文。 |
| 其他 main.c 差异 | 大规模代码重排、类型/API/日志调用调整、动态数值处理等 | 除上表列出的可追溯界面/交互变化外，不把未确认语义的纯 diff 行当作产品规范；必要时仍须用发布 exe 复核。 |

### 窗口资源、菜单及控件影响摘要

- src/resource.rc 的窗口和菜单模板在两引用间没有结构差异：主窗仍为 224×214 DLU，五个设置页面仍为 260×168 DLU；资源菜单仍包含 File、View、Settings、Help，托盘仍有清理区域、阈值和间隔子菜单；accelerator 仍定义 F1→About、F2→Settings、Alt+F4→Show/Hide。
- 后续 resource.h 调整了控件/命令整数 ID 并删除三个设置壳宏；不能把这些数字带入 3.5.2 矩阵。设置壳仍通过通用 _r_settings_createwindow 路径创建，确切可见按钮必须从 3.5.2 可执行文件截图确认。
- 后续新字符串不会改变 RC 中的五页模板几何，但会改变区域清单、清理确认、托盘菜单与高级页的显示语言/文案。
- 3.5.2 Settings 区域清单、split dropdown、Tray 区域菜单的顺序须分别按 tag 的资源/代码记录，不可用后续本地化提交的列表顺序替换。

## 4. English 与简体中文资源重算

在官方 tag 对 src/resource.rc 默认英文字符串和 UTF-16 Chinese (Simplified).ini 重新解析的结果：

| 项目 | v.3.5.2 tag | 后续 04402de8 |
|---|---:|---:|
| 活动 IDS 英文资源键（resource.rc STRINGTABLE） | 82 | 92 |
| resource.h 中 IDS 宏总数 | 88；其中 6 个 ID 45–50 无 RC/语言/代码引用 | 98；6 个旧宏仍未使用，新增 10 个 |
| 简中资源键 | 82 | 92 |
| 键覆盖 / 缺失 / 额外 | 82/82 / 0 / 0 | 92/92 / 0 / 0 |
| 与英文值不同的简中值 | 80 | 92 |
| 简中值仍等于英文 | 2：IDS_TRAYACTIONSC_HINT=Single click:、IDS_UPDATE_SPEED=Speed | 0 |
| 新增字符串 | — | 10：2 个 Advanced 文案 + 8 个区域名 |
| 已有键补译 | — | Single click:→单击：；Speed→速度 |

resource.h 虽定义 88 个 IDS 宏，但 IDS_WORKINGSET_CHK、IDS_SYSTEMWORKINGSET_CHK、IDS_STANDBYLISTPRIORITY0_CHK、IDS_STANDBYLIST_CHK、IDS_MODIFIEDLIST_CHK、IDS_COMBINEMEMORYLISTS_CHK（ID 45–50）只在头文件定义；全仓库检索未见源码/语言包引用，resource.rc 也无对应活动字符串，属于未使用遗留 ID，不增加有效翻译键。

另有 10 个 tag 运行时文案不在这 82 个 STRINGTABLE 键内：src/main.h 的 8 个区域标题及 SettingsProc 的 2 个 Advanced 标签。tag 没有 English.ini；英文来自 resource.rc 默认资源与少量硬编码文本，其他语言包不是英文基准。3.5.2 Simplified Chinese 实际界面会有这 10 处英文，Linux 产品必须另行提供完整 EN / 简体中文切换，并把未翻译项目作为本项目补译，不能伪称源版本已有。

## 5. 截图证据与限制

- 官方仓库 PNG：338×400 RGBA，SHA-256 如上；图中为旧式俄文暗色窗口示意且数值区域被遮挡，没有可验证的 Windows 版本、DPI、缩放、主题采集状态、语言切换状态和采集过程。
- 两张用户参考图分别为主窗与设置页，但缺版本、可执行文件来源、Windows build、DPI/缩放、主题、日期等采集元数据；仅作待核参考，不作 3.5.2 像素基准。
- 官方 release 有 Windows setup/portable 二进制及签名/checksum 清单；当前没有 Windows/Wine 执行环境。本轮未从发布版程序取得主窗口双语、五页设置、菜单/托盘、清理确认/结果、亮/暗主题截图。
- 因此所有真实 Windows 页面/状态截图采集仍为 BLOCKED，Windows/Linux overlay、difference 与 2 px 几何检查为 NOT RUN。现有 PNG 及用户图片均不能替代此证据。

## 6. 结论

权威 UI 资源与源代码基准已纠正至官方 tag，旧 04402de8 仅作为发布后漂移资料；简中字符串审计已按 tag 重新执行。静态窗口/菜单资源没有在两引用间改版，但 ID 数值、动态事件、翻译、清理反馈和自动清理细节确实发生变化。由于缺少 release executable 的真实 Windows 截图，规格仍是源码基线审阅稿；不能判为像素验收或 P2A 正式验收通过。

## 7. 后续文案决策状态（2026-10-27）

本节是后续状态补充，不改写上面的历史源码审计结论。P2C 对照后来将 12 项原版简中来源差异标记为 `DECISION_REQUIRED`：8 个 Memory cleaning 区域名、2 个 Advanced 文案、`Single click:` 和一个语言显示名称。完整简体中文仍是产品目标，但这不等于所有建议译文已获批准；决策前保留 Linux 当前译文，并在文案矩阵中分别记录上游原文、原版实际显示和项目译文。
