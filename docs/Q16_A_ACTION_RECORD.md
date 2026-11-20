# Q16 A 线 — 均衡清理的动作记录

基线 `d9122cb`。本文说明「均衡清理」一次点击记录了什么、每个字段怎么填、什么时候是「未观测」，以及怎么读那一行可复制文本。

## 1. 为什么要有这份记录

在此之前的均衡清理只有两种可观测状态：**做了** 和 **没做**。用户看不到：

- 这次请求编号是多少；
- 实际纳入的是哪几个 app scope（以及它们在扫描列表里的索引）；
- 写的是哪个文件的绝对路径；
- 每次写入请求了多少字节、写进去的完整文本是什么；
- 内核返回了什么、errno 是什么、属于哪一类（EAGAIN/EINVAL/短写……）；
- 如果什么都没写，是哪一条拒绝。

这些数据下层一直在测（`mr_pol_*` 的判定、`mr_bal_exec_*` 的批次计数、写入接缝记录的 errno），但在交给界面前被丢掉了。Q16 A 线把它们接起来，不改任何一个判定。

## 2. 数据从哪来（复用已有的接缝）

| 数据 | 来源 | 说明 |
|---|---|---|
| 请求 ID | `native/balance_bridge.c` 的 `action_next_id` | 进程内单调递增，每次点击 +1 |
| 确认的 scope 集 | 已有的 `policy_enrolled[]` | `action_begin()` 在纳入循环之后读取它 |
| 写入目标路径 | `mr_bal_bridge_scope_root()` + scope 名 + `memory.reclaim` | 与执行器构造路径的方式一致 |
| 单次写入字节数/文本 | `native/balance_reclaim.c` 的写入环形日志 | 所有 reclaim 写入都经过 `mr_bal_write()` 这一个入口 |
| 返回值 / errno / 分类 | 同上 | 测试用 `mr_bal_set_write_errno` 注入，不需要真实 cgroup |
| 未执行原因 | 由 `action_close_auto()` 从本轮实际状态推导 | 不让 GUI 自己复述分支顺序 |

新增的只有**查询**函数：`mr_bal_write_last_*` / `mr_bal_write_log_*`（reclaim 侧）与 `mr_bal_bridge_action_*`（bridge 侧）。现有函数签名一个都没改，11 个策略阈值（`mr_pol_config` 字段值）一个都没动。

## 3. 字段表

「未观测」不是「不知道」，而是**这条信息在这个进程里还没有产生过**。它有自己的取值，不会被写成 0 或空串冒充数据。

### 3.1 请求 ID

| 字段 | 访问器 | 成功 | 拒绝 | 未观测 |
|---|---|---|---|---|
| 请求 ID | `mr_bal_bridge_action_id()` | ≥1，单调递增 | ≥1（拒绝也有 ID） | `0` |
| 是否已有记录 | `mr_bal_bridge_action_observed()` | 1 | 1 | 0 |

一次点击一个 ID，从不复用。ID 在「开始」时分配、在任何判定之前，所以**被拒绝的点击同样有 ID** —— 这正是最需要解释的那种情况。

### 3.2 确认的 scope

| 字段 | 访问器 | 成功 | 拒绝 | 未观测 |
|---|---|---|---|---|
| 数量 | `mr_bal_bridge_action_scope_count()` | ≥1 | 0（无纳入）或 ≥1（纳入后被策略拒） | 0 |
| 名字 | `mr_bal_bridge_action_scope_name(i)` | `app-x-1.scope` | 同左 | `""`（越界） |
| 可读标签 | `mr_bal_bridge_action_scope_label(i)` | 解码后的应用名 | 同左 | `""`（越界） |
| 扫描列表索引 | `mr_bal_bridge_action_scope_index(i)` | ≥0 | 同左 | `-1`（越界） |

「确认」指**在本次请求开始时就已纳入**，而不是「最终被写入的」。所以「纳入了 1 个、被策略拒」会得到 `scopes=1 writes=0 reason=policy:no_target`，这是两件不同的事实，记录不合并它们。

索引是**扫描列表中的位置**（`mr_bal_bridge_scan_scopes()` 的下标），不是纳入顺序。策略层用纳入顺序、bridge 层用扫描顺序，记录里统一用后者，调用方不用记哪套是哪套。

### 3.3 实际写入目标

| 字段 | 访问器 | 成功 | 拒绝 | 未观测 |
|---|---|---|---|---|
| 绝对路径 | `mr_bal_bridge_action_target()` | `/sys/fs/cgroup/.../app-x-1.scope/memory.reclaim` | 无 scope 时 `""` | `""` |

