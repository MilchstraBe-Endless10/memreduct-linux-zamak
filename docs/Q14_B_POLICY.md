# Q14 B 线：有条件的默认均衡策略

状态：实现完成，阈值待 A 线校准。
适用：Issue #12 评论 6893024353（默认均衡策略）。

本文只写已经做到的事和它的依据。凡是没有对照数据支撑的收益，一律标「待 A 线验证」。

---

## 1. 目标范围：只对用户明确纳入的应用

一句话：**默认按钮的作用范围是「用户显式纳入的 app scope」，既不是全机，也不是整个 user session。**

### 为什么不是全机

旧默认路径（`activate(19.0)` → 菜单确认 → `activate(710.0)` → `mr_start_reclaim(0)`）
最终执行：

```
posix_spawn /usr/bin/pkexec /usr/libexec/memreduct-zamak-helper pagecache
  → 写 /proc/sys/vm/drop_caches   (native/memreduct_helper.c:32)
```

`drop_caches` 是**机器级**的，清掉所有用户的页缓存。用户以为在清自己那点缓存，实际
清了整台机器。这就是 Issue 说的「静默扩大范围」。

### 为什么不是整个 user session

`mr_bal_bridge_session_run()` 之前的语义是「把 `user@UID.service` 里所有 file 页都收一遍」，
而且它**故意忽略调用方传入的预算**（`native/balance_bridge.c` 内注释明说这是刻意的），
强制用 session 的全部 file 字节。这一样是把「本应用」放大成「整个会话」。

实测数据说明了为什么这个区分是必要的（协调者在本机实测，kB）：

```
session.slice:       anon  86,532   file  24,908   active_file  7,070   inactive_file 16,826
Chrome-729959:       anon   4,121   file  73,953   inactive_file 71,176
dsh-desktop-614476:  anon 447,574   file  61,108   inactive_file 53,035
```

**可回收的文件页集中在少数几个进程，不可回收的匿名页集中在另外的进程。**
按 session 收会把两者混在一起。

### 现在怎么做

`native/balance_policy.c` 的 `mr_pol_begin()` 只遍历 `enrolled != 0` 的 scope。
`native/balance_bridge.c` 的 `mr_bal_bridge_policy_enroll()` 是唯一的纳入入口，
默认**一个都不纳入**（`policy_enrolled_count == 0` 时 `mr_pol_begin` 直接返回
`MR_POL_STOP_NO_TARGET`）。

`src/main.zm` 的 index 9 分支先 `mr_bal_bridge_policy_enroll_clear()`，
再逐个 `mr_bal_bridge_policy_enroll(i)`，纳入的判定条件是
`mr_bal_bridge_scope_eligible() == 1`（该 scope 确有可回收文件页）。

### 移除的静默后备路径（逐条）

| # | 位置 | 原来的静默扩大范围 |
|---|---|---|
| 1 | `native/memreduct_helper.c:32` → 经 `src/main.zm:activate(710.0)` → `mr_start_reclaim(0)` → `pkexec .../memreduct-zamak-helper pagecache` | 默认确认路径直接落到固定写 `/proc/sys/vm/drop_caches`，清全机所有用户页缓存。现该 helper 只从菜单第 1 行显式选择进入，该行文案已改为「全机、所有用户」。 |
| 2 | `native/balance_bridge.c` `mr_bal_bridge_session_run()` | 故意忽略调用方预算，强制用 `user@UID.service` 全部 file 字节，把「本应用」放大成「整个会话」。**现已无任何 `src/main.zm` 调用点**（只保留 extern 声明与历史注释）。 |
| 3 | `src/main.zm` `activate(710.0)` 之外的 balanced 行（index 9） | 原走 `session_run`，同一会话级放大。现走策略，仅已纳入 scope。 |
| 4 | `src/main.zm` `activate(103.0)`（CI 镜像路径） | 原走 `session_run(0)`，一个测试钩子能清整会话 —— 比它声称镜像的那一行范围更大。现与 index 9 完全同路。 |
| 5 | `src/main.zm` `mr_runtime_start_balanced_run()` 启动钩子 | 原走 `session_run(bal_request_bytes)`，一个 harness 开关能清整会话。现与 index 9 完全同路。 |

第 2 项仍保留函数本体（其他非默认路径可能引用），但默认动作与两个 CI 入口都已不再调用它。
`grep -n "mr_bal_bridge_session_run" src/main.zm` 现在只剩 extern 声明与一处历史注释。

---

## 2. 触发条件：三条全部满足才动手

`mr_pol_begin()` 里 `preconditions_met()`：

