# Q14 集成结论（协调者）

分支 `q14/integration`，HEAD `15732de`
基线 `b89a2fb`

## 0. 先说被推翻的前提

本轮开始时，我向 A、B 两线下发了一条"已核实"的链路：

> 主按钮 → 确认 → `mr_start_reclaim(0)` → `pkexec …-helper pagecache`
> → 写 `/proc/sys/vm/drop_caches` → **全机所有用户**

A 线用证据推翻了这个说法对**已安装二进制**的成立性。我复核确认：

```
objdump -d /usr/bin/memreduct-zamak --disassemble=mr_production_clean_enabled
   a8de0: 31 c0   xor %eax,%eax
   a8de2: c3      ret            → 恒返回 0

objdump -d /usr/bin/memreduct-zamak --disassemble=mr_xproc_capability_enabled
   a8df0: 31 c0   xor %eax,%eax
   a8df2: c3      ret            → 恒返回 0
```

两个 gate 在已装二进制里都是**默认门**（`production_*_gate_default.c`）。
`build-production-gui.sh` 不替换它们（`build-production-clean-gui.sh` 才换）。

**所以已安装的 `235e0471` 不存在可达的全机 drop_caches 路径。**
菜单第 2 行 "Clean page cache" 恒为禁用；主按钮只打开菜单；
菜单里唯一可用的是第 1 行 "Trim this app's memory"，走
`mr_self_reclaim()` → `process_madvise(MADV_PAGEOUT)`，**只作用于本进程**。

源码层面那条链路是真的，但它通向一扇锁着的门，而我在下发前只读了源码、
没有去反汇编已安装的二进制。这是本轮的实质性错误，不是表述问题。

## 1. 按钮 → 后端 → 范围（已装二进制 `235e0471…` 的真实答案）

| 层级 | 实际行为 |
|---|---|
| 主按钮 `Clean memory` | `activate(19.0)` → 只打开清理菜单，**不执行任何清理** |
| 菜单第 1 行 释放本程序内存 | `mr_self_reclaim()`，`MADV_PAGEOUT`，**仅本进程** |
| 菜单第 2 行 清理页缓存 | gate 返回 0 → **恒禁用，不可点** |
| 菜单第 9 行 均衡清理 | `bal_entry_state()` 决定；本机未纳入应用时禁用 |
| 菜单第 10 行 深度清理 | 需两次确认；S1 因同一 gate 也不会执行 |

## 2. 16:19 那次 0.9 GB 回升：不可归因

| 证据 | 数值 | 含义 |
|---|---|---|
| A 线对照臂（320 s 无干预） | MemAvailable 极差 **798 MiB** | 1.2→2.1 GB 完全落在噪声内 |
| 我的无干预 120 s 观察 | 极差 **473 MiB**，单次 10 s 跳变 **414 MiB** | 同上 |
| A 线对照臂 anon 净增 | +131 MiB 而 avail 基本不动 | **匿名增长被排除** |
| 截图 | `deployed.png` / `after_click.png` 均无菜单弹出 | 无法证明点击到达按钮 |
| 即使点到 | 第 2 行不可点 | 清不了 |

**结论：回升来源无法归因；文件缓存回填证据不足（缺清理臂配对）。**

本底（A 线 320 s，什么都不做）：
refault_file +5.2 GiB、pswpout +3.7 GiB、zram mem_used +104 MiB、
PSI full total +4.15 s、disk read +8.4 GiB / write +1.85 GiB。

## 3. 是否有持续收益

**持续收益未证实。**

- 清理降低 PSI / 改善前台响应：**无数据**（缺清理臂）。
  A 线的延迟探针 p50 抖动仅 ±2%，分辨不出任何清理影响，该路径当前不可用。
- 清理增加 refault：**无数据**；但本底 refault 5.2 GiB/320 s 极大，
  任何"清理导致 refault 上升"的断言必须先扣掉这个本底。

## 4. B 线的实际改动

默认路径不再有静默扩大范围的后备。移除的五条：

| # | 位置 | 原范围 |
|---|---|---|
| 1 | `memreduct_helper.c:32` 经默认确认 → pkexec | 全机 drop_caches（现只能菜单显式选，且已装版不可点） |
| 2 | `mr_bal_bridge_session_run()` | 强制整个 `user@UID.service`；现无 main.zm 调用点 |
| 3 | `src/main.zm` 菜单 index 9 | 整会话 → 现走策略 |
| 4 | `activate(103.0)` CI 镜像路径 | 原 `session_run(0)` 清整会话 |
| 5 | `mr_runtime_start_balanced_run()` 启动钩子 | 原 `session_run(...)` 清整会话 |

新策略：只对**显式纳入的 app scope**、只动 `inactive_file`、
三条件同时满足才动手、五个停止条件、refault 后冷却、
anon 增长只点名不升级。深度清理保持高级手动。

**全部 11 个阈值是保守默认值，不是实测最优**，校准接口 = `struct mr_pol_config`。

## 5. 集成时发现并修掉的一个洞

`native/balance_policy.c` **不在 `zam.toml` 的 native 源列表里**。
它在 `zam check` 下干净、在单测里全绿，因为这两者都不链接二进制。
生产构建失败，九个 `undefined reference to mr_pol_*`。

文件存在、编译通过、测试通过、**永远不会随产品发出**。
已加入 `zam.toml`，构建通过。

## 6. 验证

```
python3 -m unittest discover -s tests  ->  401 tests, OK
                                            (skipped=4, expected failures=1)
zam check .                            ->  CHECK OK memreduct_zamak
scripts/ci/build-production-gui.sh     ->  成功
  artifacts/p2b/memreduct-zamak-production
  sha256 aca029af21f17ac5725f72b795f8250e8867476f26242130b0d8228a2583ebc9
```

## 7. 未做 / 未动

- 已安装 `/usr/bin/memreduct-zamak` **未改动**（仍 `235e0471…`）
- 未推送、未部署、未合并、未发版
- 未碰 Swap / ZRAM / Polkit
- 未制造负载（未跑 hog、未压测）
- 旧 worktree 未动

## 8. 不确定项

1. `235e0471` 对应哪次构建 —— 我只能证明行为一致（gate 归零），
   不能证明逐位来源；G-03 老缺陷仍在。
2. 本机只有一枚 `memreduct-zamak`（`/usr/bin`），另有 `.before-p0a` 备份；
   `find /` 全盘扫描超时未完成，未穷尽。
3. `memguard.service` 每 2 秒调 `memory.high`，会改变 reclaim 行为；
   对照/清理两臂必须同开或同关，本轮未控制这个变量。
4. A 线指出我先前"avg300 高于 avg10 说明压力在缓解"的说法有反例
   （+300s 行 full avg10=3.31 > avg300=1.06）。该方向不每时每刻成立。