格式固定为 `<root>/<scope>/memory.reclaim`，与 `mr_bal_exec_open_scope()` + `openat("memory.reclaim")` 组合出来的路径同源。

**路径存在不等于写过。** 判断有没有写必须看 `mr_bal_bridge_action_write_count()`。一个「纳入了但没写」的回合会同时有路径和 0 次写入，这不是矛盾。

### 3.4 请求量

| 字段 | 访问器 | 成功 | 拒绝 | 未观测 |
|---|---|---|---|---|
| 写入次数 | `mr_bal_bridge_action_write_count()` | ≥1 | 0 | 0 |
| 第 i 次字节数 | `mr_bal_bridge_action_write_bytes(i)` | 等于文本长度 | — | `-1`（越界） |
| 第 i 次完整文本 | `mr_bal_bridge_action_write_text(i)` | `33554432 swappiness=0` | — | `""`（越界） |

文本就是交给内核的那一串，恒为 `"<bytes> swappiness=0"`：`swappiness=0` 是「只回收文件页」的唯一手段，没有它就等于把匿名页压出去。记录保留原文而不是重新拼装，所以看到的就是写下去的。

### 3.5 内核返回

| 字段 | 访问器 | 成功 | EAGAIN | 短写 | 未观测 |
|---|---|---|---|---|---|
| 返回值 | `mr_bal_bridge_action_write_result(i)` | 等于字节数 | `-1` | 小于字节数 | `-1`（越界） |
| errno | `mr_bal_bridge_action_write_errno(i)` | `0` | `11`（EAGAIN） | `0` | `0`（越界） |
| 分类 | `mr_bal_bridge_action_write_class(i)` | `accepted` | `eagain` | `short_write` | `not_observed` |

分类的完整取值：

```
accepted        返回值等于请求长度
short_write     返回非负但不等于请求长度（本树据此按 EIO 处理，不是成功）
eagain          内核表示「文件页没得给了」
einval          内核不接受该参数串
enomem / ebusy / eperm / eacces / enoent / eio
failed_no_errno 失败但没有 errno（不该出现，出现即说明接缝被绕过）
failed_other    上面之外的 errno
not_observed    这个索引没有观测过
```

短写单列一类，因为一棵会拒绝短写的树把短写报成 accepted 就是在撒谎。

### 3.6 未执行原因

| 字段 | 访问器 | 取值 |
|---|---|---|
| 原因 | `mr_bal_bridge_action_reason()` | 见下 |
| 是否已知 | `mr_bal_bridge_action_reason_known()` | 1 = 确实记录过；0 = 未观测 |

```
none            写了，无需解释
no_scope        没有纳入任何 scope（什么都没有成为目标）
not_run         有 scope，但策略根本没被问到
policy:<stop>   策略拒绝，<stop> 是它自己的稳定名
                 （no_target / target_met / pressure_worse / telemetry_missing /
                   cooldown / budget_spent / refault / cancelled / no_trigger）
write_skipped   策略同意、且写了一次却没到达接缝（这是故障，不是拒绝）
not_observed    本轮还没结束，或这个进程从未发起过请求
```

原因是 `action_close_auto()` **从实际状态推导**的，不是由 GUI 传话。GUI 若要自己复述，就得把策略的分支顺序抄一遍，抄错时记录会说谎。

## 4. 一行可复制文本

`mr_bal_bridge_action_export()` 返回一行。**状态区是自绘 canvas，文字选不中** —— 所以这一行同时被
`mr_bal_bridge_action_export_path()` 写到文件里，界面显示的是「可复制：<路径> <记录本身>」。用户复制
文件内容即可，不需要手工抄。

文件位置：`$XDG_STATE_HOME/memreduct/balanced-last.txt`（未设该变量时用
`$HOME/.local/state/memreduct/balanced-last.txt`），目录 0700、文件 0600，每轮覆盖（记录的是「上一次点击」，
不做无界追加）。目录或文件写不出来时路径为空串，界面退回只显示记录本身 —— 便利性失效，信息不失效。

例：

```
MEMREDUCT-BALANCED id=7 scopes=1 scope[0]=com.google.Chrome#3 target=/sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service/app.slice/app-com.google.Chrome.scope/memory.reclaim writes=1 w[0]={bytes=28 text="33554432 swappiness=0" ret=-1 errno=11 class=eagain} reason=none
```

怎么读：

