# ADR-0004 草案：无特权语言与主题偏好持久化

- **状态：PROPOSED / 待 ChatGPT 与项目所有者复审；未批准、未实现。**
- **阶段：P3A-pre；本 ADR 不授权生产实现。**
- **跟踪：**[Issue #8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)。
- **范围：**仅持久化界面语言与应用亮/暗主题；不包含系统或内存管理设置。
- **审查基线：**PR #7 head `629fc3f88ee91d0cc3582d6be1789991fc3f72b6`；P2C-4R1 已接受的历史基线为 `556a727c100aad4af1a25c48c10513c4dd42c5c7`。
- **产品默认值：**建议 `zh-CN + light`，仍待所有者最终确认；没有确认前不得把它描述为已批准的产品决定。

## 背景与决策范围

语言和应用 Canvas 主题可在进程内切换，但当前不保证跨进程恢复。本提案只为两项普通用户偏好提供无特权持久化。P2C 的窗口尺寸、字体、菜单与导航几何不在本 ADR 的可变范围内。

Zamak 0.2.0 的 `std/fs` 文档列出基础读写、目录和重命名接口，但没有列出 `openat` / `O_NOFOLLOW`、文件权限、锁和 `fsync` 保证。因此，不能仅凭 `std/fs.write` 声称满足安全路径和崩溃一致性；提案使用固定路径、窄接口的 POSIX/Linux C 用户态存储桥接。桥接不得暴露任意路径、shell 命令、特权 helper 或内存清理入口。

## 1. 路径解析与文件安全

### 1.1 选择配置根目录

1. 若 `XDG_CONFIG_HOME` 非空、为绝对路径且不含词法 `.` / `..` 路径组件，选择该目录。
2. 若它未设置、为空或不符合上述格式，则使用绝对且非空的 `HOME` 下 `.config`。若 `HOME` 缺失、相对或无法安全打开，则返回 `PATH_UNAVAILABLE`，进程可用默认值运行，但不得尝试其他未经定义的路径。
3. 已选择的 XDG 根目录若不存在或无法打开，不再静默回退到 HOME，以免同一用户偏好被分裂到两个位置。
4. 配置根目录只作为可信路径锚点。HOME、XDG_CONFIG_HOME 及回退路径中的 `.config` 可以是用户有意配置的目录符号链接；打开后以目录 fd 为准，并验证目标是当前有效 UID 所有、可访问、非 group/world-writable 的目录。符号链接路径在启动时解析一次，不在后续操作中重新拼接完整路径。祖先目录可以由系统账户管理（例如 `/`、`/home`），其权限按系统目录惯例验证；真正承载数据的 app 目录另行严格校验。
5. `memreduct-zamak`、`preferences.conf`、`preferences.lock`、临时名和恢复备份名均是程序固定常量或由安全随机字节生成；配置内容不能决定路径。app 目录不得是符号链接。拒绝词法路径遍历；配置根 fd 之后所有应用对象操作均相对该 fd 执行。

数据路径：`<config-root>/memreduct-zamak/preferences.conf`。缺少 app 目录时只在配置根内用 `mkdirat(..., 0700)` 创建；遇到并发创建的 `EEXIST` 时重新以 no-follow 方式打开并重新校验，不能沿用未经检查的对象。

### 1.2 fd、符号链接与权限检查

- 先打开配置根目录 fd 并用 `fstat` 验证目录类型、UID、访问权限及非 group/world-writable 属性；后续操作使用目录 fd，不再使用拼接出来的绝对路径。
- 优先用 `openat2` 对单一固定子路径限制 `RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_SYMLINKS`。只在 `ENOSYS` 明确表示系统没有该 syscall 时退到等价的 `openat`：只接受单个固定 basename，配合 `O_CLOEXEC | O_NOFOLLOW`，并立即 `fstat` 校验。`EINVAL`、权限错误及其他失败均视为参数/安全条件异常并失败关闭，不以回退掩盖。若 `openat`、`O_NOFOLLOW` 或 fd 相对校验不可用，必须失败关闭；不得退到普通路径 `open()` / `rename()`。
- app 目录必须是 `euid` 所有的真实目录，权限严格为 `0700`，不得有额外链接或不安全对象。配置文件和锁文件必须是 `euid` 所有、普通文件、单链接、权限严格为 `0600`；不自动 `chmod` 修复既有的不安全对象。临时文件和恢复副本创建时请求 `0600`，再以 `fstat` 验证实际模式。
- 配置文件和锁文件的符号链接、非普通文件、UID 不符、链接数异常或模式不符分别返回安全对象/权限状态，不读取其内容、不跟随、不覆盖。错误反馈不包含绝对路径、配置内容或敏感环境变量。
- 锁文件使用固定 `preferences.lock`。仅在创建新文件时以 `O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW` 请求 `0600`，验证这是本进程创建且 UID/类型正确的 inode 后，允许对该新 inode `fchmod(0600)` 以抵消过严 umask；若对象已存在，则不得 chmod 修复，必须以 no-follow 方式打开并校验类型、UID、单链接及精确 `0600` 模式。再通过 `flock` 序列化本应用实例。释放时只 `LOCK_UN` 并关闭 fd，**永不 unlink 锁文件**；否则可能让并发实例锁住不同 inode。锁是协作式锁，外部程序直接改写配置不受它约束，故每次写入仍需校验和回读。新建临时文件和恢复副本也只能对本进程刚创建并验证的 inode 做 `fchmod(0600)`；既有文件权限错误一律拒绝而不自动改权。

### 1.3 配置文件格式

仅接受 ASCII、单行 key/value、UTF-8 无 BOM 的小文件，最大 4 KiB：

```ini
schema=1
language=zh-CN
theme=light
```

| 键 | schema 1 值 | 建议默认值 |
|---|---|---|
| `language` | `en`、`zh-CN` | `zh-CN`（待所有者确认） |
| `theme` | `light`、`dark` | `light`（待所有者确认） |

重复已知键、缺少 schema/必需键、非法枚举、NUL/控制字节、超长内容或畸形行是 `CORRUPT`。同一 schema 的未知键可以被忽略，但不得进入应用状态；下一次用户明确成功保存时规范化为仅包含已知键的 canonical 内容。`auto_clean`、清理区域、快捷键、helper 命令、管理员授权、Polkit、Swap/ZRAM 等任何其他键均永不解析为功能状态。

schema 0 迁移仅使用明确标记的 **test-only synthetic fixture**（`locale=en|zh`、`dark=0|1`）覆盖纯迁移函数；它不是已发布格式。除非未来 ADR 明确批准，生产读取不宣称支持 schema 0，也不声称已有迁移用户数据。schema 1 可原样读取。未知更高 schema 返回 `UNSUPPORTED_SCHEMA`，禁止旧版普通保存或无确认降级覆盖。

## 2. 读取结果、写入结果与状态转换

### 2.1 类型化结果

存储接口应将 I/O 事实与偏好值分开返回，不用单个布尔值表示成功：

```text
LoadStatus = ABSENT | VALID | CORRUPT | UNREADABLE | UNSUPPORTED_SCHEMA
           | UNSAFE_OBJECT | OWNERSHIP_OR_MODE | PATH_UNAVAILABLE

StoreStatus = COMMITTED_DURABLE | NO_CHANGE | FAILED_UNCHANGED
            | BLOCKED_RELOAD | VISIBLE_DURABILITY_UNCERTAIN
            | RECOVERY_REQUIRED
```

`LoadResult` 至少包含 status、有效值（仅 `VALID`，或 `ABSENT` 时返回建议默认值）、以及仅供本进程比较用的 generation/digest。`StoreResult` 至少包含 status、失败阶段、粗粒度错误类别、是否已经发生成功 rename、以及安全回读状态；UI 不展示绝对路径。错误码和偏好内容不写入公开日志。

### 2.2 保存事务顺序

普通语言/主题变更和有效配置的 Reset 必须在同一把锁覆盖下完成以下步骤：

1. 获取并校验配置根与 app 目录 fd；打开并校验稳定锁文件，取得 `flock`。
2. **锁内重新读取 `preferences.conf`**，区分缺失、有效、损坏、不可读、未来 schema、不安全对象、归属/权限错误。绝不使用启动时缓存的快照作为保存依据。
3. 缺文件时以待确认的默认值为基线；当前 schema 有效时只合并本次明确修改的字段。Reset 则产生完整默认快照。若结果和现存 canonical 内容相同，返回 `NO_CHANGE`，不重新写入。
4. 对损坏、不可读、未来 schema、不安全对象或权限/归属不符，普通保存返回 `BLOCKED_RELOAD` 或更具体的拒绝状态；不以默认值替代，也不尝试清理、修复或覆盖原件。
5. 在同一目录创建随机唯一临时文件 `O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW`，`0600`。完整写入；短写入循环推进 offset，`EINTR` 重试同一未完成系统调用；写入完成后 `fsync(temp_fd)`，关闭并验证错误状态。
6. 调用固定 basename 的 `renameat` 原子替换 `preferences.conf`。rename 成功的瞬间，新文件名即对其他进程可见。之后 `fsync(config_dir_fd)` 确认目录项持久性。
7. 目录 fsync 成功返回 `COMMITTED_DURABLE`；rename 前失败返回 `FAILED_UNCHANGED`，仅在确认仍是本事务创建的 inode 后尽力删除临时文件。不能删除不属于本次事务的临时对象。
8. rename 已成功、目录 fsync（非 EINTR 可重试完成）失败时返回 `VISIBLE_DURABILITY_UNCERTAIN`。此结果**不表示旧配置仍然存在**，也不表示 rename 已回滚。仍持锁时通过 no-follow fd 安全回读目标：若内容与预期 canonical bytes 完全相同，则运行时采用该可见值并标记“本次可见、跨崩溃持久性未确认”；若回读得到另一份有效值，则运行时采用实际读到的值并标记外部/状态冲突；若无法安全确认，则不猜测磁盘值，保留当前 UI 值、标记状态未知并暂时禁止后续设置写入，直到重新加载或用户明确恢复。无论哪种情况都向用户显示非模态但明确的状态反馈。
9. 若 `renameat` 返回失败，必须在仍持锁时安全回读并比较事务前 generation/digest。只有确认目标仍为旧版本或缺失时才可报 `FAILED_UNCHANGED`；否则报告 `VISIBLE_DURABILITY_UNCERTAIN`，不假设失败即未生效。

### 2.3 状态转换表

| 当前状态 / 注入点 | 命名空间事实 | 返回状态 | 运行时/UI 处理 |
|---|---|---|---|
| 锁内读取为缺失，尚未写临时文件 | 目标仍缺失 | 继续以默认值建立本次请求 |
| 锁内读取为有效 schema 1 | 读到当前内容 | 继续合并单字段或构造 Reset |
| 锁内读取为损坏/不可读/未来 schema/不安全对象/归属权限错误 | 原对象未修改 | `BLOCKED_RELOAD` 或具体拒绝状态 | 保持 UI；显示原因和恢复入口，不静默替换 |
| 临时创建、写入、文件 fsync 或 rename 调用前失败 | rename 尚未成功；旧目标保持原名 | `FAILED_UNCHANGED` | 保留旧 UI；清理本事务临时文件（经 inode 校验） |
| rename 返回错误 | 不以 errno 单独推断目标状态 | 安全回读后判定 unchanged 或 `VISIBLE_DURABILITY_UNCERTAIN` | 按回读结果处理；不自动重放整个事务 |
| rename 成功，目录 fsync 成功 | 新目标已可见且目录同步完成 | `COMMITTED_DURABLE` | 应用新值并提示成功 |
| rename 成功，目录 fsync 持续失败 | 新目标已可见；崩溃后持久状态未知 | `VISIBLE_DURABILITY_UNCERTAIN` | 按安全回读协调 UI；明确标记不确定，不声称旧文件仍在 |
| 回读/对象安全验证失败 | 不能确认当前目标状态 | `RECOVERY_REQUIRED` | 不猜测磁盘值；锁外阻止再次保存，显示恢复说明 |

### 2.4 重试规则与避免重复覆盖

- `EINTR` 对同一 `read` / `write` / `fsync` / `flock` 系统调用按剩余 buffer/offset 安全重试；短写入仅继续写剩余字节。写入 fd 不设置 `O_NONBLOCK`，意外 0 字节写入作为失败。
- 锁竞争可以等待锁释放；若将来设置超时，超时仅提示忙碌，不能绕过锁。
- 临时文件随机名碰撞只可有限次重新生成名称；永远不打开或覆盖碰撞到的对象。
- 在 rename 前发生 `ENOSPC`、`EDQUOT`、权限错误、文件 fsync 失败等，事务确认未改变目标时不自动重跑整笔事务；向用户反馈。用户显式重试时必须重新取得锁、重新读取/校验当前文件并基于最新内容构造新事务。
- 损坏、未来 schema、不安全路径、权限/归属不符和格式错误不是瞬时故障；禁止自动重试或降级覆盖。只有对象/权限/路径在外部修复后，用户重新发起操作才再次读取。
- rename 后目录 fsync 的 `EIO` 等持续错误属于提交不确定；禁止自动再次写入或再次 rename。若用户显式重试，先在锁内读取；只有目标字节摘要仍与这笔意图完全一致时，才允许仅重试目录 `fsync`，不得重写、再次 rename 或生成新恢复副本。目标已变化则采用新的安全读取结果并要求用户再次确认。
- 每次新用户动作都在锁内重读；当前内容已等于期望 canonical 值则返回 `NO_CHANGE`。这使重复点击/重新提交幂等，并防止旧窗口快照覆盖另一个实例较新的字段。

## 3. 损坏数据、Reset 与显式恢复

启动加载时：`ABSENT` 正常使用建议默认值且不自动创建文件；`VALID` 使用文件值；其他失败状态可用一组建议默认值运行，但必须显示对应状态、保持原文件不变，并禁止普通偏好保存，直到安全恢复或路径修复完成。不得把“默认值正在运行”写成“配置已恢复/已保存”。

普通设置保存在锁内重读后，只允许缺失文件或当前支持的有效 schema。遇到 `CORRUPT`、`UNREADABLE`、`UNSUPPORTED_SCHEMA`、`UNSAFE_OBJECT`、`OWNERSHIP_OR_MODE` 或 `PATH_UNAVAILABLE` 必须拒绝，不能悄悄以默认值覆盖。

Reset 是用户动作，不是启动修复：

- 有效 schema 1：展示普通 Reset 确认；确认后只重置语言和主题；取消没有 I/O 或 UI 状态变化。未知但允许忽略的 schema 1 字段会在这次明确保存时规范化掉。
- 无配置文件：展示确认后将建议默认值作为完整 schema 1 写入；取消不创建文件。
- 可读的损坏文件或未来 schema：使用独立的 **“备份并恢复默认值”**流程。先显示数据保留/降级说明；未来 schema 的文案必须明确说明旧版本 schema 1 将丢弃其不理解的字段，并要求专门的二次确认。确认后在锁内重新读取并安全验证原件，把原始字节精确复制到同目录随机唯一 `.bak` 文件（权限 0600；以流式复制、最多 64 KiB），对备份 fsync 文件并 fsync 目录；只有备份完整后才开始写入默认配置事务。源文件大于 64 KiB 或无法完整读取时拒绝应用内恢复，保留原件并说明需人工处理。任何备份失败均不改原配置。备份名冲突只生成新随机名，禁止覆盖。若后续默认配置事务 rename 前失败，原配置名仍保持有效，备份是额外副本；若 rename 后目录 fsync 失败，按 `VISIBLE_DURABILITY_UNCERTAIN` 规则处理且备份仍保留。
- 当前 UID 所有但不可读的普通文件、或因权限/模式不合规而不能安全读取/备份的对象，恢复入口仅说明如何由用户手动处理，不尝试重命名/删除/修改权限。符号链接、非普通文件或 owner 不符对象绝不跟随、备份、替换或删除；需要用户在应用外检查。

Reset 成功后只把语言和应用主题恢复到最终确认的默认值。不得暗示它重置或启用了清理区域、自动清理、托盘、快捷键、Polkit、Swap/ZRAM 或其它系统设置。

## 4. 多实例、崩溃与临时对象

- 锁的生命周期覆盖重新读取、字段合并、临时写入、文件 fsync、rename、目录 fsync 及不确定时的安全回读。参与应用的并发保存串行化。
- 不同字段的更新在锁内从最新有效快照合并；相同字段按锁内成功事务的先后次序 last-writer-wins。Reset 是完整快照并按获取锁的顺序执行。已有运行实例不承诺热同步；各实例只保证自己的 UI 反映自己的事务/回读结果，重启后读当前文件。
- 崩溃在 rename 前，目标路径仍为旧文件或缺失；残留临时文件不可作为配置读取。崩溃在 rename 成功后，新的目录项已对并发进程可见，但若目录 fsync 未完成，重启后持久结果不作承诺；启动只加载实际存在且通过格式/权限校验的文件，不依据日志推断。
- 临时文件名使用独立前缀和随机后缀；加载器只认固定 `preferences.conf`。只清理本事务创建且 inode 身份已验证的临时文件；启动时不按通配符删除未知残留文件。测试应验证残留 temp 不被读取，也不覆盖新文件。
- `preferences.lock` 在任何成功/失败/进程退出路径均不删除。内核在进程退出时释放 `flock`；下一实例打开同一个固定 inode 继续序列化。

## 5. 未来 schema 迁移

schema 迁移为纯函数，与路径和 I/O 分离。只有未来 ADR 明确添加真实迁移输入版本后，启动时才允许在内存中显式迁移，并仅在用户触发保存时写入新 schema。未来未知 schema 一律不自动降级覆盖。test-only synthetic v0 fixture 只验证转换函数，不代表上线迁移格式。

## 6. P3A 测试矩阵草案

全部读写和 GUI 测试使用临时 HOME / XDG 根目录、隔离子进程和非特权身份；不得触碰真实用户配置、活动 Hyprland 或系统内核接口。故障注入只在 test build 的存储层接口中提供，不允许生产环境变量/命令触发注入。

| 类别 | 测试与期望 |
|---|---|
| 解析和默认 | schema 1 合法解析；缺文件默认；建议默认未确认时测试 fixture 显式注入 expected default；不自动创建；缺字段、重复键、非法枚举、控制字节、超长、畸形均是 CORRUPT。 |
| 加载状态 | 分别断言 ABSENT、VALID、CORRUPT、UNREADABLE、UNSUPPORTED_SCHEMA、UNSAFE_OBJECT、OWNERSHIP_OR_MODE、PATH_UNAVAILABLE，不把它们合并成“缺省”。 |
| 未知字段/安全隔离 | schema 1 未知项不进入 AppState；伪造清理、helper、授权、Swap/ZRAM 字段没有任何动作；明确保存后只写 canonical 字段。 |
| 迁移 | v1 identity；test-only synthetic v0 纯迁移函数；生产 loader 不误称支持未批准格式；未知高 schema 阻止普通保存。 |
| 路径根 | XDG 缺失/空/相对时按约定回退；选中的有效 XDG 路径不可用时不得切换到 HOME；symlinked HOME、XDG_CONFIG_HOME、HOME/.config 作为根目录可安全打开并验证实际 dirfd；词法 `.` / `..` 拒绝；缺失目录并发创建后重新打开校验。 |
| app 路径安全 | app dir symlink、config/lock leaf symlink、FIFO/目录/设备节点、外部 sentinel、UID/mode/link count 错误均拒绝；sentinel 字节不变；openat2 与 openat fallback 走各自断言；无安全原语时 fail closed。 |
| 锁 | lock 必须普通文件、UID=euid、mode 0600、nlink=1；多实例锁竞争与释放；释放后 lock path/inode 仍存在；两实例同时修改不同字段最终保留两值；同字段按锁顺序；Reset 串行化。 |
| 写入完整性 | 0600/0700 验证；短写入推进 offset；EINTR 只重试当前系统调用；意外零写失败；文件 fsync 成功后再 rename；lock 期间重复用户操作得到 NO_CHANGE。 |
| 故障注入-rename前 | 临时创建、写入中断、文件 fsync 失败、rename 调用前注入失败分别验证旧文件 SHA/字节与可读状态仍有效，临时清理只针对本次 inode。 |
| 故障注入-rename | rename 返回失败后执行安全回读；仅在旧 generation 未变时报告 FAILED_UNCHANGED；目标状态不符时报告 UNCERTAIN；测试不得假定 errno 单独证明结果。 |
| 故障注入-rename后 | 成功 rename 后注入目录 fsync EIO；新文件名应在锁内回读可见，结果必须 VISIBLE_DURABILITY_UNCERTAIN，不可声称旧配置仍在；runtime 与 UI 按回读值/无法确认两条路径分别验证。 |
| 重试 | 自动不重播事务；空间恢复后的用户显式重试先重读；不确定状态只有摘要匹配时可仅重试目录 fsync；目标已被改变则不覆盖；文件相同返回 NO_CHANGE。 |
| 异常中止/临时物 | 在临时创建、部分写、文件 fsync、rename 前/后、目录 fsync 各断点杀测试进程；旧/新/不确定状态符合表；残留 temp 不被读；备份名碰撞不会覆盖任何已有对象。 |
| 损坏与恢复 | 启动/普通保存不覆盖损坏、不可读、future schema；可读损坏配置须先产生精确、0600、已 fsync 备份才允许 reset；备份最大 64 KiB；future schema 专门二次确认；备份失败/取消保持原字节；unsafe symlink/foreign owner 不恢复。 |
| 语言/主题 | en/zh-CN × light/dark 四组合分别重启；主窗口、View 菜单和五页状态一致；失败写入不得先更改 UI；不改变既有几何与字体。 |
| Reset UI | 确认、取消、写入失败、正常重置、损坏备份恢复、未来 schema 降级特殊确认分别测试。 |
| 回归与无特权 | 现有 Xvfb/XTEST、Weston 生命周期与 P2C 几何/导航/栅格原阈值回归；`euid == uid`；没有 helper、Polkit、内核写入、真实清理或 Swap/ZRAM 改动。 |

## 7. UI 一致性、方案边界与模块提案

最终实现若获单独批准，须在创建主窗口前读取一次偏好对象；主窗、View 菜单和五个设置页使用同一状态源。亮暗偏好只驱动现有应用 Canvas 调色板，不改 GTK 全局主题，不改变已验收 558×335 设置客户区、控件几何、字体、菜单结构或导航位置。`COMMITTED_DURABLE` 后应用保存值；预 rename 失败保留原 UI；rename 后不确定按第 2.2 节回读协调，不得以 UI 假装已回滚。

模块边界提案：

- `src/preferences.zm`：schema、枚举、建议默认值、解析/序列化、迁移纯函数。
- `native/preferences_store.c` / `.h`：固定路径用户级读取、fd 校验、锁、原子事务和状态码；无 UI、无清理动作、无任意路径/命令参数。
- `src/main.zm`：启动时加载并共享状态；语言/主题动作仅在存储事务结果允许后更新。
- `src/settings_ui.zm`：Reset 确认与恢复反馈；保持既有设置页几何。
- `tests/` 与 `.github/workflows/`：临时目录、存储故障注入、生产 Xvfb/Weston 回归；CI 权限保持只读，不加 `pull_request_target`。

不采用数据库、JSON runtime、非原子保存、root helper 或 Polkit。自动清理、内存区域、托盘后台、全局快捷键及 Swap/ZRAM 永远不属于本阶段。

本 ADR 仍为 **PROPOSED**。实现前须由 ChatGPT 独立复审并由项目所有者单独批准；本文件及 Issue #8 均不构成实现授权。
