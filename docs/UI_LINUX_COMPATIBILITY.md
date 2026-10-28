# Windows 3.5.2 UI 与 Linux 能力映射

本表以 Henry++ Mem Reduct `v.3.5.2` 官方 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 为唯一上游基准。它描述 Linux 的数据/操作语义，不表示相关 UI 或后端已实现。

状态枚举与 [交互矩阵](UI_INTERACTION_MATRIX.md) 一致：`NATIVE`、`EQUIVALENT`、`UNSUPPORTED`、`NOT_IMPLEMENTED`。任何 Windows 内核 API 都必须单独证明 Linux 对应能力；名称相似不代表操作等价。

## 指标和内存对象

| Windows 3.5.2 项目 | Linux 数据/接口 | 差异与用户可见说明 | 权限 | 状态 |
|---|---|---|---|---|
| Physical memory | `/proc/meminfo`: `MemTotal`、`MemAvailable`；used 估算为 total − available | `MemAvailable` 是 Linux 可分配量估算，不是 Windows 空闲页计数的逐项复刻；显示公式和来源 | 无 | `EQUIVALENT` |
| Pagefile | `/proc/meminfo`: `SwapTotal`、`SwapFree` | 只显示 Swap 占用类比，不称为 Windows Pagefile；不执行 `swapoff` | 只读，无 | `EQUIVALENT` |
| System working set | Linux 没有同定义的全系统 Windows working-set 值 | 不用 `Cached`/`SReclaimable` 冒充。若显示缓存估值，应使用单独准确标签 | 无 | `UNSUPPORTED` |
| System file cache metric | `Cached`、`SReclaimable` 等 `/proc/meminfo` 字段可作为单独缓存估值 | 字段不与 Windows `SystemFileCacheInformationEx` 一一对应，也不等同“可释放量” | 只读，无 | `EQUIVALENT`（仅估值展示） |

## 八个 Windows 清理选择

以下均保留原版 UI 文案和 resource ID 供高保真呈现；没有 Linux 等价实现时必须禁用并解释，不能伪装成功。

| Windows resource ID / 操作 | Windows 3.5.2 API | Linux 对应与限制 | 特权边界 | 状态 |
|---|---|---|---|---|
| `IDM_CLEAN_WORKINGSET=183` — Working set | `MemoryEmptyWorkingSets` | Linux 按进程回收不等价于全系统 Windows 工作集清理；不得改用 `drop_caches` | 候选接口需另行安全审查 | `UNSUPPORTED` |
| `IDM_CLEAN_SYSTEMFILECACHE=184` — System file cache | `SystemFileCacheInformationEx` | `drop_caches` 是粗粒度页缓存驱逐，不是同一 Windows 操作 | 写内核接口需显式授权；本阶段不执行 | `UNSUPPORTED` |
| `IDM_CLEAN_MODIFIEDFILECACHE=185` — Modified file cache | `_app_flushvolumecache` | `sync` 或 `drop_caches` 均不等价；可能引发实际写回 | 不在本阶段执行写操作 | `UNSUPPORTED` |
| `IDM_CLEAN_MODIFIEDLIST=186` — Modified page list | `MemoryFlushModifiedList` | Linux 无同结构列表与相同语义 | 不可映射为 `drop_caches` | `UNSUPPORTED` |
| `IDM_CLEAN_STANDBYLIST=187` — Standby list | `MemoryPurgeStandbyList` | Linux LRU/page cache 不等于 Windows Standby list | 不可映射为 `drop_caches` | `UNSUPPORTED` |
| `IDM_CLEAN_STANDBYLISTPRIORITY0=188` — Standby list (without priority) | `MemoryPurgeLowPriorityStandbyList` | Linux 无 Windows 优先级备用页列表 | 无安全等价动作 | `UNSUPPORTED` |
| `IDM_CLEAN_REGISTRYCACHE=189` — Registry cache | `SystemRegistryReconciliationInformation` | Linux 无 Windows Registry cache 对象 | 不适用 | `UNSUPPORTED` |
| `IDM_CLEAN_COMBINEMEMORYLISTS=190` — Combine memory lists | `SystemCombinePhysicalMemoryInformation` | Linux 无同名 Windows 内存列表合并操作 | 不适用 | `UNSUPPORTED` |

## 策略、提示与桌面能力

| Windows 行为 | Linux 处理约束 | 权限 | 状态 |
|---|---|---|---|
| 阈值/间隔自动清理 | 条件策略需先有冷却、去重、取消和可观察性；特权动作未独立安全验收前保持关闭 | 不能后台保存/复用 Polkit 凭据 | `NOT_IMPLEMENTED` |
| UAC warning bypass | 不提供“跳过 Polkit”选项；授权拒绝和取消须分别反馈 | Polkit 对每项动作独立授权 | `UNSUPPORTED` |
| Global hotkey | 普通窗口内快捷键可实现；Wayland 全局热键需 portal/桌面集成，不能假称全局注册 | 无 | `NOT_IMPLEMENTED` |
| Tray icon/menu/click/middle-click | 需实际验证 StatusNotifierItem watcher、菜单、激活方式和桌面通知；中键不是所有 Wayland 桌面都支持 | 无 | `NOT_IMPLEMENTED` |
| Windows Task Manager action | Linux 发行版/桌面没有统一目标应用；不得启动任意命令替代 | 无 | `UNSUPPORTED`（原动作） |

## 安全不变量

- GUI 保持普通用户权限；特权 helper 只接收固定枚举，禁止任意命令、路径和 PID。
- 不在 CI 或未经批准的主机上写真实 `/proc/sys/vm/drop_caches`；不重置 Swap/ZRAM，不安装常驻 root 服务。
- 清理结果只报告 helper 退出状态与可观测指标变化，不能声称准确释放量或零性能影响。
- 不把八个 Windows 清理选项合并映射为单个 Linux 操作。

逐控件文本、资源坐标、处理函数、菜单位置、完成状态和验证 ID 见 [UPSTREAM_UI_BASELINE](UPSTREAM_UI_BASELINE.md) 与 [UI_INTERACTION_MATRIX](UI_INTERACTION_MATRIX.md)。