| 条件 | 默认阈值 | 字段 |
|---|---|---|
| 持续压力 | `psi_some_centi >= 500`（5.00%）**且** `pressure_seen_ms >= 2000` | `psi_trigger_centi` / `pressure_dwell_ms` |
| 可用余量不足 | `mem_available < 50% × MemTotal` | `low_water_centi = 5000` |
| 范围内确有合适文件页 | 见第 3 节 | — |

单条都不够，因为：

- 只有压力没有紧张 → 是别人在忙，不是我们要解决的；
- 只有紧张没有压力 → 缓存正在干它该干的事；
- 两者都有但没有合适冷页 → 没有安全的可拿之物。

`pressure_seen_ms` 的存在是为了让**单次尖峰不算持续压力**。
（注意：bridge 当前把 `pressure_seen_ms` 填成 config 的 `pressure_dwell_ms`，
即「没有历史即视为已满足」。这是一个**已记录的简化**，真正的连续性追踪要靠
累积采样，留给下一轮。测试 `test_a_pressure_spike_is_not_sustained_pressure`
覆盖的是策略层对不满足该条件时的拒绝行为。）

---

## 3. 关键区分：文件计费量不是冷页量

这是项目硬约束，也是本策略最容易做错的地方。

一个 scope 之所以成为候选，看的是：

```c
/* 冷源必须相对活跃侧有份量，否则这是一个正在使用的 scope */
if (scope->active_file > 0 && cold < scope->active_file / 8ull) return 0;

/* refault 历史标记「冷页其实是想要的」 */
allowed = percent_of(scope->file_bytes, config->refault_budget_centi);
if (scope->refault_file > allowed) return 0;
```

- 冷页量取 `memory.stat` 的 **`inactive_file`**，不是 `file`。
- `active_file` 远大于 `inactive_file` 的 scope 被判为「正在使用」，跳过
  （`mr_pol_scope_skip_reason()` 返回 `active_working_set`）。
- `workingset_refault_file` 超过 `file` 的 10% 判为「热」，跳过（返回 `refaulting`）。

测试 `test_file_billing_without_cold_pages_is_not_a_candidate` 就构造了一个
「计费 300 MiB、但 inactive 只有 1 MiB」的 scope，断言它必须被拒绝。

---

## 4. 分批的 file-only 定向回收

一批 = 一次写。`mr_bal_bridge_policy_plan()` 给出第一批，`policy_run_batch()` 发一批，
`policy_continue()` 重新决策要不要下一批。

请求量三重封顶（`request_for()`）：

```
min(inactive_file × 25%,  batch_bytes = 32 MiB,  max_request_bytes − 本轮已请求)
```

`max_request_bytes` 默认 256 MiB，`max_batches` 默认 8。

**file-only 是写进写入内容里的，不是靠注释保证的**：
`balance_exec.c` / `balance_reclaim.c` 的写入形式固定带 `swappiness=0`，
测试 `test_the_single_batch_is_file_only_in_form` 直接对注入的写钩子断言这一点。

---

## 5. 停止条件：五类，任一命中就不发下一批

| 停止原因 | 枚举 | 判据（`mr_pol_continue`） |
|---|---|---|
| 达到目标水位 | `MR_POL_STOP_TARGET_MET` | `avail_after >= 65% × MemTotal` |
| 刚清的页被快速读回 | `MR_POL_STOP_REFAULT` | `refaulted > 0` |
| 压力恶化 | `MR_POL_STOP_PRESSURE_WORSE` | `psi_after >= 20%` **或** `psi_after > psi_at_start` |
| 关键遥测缺失 | `MR_POL_STOP_TELEMETRY_MISSING` | `evidence == NULL` 或 `mem_total == 0` |
| 用户取消 | `MR_POL_STOP_CANCELLED` | 调用方 `policy_finish()` / 不继续 |

「压力恶化」这一条我加了**任何上升都停**（不只是超过 20% 上限），
因为是我们自己在回收，看到自己造成的恶化就该收手，不该等它撞上限。

---

## 6. 冷却

两层：

1. **scope 级冷却**：某个 scope 刚 refault，`mr_pol_cool_scope()` 给它
   `now + cooldown_ms`（默认 60 s）的冷却期，期内 `scope_is_candidate()` 拒绝它。
   目的是**不重复清一个正在重建的健康缓存**。
2. **轮次级间隔**：`since_last_run_ms < cooldown_ms` 且已有上一次运行时间 →
   `MR_POL_STOP_COOLDOWN`。防止调用方自己循环。

测试 `test_a_cooling_scope_is_left_alone` / `test_cooldown_expires` /
`test_round_cooldown_prevents_immediate_rerun` 覆盖。

---

## 7. 健康缓存回填不重复清

这是冷却机制的直接后果，单独说明因为它是一个独立要求：