1. `id=7` —— 第 7 次点击。
2. `scopes=1 scope[0]=com.google.Chrome#3` —— 确认了 1 个 scope，是扫描列表第 3 位的 Chrome（`#3` 就是这个索引）。
3. `target=.../memory.reclaim` —— 写的就是这个文件的绝对路径。
4. `writes=1` —— 一共写了 1 次。
5. `w[0]={bytes=28 text="33554432 swappiness=0" ret=-1 errno=11 class=eagain}` —— 请求 32 MiB（文本 28 字节），内核返回 -1、errno=11（EAGAIN），分类 `eagain`：文件页不够给。
6. `reason=none` —— 写过了，没有要解释的拒绝。

同一个回合如果碰上「没有纳入任何 scope」，那一行是：

```
MEMREDUCT-BALANCED id=8 scopes=0 target=unavailable writes=0 reason=no_scope
```

`target=unavailable` 而不是某个路径：没有确认的 scope，就没有这次请求真正接近过的文件，编一个路径出来比留空更糟。

字段保证单行：每个字段在写入前都被限长且不含换行，粘进日志或终端不会被截断成多行。

## 5. 界面

状态区（footer 那条 `s.status`，y=183 DLU 处）在原有句子后面追加两段：

- 一句人话摘要（`mr_bal_bridge_action_summary()`），例如
  `请求 ID 7：向 com.google.Chrome 写入 28 字节「33554432 swappiness=0」；内核返回 -1（errno=11，eagain）。`
- 一行 `可复制：/home/<user>/.local/state/memreduct/balanced-last.txt MEMREDUCT-BALANCED id=7 ...`

文件里的内容就是那行 `MEMREDUCT-BALANCED ...` 本身（不含 GREP 前缀、不含路径、恰好一个换行）。

**只加文字，不改布局几何**：没有新增控件、没有改变任何 `ui_geometry` 计算、没有动窗口尺寸。

拒绝路径也显示记录（原先那里只有「没有纳入任何应用」一句话），因为「为什么没做」正是用户要看的东西。

## 6. 测试

`tests/test_q16_action_record.py`，10 个用例，全部通过注入接缝驱动：

- `mr_bal_bridge_test_set_scope_root` 用临时目录冒充 app.slice，scope 名、标签、索引和绝对目标路径都是真实构造出来的字符串；
- `mr_bal_set_write_hook` + `mr_bal_test_set_write_errno` 注入返回值与 errno，包括 EAGAIN、EINVAL 和短写。

不读 `/sys`、不写真实 cgroup、不碰 `/proc/sys/vm`。

覆盖：

| 用例 | 覆盖 |
|---|---|
| `test_a_fresh_process_reports_no_request_rather_than_a_zero` | 全部字段的「未观测」取值 |
| `test_a_successful_round_records_id_scopes_target_and_every_field` | 成功路径的每个字段 + 导出行的每一段 |
| `test_request_ids_are_monotonic_across_clicks` | ID 单调、不重复 |
| `test_a_click_with_nothing_enrolled_records_no_scope_and_says_so` | `no_scope` 拒绝 |
| `test_a_policy_refusal_names_the_policy_stop` | `policy:<stop>` 拒绝 |
| `test_an_eagain_write_records_return_errno_and_class` | EAGAIN 的返回值/errno/分类 |
| `test_an_einval_write_is_distinguished_from_eagain` | EINVAL 不与 EAGAIN 合并 |
| `test_a_short_write_is_not_reported_as_accepted` | 短写不算成功 |
| `test_the_copyable_line_is_actually_written_to_a_file` | 导出文件确实写出、0600、单行、与记录逐字节一致 |
| `test_the_export_path_is_empty_when_no_home_is_known` | 没有 HOME/XDG 时不谎报路径 |

## 7. 明确没有做的事

- 没有改 11 个策略阈值（`mr_pol_config` 字段值）。
- 没有改预算、没有改 scope 选择算法、没有扩大范围。
- 没有碰 Swap / ZRAM / Polkit，没有推送、没有部署、没有合并。
- 没有新增任何会写入的代码路径：记录只观察，不决定。
- **未实现**：读写目标路径的 `openat` 失败（EBADF 一类）不进入写入日志，因为那种失败在 `openat` 就返回了、根本没到 `mr_bal_write()`。这一类失败目前只体现为 `mr_bal_bridge_run()` 返回 0，记录里表现为 `writes=0` + `write_skipped`，**没有单独的 errno 字段**。要补需要给 `mr_bal_exec_open_scope` 也加一个观测点，那超出「不改现有函数签名」的约束，因此留作后续。
