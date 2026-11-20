# Q16 B 线：scope 增长归因

脚本 `scripts/ci/q16-scope-attribution.py`，测试 `tests/test_q16_scope_attribution.py`（9 项）。

## 为什么必须分两个排名

「谁占内存最多」和「谁在长」是两个问题：

- **存量**大但静止的进程，处理方式是「要不要关掉它」
- **增量**大的进程，处理方式是「它为什么现在在长」

合并成一个「内存大户」排名会把这两件事搅在一起，得出的建议两头不靠。

## 实测（2026-11-15 22:11–22:12，60 秒窗口）

### 存量 Top 8（MB，父聚合 slice 已排除）

| scope | anon | file | inactive_file | refault |
|---|---|---|---|---|
| dsh-subprocess-673676-48ebd6702fa0 | 2619.3 | 87.2 | 70.9 | 6.0 |
| Hyprland-gtk-launch-6f6f3595 | 1361.5 | 192.1 | 158.1 | 6.7 |
| deepseek-ai-dsh-desktop | 481.2 | 103.7 | 97.9 | 2.9 |
| omniroute | 334.2 | 68.4 | 58.6 | 8.7 |
| uuremote-native | 139.6 | 14.1 | 10.3 | 0.2 |
| Hyprland-gtk-launch-e49316b1 | 94.2 | 6.8 | 6.8 | 2.8 |
| Hyprland-gtk-launch-a1e3e7b0 | 64.8 | 20.3 | 3.8 | 0.2 |
| Hyprland-gtk-launch-8621e5eb | 63.1 | 54.8 | 39.7 | 4.9 |

### 窗口增长 Top 8（末列为增量 MB）

| scope | anon | 增量 |
|---|---|---|
| dsh-subprocess-673676-48ebd6702fa0 | 2619.3 | **+93.8** |
| Hyprland-gtk-launch-6f6f3595 | 1361.5 | +47.9 |
| Hyprland-gtk-launch-a1e3e7b0 | 64.8 | +29.3 |
| uuremote-native | 139.6 | +14.0 |
| org.chromium.Chromium | 45.7 | +6.3 |
| com.google.Chrome | 6.0 | +2.9 |
| Hyprland-gtk-launch-3a0bbf84 | 24.9 | +2.7 |
| folia-major | 9.1 | +2.5 |

**两个排名重合 4/8，但不完全一致**——`omniroute`（334 MB）和 `deepseek-ai-dsh-desktop`（481 MB）
存量很大但窗口内几乎没长，所以只出现在存量榜；`Hyprland-gtk-launch-a1e3e7b0`
存量只排第 7 却增长了 29.3 MB，在增长榜上升两位。

### 关键读数：全机 anon 不等于这些 scope 之和

```
全机 AnonPages       6714.4 MB
这些 scope 之和      2170.5 MB
```

**约 2/3 的匿名内存在任何 app scope 之外。** 可能来源：内核记账、page tables、
非 app.slice 的 cgroup（如 system slice、user@.service 本体）、以及 shmem 等。

这条数字是对上一轮错误的直接修正：我当时看到全机 anon 涨 1 GiB 就说「Chromium 吃了 6.6 GiB」，
那是把全机总量算给单一进程。**现在工具本身就把总量和 scope 和分开打印，且不做任何分摊。**

## 三个不许犯的错（已写进脚本注释与测试）

1. 不把全机 `AnonPages` 归因给任何 scope。测试 `test_script_never_apportions_machine_anon`
   断言代码里不存在任何相除/分摊逻辑。
2. 不把 cgroup `anon` 说成「进程 RSS」。cgroup 记账可能包含已退出进程的页，
   且与进程 RSS 定义不同。
3. 不把一次窗口的增量说成「泄漏」。一个窗口是趋势，不是诊断。

## 一个实测到的坑：memory.stat 非原子读

单次读取会撕裂。实测样本：

```
anon        41,271,296
active_anon 42,803,200
inactive_anon      0
active+inactive 42,807,296   ← 比 anon 还大 1.5 MB
```

所以 `anon == active_anon + inactive_anon` **在单点不成立**。脚本不假设它成立，
增量用两次完整采样相减，并在文档与测试里都写明这一点
（`test_torn_sample_is_documented`）。

## 辅助提示（清理结果区）

挂载点：`src/main.zm:487` `bal_policy_result_text()` 已有 `anon_bound` 分支：

> 均衡清理未执行：增长来自匿名内存，主要贡献者是 {who}。本策略不会把匿名页压出去；
> 只会挪动曲线，不会解决问题。

这条文案的方向是对的，本轮**不新增常驻监控、不做后台轮询**，符合
「第二条只做清理结果里的辅助提示」的要求。`who` 由
`native/balance_bridge.c` 的 `mr_bal_bridge_policy_anon_scope_label()` 提供，
它来自 `balance_policy.c` 的 `MR_POL_VERDICT_ANON_BOUND` 判定（anon_total > cold_total
时命名 anon 最大 scope、不升级动作）。

## 限制

- 60 秒窗口只能看趋势，不能判定泄漏
- 同名 scope（两个 Chromium 实例）合并取较大者，会低估实例数
- 脚本只读 `/sys/fs/cgroup`，不写任何 cgroup