- refault 一旦观测到，本轮立即停（`MR_POL_STOP_REFAULT`）；
- 该 scope 进入冷却，下一轮**不会被选中**；
- 且有 refault 历史的 scope 在资格判定阶段就被 `refault_budget_centi` 挡掉。

三重，不是单点。

---

## 8. 匿名增长：指出贡献 scope，不升级档位

当所有纳入 scope 都没有合适冷页、而匿名内存占主导时：

```c
if (anon_total > cold_total && biggest_anon >= 0) {
    out->verdict = MR_POL_VERDICT_ANON_BOUND;
    out->stop = MR_POL_STOP_NO_TARGET;
    out->scope_index = biggest_anon;      /* 点名最大贡献者 */
    out->anon_bytes = biggest_anon_bytes;
}
```

界面上由 `mr_bal_bridge_policy_anon_scope_label()` 取到该 scope 的显示名，
`bal_policy_result_text()` 组成一句话：增长来自匿名内存，主要贡献者是 X，
本策略不会把匿名页压出去。

**这一条路径上没有任何 PAGEOUT 调用**，也不存在「再试一次更重的档位」。
`MR_BAL_ACTION_ANON_PAGEOUT` 的策略层枚举虽然存在（继承自 balance_plan.h），
但 `balance_policy.c` 从不产生它。

测试 `test_when_only_anon_grows_the_verdict_is_anon_bound` 与
`test_anon_bound_round_names_a_scope_and_writes_nothing` 断言：
返回 0、verdict 为 anon_bound、**写入次数为 0**。

---

## 9. 深度清理保持高级手动

`balance_policy.c` 不包含任何深度清理路径：没有 S1（全机 pagecache）、
没有 S4（压力回收），也没有任何按时间/按条件自动触发它们的代码。
深度清理仍只在 `src/main.zm` 的 `activate(104.0)` 里，由用户两次按下才执行。

自动模式本轮**不启用**：`mr_pol_begin()` 没有一个由计时器或后台线程调用的入口。

---

## 10. 阈值与来源

全部是**保守默认值**，不是实测最优。校准接口就是 `struct mr_pol_config` 本身
（`mr_pol_config_default()` 填默认，调用方可整字段覆盖）。

| 字段 | 默认值 | 来源 / 依据 | 状态 |
|---|---|---|---|
| `low_water_centi` | 5000（50%） | 沿用 `balance_plan.c` 既有值 | 待 A 线校准 |
| `high_water_centi` | 6500（65%） | 同上，留 15 点回滞 | 待 A 线校准 |
| `psi_trigger_centi` | 500（5.00%） | 同上 | 待 A 线校准 |
| `psi_worse_centi` | 2000（20.00%） | 同上 | 待 A 线校准 |
| `pressure_dwell_ms` | 2000 | 本线新设：让单次尖峰不算数 | 待 A 线校准 |
| `cooldown_ms` | 60000 | 本线新设：比 plan 的 30 s 更保守 | 待 A 线校准 |
| `batch_bytes` | 32 MiB | 沿用 `balance_reclaim.c` 的批次大小 | 待 A 线校准 |
| `max_batches` | 8 | 本线新设（plan 是 32，太激进） | 待 A 线校准 |
| `max_request_bytes` | 256 MiB | 本线收窄（plan 是 512 MiB） | 待 A 线校准 |
| `cold_fraction_centi` | 2500（25%） | 沿用 plan 的 `file_fraction_centi` | 待 A 线校准 |
| `refault_budget_centi` | 1000（10%） | 本线新设 | 待 A 线校准 |

**关于比例的依据（可核）**：
本机 session 级 `inactive_file` 约 16.8 MB，而 `active_file` 7.1 MB。
`cold_fraction_centi = 2500` 意味着单批最多问 4.2 MB —— 对一个总量 16 MB 的池子，
这是「拿走四分之一」，不是「清空」。目标水位也刻意不设成「清空」，
而是设成「可用回到 65% 就停」。

**给 A 线的接口**：A 线回放数据出来后，只需改 `mr_pol_config_default()` 的对应字段，
`balance_policy.c` 的判定逻辑不需要动。测试里所有 fixture 都基于 config 默认值推导，
改默认值会立刻反映到测试期望上（`test_config_defaults_are_conservative`
断言的是字段间关系而不是绝对值，因此不会因为校准而变红）。

---

## 11. 分支覆盖对照

### `tests/test_policy_state.py`（32 个，纯策略层）

