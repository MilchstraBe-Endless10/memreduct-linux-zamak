# P2C-3 上游简中显示差异矩阵

基准固定为 Henry++ Mem Reduct `v.3.5.2`，源码 tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。本表比较该版本的英文源码、简体中文资源、Windows 中文运行显示和当前 Linux Zamak 显示；后续 3.5.3 开发源码不作为本表依据。

“无简中资源”表示该字符串在 3.5.2 中是 `main.h` / `main.c` 硬编码文本，而非 `resource.rc` 字符串表中的可本地化 key。Windows 实际中文显示依据隔离 Windows Server 2022 runner 的 v3.5.2 原生截图，96 DPI、100% 缩放；当前 Linux 文案来自 `src/settings_ui.zm`。

| 原版资源 / 位置 | 上游英文 | 3.5.2 简中资源 | Windows 简中实际显示 | Linux 当前简中显示 | 差异类型 | 决策状态 |
|---|---|---|---|---|---|---|
| `IDC_REGIONS=124`, row 0 / `IDM_CLEAN_WORKINGSET=183` | Working set | 无；`main.h` 硬编码 | Working set | 工作集 | 原版未翻译 | `DECISION_REQUIRED` |
| `IDC_REGIONS=124`, row 1 / `IDM_CLEAN_SYSTEMFILECACHE=184` | System file cache | 无；`main.h` 硬编码 | System file cache | 系统文件缓存 | 原版未翻译 | `DECISION_REQUIRED` |
| `IDC_REGIONS=124`, row 2 / `IDM_CLEAN_MODIFIEDLIST=186` | Modified page list* | 无；`main.h` 硬编码 | Modified page list* | 已修改页列表* | 原版未翻译 | `DECISION_REQUIRED` |
| `IDC_REGIONS=124`, row 3 / `IDM_CLEAN_STANDBYLIST=187` | Standby list* | 无；`main.h` 硬编码 | Standby list* | 备用列表* | 原版未翻译 | `DECISION_REQUIRED` |
| `IDC_REGIONS=124`, row 4 / `IDM_CLEAN_STANDBYLISTPRIORITY0=188` | Standby list (without priority) | 无；`main.h` 硬编码 | Standby list (without priority) | 无优先级备用列表 | 原版未翻译 | `DECISION_REQUIRED` |
| `IDC_REGIONS=124`, row 5 / `IDM_CLEAN_MODIFIEDFILECACHE=185` | Modified file cache | 无；该项在固定 tag 的区域清单中 | Modified file cache | 已修改文件缓存 | 原版未翻译 | `DECISION_REQUIRED` |
| `IDC_REGIONS=124`, row 6 / `IDM_CLEAN_REGISTRYCACHE=189` | Registry cache (win8.1+) | 无；`main.h` 硬编码 | Registry cache (win8.1+) | 注册表缓存（Windows 8.1+） | 原版未翻译；后缀大小写按源码不同 | `DECISION_REQUIRED` |
| `IDC_REGIONS=124`, row 7 / `IDM_CLEAN_COMBINEMEMORYLISTS=190` | Combine memory lists (win10+) | 无；`main.h` 硬编码 | Combine memory lists (win10+) | 合并内存列表（Windows 10+） | 原版未翻译；后缀大小写按源码不同 | `DECISION_REQUIRED` |
| `IDC_ALLOWSTANDBYLISTCLEANUP_CHK=153` | Allow “Standby lists” and “Modified page list” cleanup on autoreduct | 无；`SettingsProc` 硬编码 | Allow “Standby lists” and “Modified page list” cleanup on autoreduct | 允许自动清理备用列表和已修改页列表 | 原版未翻译 | `DECISION_REQUIRED` |
| `IDC_LOGRESULTS_CHK=154` | Log cleaning results into a debug log | 无；`SettingsProc` 硬编码 | Log cleaning results into a debug log | 将清理结果写入调试日志 | 原版未翻译 | `DECISION_REQUIRED` |

## 决策说明

- 严格复刻原版时，简体中文 Windows 截图中的上述十项必须继续显示英文；将它们翻译为中文会降低对应页的截图相似度。
- Linux 产品已被明确要求支持 English 与简体中文独立切换；当前 Linux 中文显示遵循该产品要求，不改变这十项现有翻译。
- 因“复刻原版中文界面”与“完整简体中文”两项要求在这些资源上冲突，视觉验收将它们标记为内容差异，不能归为字体误差或计算像素一致。当前保留中文翻译是临时遵循完整本地化要求的行为；最终是否在该十项上优先逐字复刻，需要产品负责人明确决定。

## 证据边界

- 上游英文及未翻译项：固定 tag `src/main.h`、`src/main.c` 与 `docs/UI_INTERACTION_MATRIX.md` 的逐资源记录。
- Windows 中文显示：隔离 Windows runner 采集的 Mem Reduct 3.5.2 原生设置页截图；列表和 Advanced 页中这些字符串仍显示英文。
- Linux 中文显示：生产 Zamak `src/settings_ui.zm`；中间滚动状态比较会另行测量 Windows/ Linux 行文字墨迹框。
- 这张表描述文案差异，不意味着对应清理、自动清理或日志操作在 Linux 已实现。所有尚未审查的功能仍保持禁用。
