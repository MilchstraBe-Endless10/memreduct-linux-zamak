# ADR-0009: 高压内存回收（Q17 提案）

日期：2026-11-20　状态：PROPOSED（待真机数据补齐后定稿）

## 背景与用户目标

用户的目标不是"文件页短暂下降"，而是"点一下，可用内存明显回升（GiB 级）"。
2026-11-20 深夜真机实测（15.4G RAM，MemAvailable 一度低至 ~700 MiB）：

### 实测一：S4 等效分批回收（对 user@1000.service 写 memory.reclaim）

- round 0: 512 MiB 请求 accepted
- round 1: swap-rate guard 正确触发（后台 pswpout ~11217 pages/s，远超 20000 阈值前的波动）
- net: -248 MiB → 第二轮 +240 MiB → 20 秒后跌回 -385 MiB

**结论**：在持续高压机器上，memory.reclaim 回收出的空间立即被活跃分配吃掉，
单次净收益不稳定甚至为负。这不是 bug，是语义：reclaim 释放的是"可回收页"，
而高压机器上冷页早就在被内核持续回收了。

### 实测二：memory.high 限流（高压回收正路）

- `user@1000.service` 层写 memory.high 需要 root（该层非 delegated）——用户态不可达
- DSH 自身 scope（dsh-subprocess-*.scope）是 delegated 的，可写，但只有 18 MiB，
  限自己无意义

**结论**：真正的大额回收需要把「整个 user session」限流到目标值再放开，
逼内核把 session 的冷页挤进 zram（zram 15.4G 容量仅用 5.1G，空间充足），
物理 RAM 释放给前台。这正是历史实测中"单轮 1.3 GiB / 四段 2.88 GiB"的机制。

## 方向（Q17 要做的）

1. **S2/S3 的 session sweep 改为 memory.high 限流脉冲**：对
   `mr_bal_exec_session_root()` 指向的 cgroup 写 `memory.high = 目标值`（如
   current 的 66%），持 8–15 秒，期间周期检查 MemAvailable，然后写回 `max`。
   限流是内核行为：不杀进程、不丢数据、把冷 anon 页压进 zram、热页留下。
2. **特权层**：user@1000.service 层写 memory.high 需要 root——这必须走
   既有 pkexec/helper 通道（与 S1 同一条），而不是绕开它。helper 增加
   "session throttle pulse" 动作，polkit 仍按既有授权。
3. **守卫不变**：PSI 增长、swap-rate、用户取消、每批前检查——全部保留。
   memory.high 脉冲对桌面交互的影响由 PSI 守卫管。
4. **恢复保证**：任何路径退出（含 crash——用一个 watchdog 计时器兜底）都必须
   把 memory.high 写回 max。这一点是安全底线，比收益重要。

## 与 Q16 的关系

Q16（按应用文件页回收）保持原样交付，它是"轻量、精细、无感知"的档位。
Q17 是"重档"：整会话、有短暂延迟代价、但换来 GiB 级回升——是用户真正要的。
两个档位在 UI 上分开命名，不混用文案。

## 2026-11-20 深夜追加实测（高压满载状态，按用户要求当场做）

对每个 scope 依次执行 memory.high 脉冲（限到 current 的 60–75%，持 6–8 秒，
写回 max），全部无 root、无进程被杀、结束后逐一核验 memory.high=max：

| 脉冲目标 | scope 占用变化 | MemAvailable 瞬时净增 |
|---|---|---|
| dsh-subprocess-1034070 (DSH 运行时) | 2575 → 2006 MiB | +585 MiB |
| Chromium ×2（每 scope 6s 脉冲） | — | +491 MiB |
| app.slice 全 scope memory.reclaim 40% | — | +81 MiB |
| **合计** | | **≈ +1.06 GiB 瞬时** |

PSI some avg10 从 2.94 → 3.09（脉冲期间），桌面无可感知卡顿。

**关键负面结果（必须诚实记录）**：脉冲后 1 分钟 MemAvailable 回落到
~40 MiB——低于实验前。原因是机器整体处于极端持续分配状态（zram 13.9G
满载数据、pswpout 数千页/秒、另有 VM/浏览器/多 agent 并发）。脉冲确实把
冷页挤进了 zram、把物理 RAM 还给了前台，但**立刻被持续的分配吃掉**。

## 结论与设计含义

1. memory.high 脉冲**是有效的回收机制**（瞬时 +1 GiB、无特权、无杀进程、
   完全可逆），远优于单写 memory.reclaim（净收益为负）。
2. 但它**不是魔法**：内存紧张的本质是"活跃工作集 > 物理 RAM"。软件能做的是
   把回收时机从"内核被动挤"变成"用户主动按"，把前台需要的页留住、把冷页
   压进 zram——收到的是几秒到几分钟的呼吸空间。
3. **Q17 产品的真实价值主张**（写进 UI 文案）："为当前前台应用争取一段
   突出的内存余量"，并如实展示"X 分钟后回落"的预期。不承诺"保持低位"。
4. 实现：对每个大占用 scope（≥64 MiB）做 60% 限流脉冲 8 秒 + PSI 守卫 +
   全量恢复检查；crash 兜底 watchdog。这是 Q17 的实现蓝图。

## 下一步

- [ ] 在已装版 + root 授权下实测 memory.high 脉冲的真实净收益（3 次采样）
- [ ] helper 增加 throttle 动作 + polkit
- [ ] S2/S3 改造 + watchdog
- [ ] 真机验收：MemAvailable 净增 ≥ 1 GiB，PSI avg10 峰值 < 20
