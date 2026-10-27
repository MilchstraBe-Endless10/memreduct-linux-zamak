# ADR-0004 草案：无特权语言与主题偏好持久化

- **状态：PROPOSED / 待 ChatGPT 与用户复审；尚未批准，也尚未实现。**
- **阶段：P3A-pre。**
- **跟踪：**[Issue #8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)。
- **范围：**只持久化界面语言与应用亮/暗主题；不包含任何系统或内存管理设置。
- **基线：**Zamak `0.2.0`，P2C 复审基线 PR #7 `556a727c100aad4af1a25c48c10513c4dd42c5c7`。

## 背景

当前语言和应用 Canvas 主题在进程生命周期内可以切换，但应用重启后不保证恢复。本阶段只增加两项普通用户偏好：`language` 与 `theme`。官方 Mem Reduct v3.5.2 的默认语言为简体中文；当前应用默认配色为亮色，因此建议新配置的默认值为 `zh-CN + light`。

查阅 Zamak v0.2.0 官方标准库文档 [`std/fs` / `std/path`](https://github.com/VeyraLux/Zamak/blob/v0.2.0/book/en/src/standard-library.md#files-and-paths-std-fs-and-std-path)：有 `read/write/append`、`exists/is_file/is_dir/size`、`list/mkdir/remove/rename` 等接口；文档没有列出 `openat`/`O_NOFOLLOW`、权限设置、symlink-free `lstat`、文件锁或 `fsync` 接口。项目当前用 `std/fs.read` 读取 `/proc/meminfo`，没有配置文件写入模块。基于这个可核验接口边界，不能仅凭 `std/fs.write` 声称实现了 no-follow、0600 权限和原子持久化。

## 决策提案

### 1. 路径与权限

优先读取绝对且非空的 `$XDG_CONFIG_HOME`，配置文件位于：

`$XDG_CONFIG_HOME/memreduct-zamak/preferences.conf`

若 `XDG_CONFIG_HOME` 未设置、为空或不是绝对路径，则回退到 `$HOME/.config/memreduct-zamak/preferences.conf`。如果 HOME 缺失或路径无法安全打开，程序仍以默认值运行，但报告持久化不可用；不尝试提权或要求 root。

只允许固定的应用目录名与文件名，不允许配置内容提供路径。路径遍历组件（`.` / `..`）拒绝。对应用目录和配置文件使用不跟随符号链接的目录相对操作；若应用目录是 symlink、目标文件是 symlink/非普通文件或属主不符，拒绝读写并给出不包含绝对路径的错误反馈。可接受的 XDG 根目录必须是当前用户可访问的目录；最终实现需明确并测试对 symlinked home/config root 的处理，不得悄悄跟随配置文件叶节点。

- 应用专属目录权限：`0700`。
- 配置文件、锁文件、临时文件权限：`0600`。
- 仅用当前有效 UID；不调用 `sudo`、Polkit、特权 helper、shell 命令或 root 服务。
- 日志不打印用户主目录、XDG 路径或配置内容。

### 2. 最小配置格式

使用纯 ASCII、单行 key/value 文件，避免数据库和 JSON 运行时：

```ini
schema=1
language=zh-CN
theme=light
```

允许枚举：

| 键 | 允许值 | 默认值 |
|---|---|---|
| `language` | `en`、`zh-CN` | `zh-CN` |
| `theme` | `light`、`dark` | `light` |

文件大小上限建议 4 KiB。重复的已知键、缺失的 `schema`/必需字段、非法枚举、非 ASCII 控制字节或畸形行均视为损坏。schema 1 的未知键不进入运行状态，且不会对应任何动作；下一次明确保存时规范化为只包含受支持字段的文件。尤其 `auto_clean`、清理区域、helper 命令、管理员授权、Polkit、Swap/ZRAM 等键永远不被读取或解释。

缺少配置文件是正常首启状态，使用默认值且不报错。损坏、不可读或不支持的未来 schema 时，进程使用整组安全默认值并显示非模态状态反馈；启动时不自动重写或删除原文件。较新的未知 schema 会阻止普通偏好保存，避免旧版覆盖新数据；用户确认 Reset 后才允许明确写入 schema 1 默认值。

### 3. 写入事务、Reset 与多实例

`std/fs` 负责语言级解析/序列化；用一个范围严格限定的 POSIX C 存储桥接封装固定路径的读取和事务写入，不暴露任意路径/命令接口：

1. 打开并校验用户配置根与 `memreduct-zamak` 目录；叶节点 no-follow，检查普通文件和 UID。
2. 通过 `preferences.lock` 和 advisory `flock` 序列化应用实例之间的写入。
3. 在同一目录以 `O_CREAT|O_EXCL|O_NOFOLLOW` 创建随机临时文件，模式 `0600`，完整写入并 `fsync`。
4. `renameat` 原子替换固定的 `preferences.conf`，随后 `fsync` 目录；失败时清理本次临时文件并保留旧配置。
5. 对语言或主题单项变更，在锁内重新读取最新配置并仅合并该字段，避免旧实例用陈旧快照覆盖另一个字段。Reset 在锁内整体写入默认值。相同字段的并发选择按成功提交顺序 last-writer-wins；已运行的其他实例不承诺热同步，需重启/重新打开后读取最新值。

如果目标文件已是 symlink、目录、非普通文件、UID 不匹配或路径校验失败，则不执行 rename/write；测试必须确认 symlink 指向的外部 sentinel 内容未改变。`renameat` 替换目标之前仍需在锁内 `fstatat(..., AT_SYMLINK_NOFOLLOW)` 核验类型；所有目标名为固定常量。

语言/主题变更只有在配置写入成功后才更新全局偏好和 UI；写入失败时保留旧界面状态并显示明确错误。启动读取失败时允许安全默认值运行，但应标记当前偏好未持久化。Reset 流程先显示确认；取消不产生 I/O、不改变状态；确认后仅把本阶段支持的语言/主题恢复默认，写入成功后再应用；失败时保留原 UI 与配置状态。禁用的其他设置不应被 Reset 文案暗示为已持久化或已恢复。

没有配置文件时默认无需创建它，直到用户明确切换语言/主题或执行 Reset。崩溃发生在 rename 前保留旧文件；发生在 rename 后文件为完整新版本。残留临时文件不得被读取为有效配置。

### 4. 版本迁移

写入 schema 1；解析和迁移是纯函数，与文件 I/O 分离。未来新增 schema 时逐版本显式迁移，迁移仅发生在内存中，只有用户触发写入后才原子保存新版本。未知更高版本不降级覆盖。

由于当前项目没有已发布的持久化文件格式，不存在真实旧版配置需要迁移。为满足迁移机制测试，计划使用一个明确标注为 **test-only synthetic v0 fixture** 的旧字段形式（`schema=0`、`locale=en|zh`、`dark=0|1`）测试迁移到 v1；它不是已部署格式，也不得声称有现存用户数据。实际若在实施前发现已有未入库配置格式，须先以真实样本替换该 fixture 并更新本 ADR。

### 5. UI 状态一致性

程序启动时在创建主窗口之前只加载一次偏好对象；主窗口、View 菜单和五个设置页均使用同一对象。English/简中始终独立切换，亮/暗主题继续使用现有 Canvas 调色板行为，不改变 GTK 全局主题。

偏好加载和持久化不得改变已验收的 558×335 设置客户区、Windows 对照几何、字体、菜单结构或导航位置。所有清理、自动清理、区域选择、定时器、快捷键、托盘后台状态、Polkit、Swap/ZRAM 仍按当前安全矩阵禁用；配置数据不能解锁任何这些路径。

## 备选方案

- **仅用 `std/fs.write` + 临时文件 + rename：拒绝作为完整安全方案。** 0.2.0 文档没有 no-follow、mode、fsync 与锁 API，且 `fs.read`/`fs.write` 会将路径安全责任留给调用方。
- **JSON/数据库：不采用。** 只有两个枚举偏好，格式和依赖不值得增加复杂度。
- **用户级配置但不原子写：不采用。** 崩溃或并发写入不能保证旧文件仍有效。
- **root helper / Polkit：禁止。** 普通 UI 偏好不需要系统权限。

## P3A 验收测试矩阵

所有文件测试均使用临时 HOME/XDG 配置目录和独立进程；不触碰真实用户目录。

| 类别 | 测试与期望 |
|---|---|
| 解析/默认值 | schema 1 合法解析；缺文件默认 `zh-CN/light`；默认文件不自动创建；必需字段缺失、重复键、非法枚举、超长/畸形文件均拒绝并使用默认值。 |
| 未知字段 | schema 1 未知字段忽略，不进入 AppState；伪造 `auto_clean=true`、`helper=...` 等键无任何系统动作；明确保存后输出 canonical known fields。 |
| 版本 | synthetic v0→v1 纯函数迁移；schema 1 identity load；未知高版本降级到默认并禁止普通保存；显式 Reset 才覆盖。 |
| 损坏/读取失败 | 损坏、不存在、权限拒绝分别返回不同状态；默认启动不崩溃，不自动覆盖原文件；用户可见反馈不泄露路径。 |
| 路径安全 | 相对 XDG 回退到 HOME/.config；`..` 拒绝；应用目录 symlink/配置文件 symlink/非普通文件拒绝；symlink 外部 sentinel 保持字节不变；错误路径测试不越出临时根目录。 |
| 原子写与崩溃 | 0600/0700 权限检查；临时写入/flush/rename 故障注入；rename 前失败后旧文件仍有效，rename 后只能读到完整新配置；残留 temp 不被当作配置。 |
| 并发实例 | 两进程同时分别改语言与主题，锁内按字段合并且两值最终同时保留；同字段冲突按提交顺序；Reset 与其他实例保存序列化且结果为完整快照。 |
| 语言/主题恢复 | 四种组合分别启动→退出→重启；主窗口、View 菜单、五个设置页状态全部一致。 |
| Reset 事务 | 确认后写入默认并更新 UI；取消保持文件/界面 SHA 状态不变；写入失败时不先更新 UI，显示失败。 |
| UI 回归 | 正式生产 Xvfb/XTEST 截图与键盘/菜单测试；五页中英文和主题状态；P2C 几何/导航/raster 全部原阈值回归。 |
| Wayland 生命周期 | 私有 Weston Headless/Pixman、独立 socket、客户端退出码与日志；无输入设备时只报告生命周期，不宣称 Wayland 输入 PASS。 |
| 无特权 | 隔离测试 `euid == uid`；测试过程无 helper/Polkit/真实内存清理、内核、Swap/ZRAM 写入；审查 config unknown-key 不会进入 `policy` 模块。 |

## 计划模块边界

- `src/preferences.zm`：schema、枚举、默认值、解析/序列化、纯迁移、测试。
- `native/preferences_store.c` / `.h`：固定路径用户级读写、安全路径打开、锁、原子保存；不包含 UI、清理动作或任意 command 参数。
- `src/main.zm`：启动时加载并共享偏好；语言/主题命令只有提交成功后更新状态。
- `src/settings_ui.zm`：Reset 确认流程委托 preferences transaction；页面几何不变。
- `tests/`：纯解析和迁移测试、临时目录 C 存储测试、隔离 GUI 四组合/Reset 恢复检查。
- `.github/workflows/`：在现有生产 Xvfb 与 Weston 工作流上增加无特权配置场景，不修改 `pull_request_target` 或权限范围。

本 ADR 未批准前，不实现上述生产模块；P3A 也不包括清理或特权能力。