| 分支 | 测试 |
|---|---|
| 三条件全满足 → 发批 | `test_a_qualifying_scope_is_acted_on_file_only` |
| 请求量按冷页比例封顶 | `test_request_is_capped_by_the_share_of_cold_pages` |
| 选最大冷源 | `test_the_largest_cold_source_is_chosen` |
| 小 scope 不被取整成 0 | `test_a_single_cold_scope_still_gets_one_honest_ask` |
| 余量充足 → 不动 | `test_plenty_available_means_no_action` |
| 有余量但有压力 → 不动 | `test_pressure_without_scarcity_does_not_arm` |
| 缺余量但无压力 → 不动 | `test_scarcity_without_pressure_does_not_arm` |
| 压力尖峰不算持续 | `test_a_pressure_spike_is_not_sustained_pressure` |
| **file 大小不足以入选** | `test_file_billing_without_cold_pages_is_not_a_candidate` |
| 未纳入的 scope 绝不动 | `test_unenrolled_scopes_are_never_touched` |
| 不可读 scope 跳过 | `test_an_unreadable_scope_is_skipped_not_assumed_fine` |
| 零 scope 不崩 | `test_zero_scopes_is_a_no_target_not_a_crash` |
| anon 主导 → 点名不升级 | `test_when_only_anon_grows_the_verdict_is_anon_bound` |
| 冷却中的 scope 不动 | `test_a_cooling_scope_is_left_alone` |
| 冷却到期可再用 | `test_cooldown_expires` |
| refault 历史挡掉 scope | `test_a_refaulting_scope_is_skipped_by_history` |
| 达目标水位停 | `test_target_reached_stops_the_run` |
| refault 停 | `test_refault_stops_the_run` |
| 压力超上限停 | `test_worsening_pressure_stops_the_run` |
| 压力微升也停 | `test_any_psi_rise_stops_even_below_the_ceiling` |
| 批次数上限停 | `test_batch_ceiling_stops_the_run` |
| 请求量上限停 | `test_request_ceiling_stops_the_run` |
| 遥测缺失拒动 | `test_missing_evidence_refuses_to_act` |
| mem_total=0 视为缺失 | `test_zero_mem_total_is_treated_as_missing_telemetry` |
| 取消有名字 | `test_user_cancel_is_a_named_stop` |
| 轮次间隔冷却 | `test_round_cooldown_prevents_immediate_rerun` |
| 停止/verdict 名称唯一 | `test_every_stop_has_a_stable_name` / `test_every_verdict_has_a_stable_name` |
| 跳过原因可解释 | `test_skip_reasons_explain_why_a_row_is_greyed` |
| 默认值保守且自洽 | `test_config_defaults_are_conservative` |

### `tests/test_policy_bridge.py`（13 个，接线层，真 fixture 目录 + 写钩子）

| 分支 | 测试 |
|---|---|
| 默认不纳入任何应用 | `test_nothing_is_enrolled_by_default` |
| 存在但未纳入 → 零写入 | `test_unenrolled_scope_is_never_submitted` |
| 默认路径不走全机 helper | `test_default_round_never_touches_the_global_helper` |
| 写入形式带 swappiness=0 | `test_the_single_batch_is_file_only_in_form` |
| 只有 active 页 → 零写入 | `test_no_cold_pages_means_no_write` |
| 冷却 scope 在接线层生效 | `test_a_cooling_scope_is_skipped_by_the_wiring` |
| 每个纳入 scope 都可解释 | `test_skip_reason_is_reportable_for_every_enrolled_scope` |
| 非法索引拒绝纳入 | `test_enroll_rejects_a_scope_that_cannot_be_read` |
| 重复纳入幂等 | `test_enroll_is_idempotent` |
| 拒绝原因不被吞掉 | `test_stop_names_are_reported_not_swallowed` |
| anon 轮次零写入 | `test_anon_bound_round_names_a_scope_and_writes_nothing` |
| 批次循环必然终止 | `test_batches_never_exceed_the_round_ceiling` |
| finish 后不再发批 | `test_finish_ends_the_round` |

---

## 12. 本轮没有做的事（明确记录）

1. **没有真实压力回放**。所有测试都是注入 fixture，没有用 A 线的实测回放驱动。
   分支覆盖是完整的，但「在真实压力曲线下策略是否做对」尚未验证 —— 待 A 线。
2. **`pressure_seen_ms` 没有真正的连续性追踪**。bridge 现在把它填成 config 默认值。
   正确的实现需要累积采样历史，本轮没做。
3. **per-scope refault 计数依赖 cgroup 的 `workingset_refault_file`**。
   内核版本不支持该字段时它保持 0（策略视为「无历史」而非「干净」）。
   本机内核是否有该字段，未经真机验证。
4. **没有对照数据**。本策略是否比旧默认路径产生更少的卡顿、更少的 refault，
   **没有任何实测对照**。任何「持续收益」的说法在此都不成立 —— 待 A 线验证。
5. **不碰 Swap / ZRAM / Polkit**。全机 pagecache helper 保留在菜单第 1 行，
   文案已改为明示「全机、所有用户」。
