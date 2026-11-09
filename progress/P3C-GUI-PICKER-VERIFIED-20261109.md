## P3C 跨进程回收 GUI 侧：真实候选枚举已验证

2026-11-09，私有 Xvfb + XTEST，驱动 **m0+m1-candidate** 构建
（`artifacts/p3c-crossproc/memreduct-zamak-production-m0-m1-candidate`，
sha256 `83392128fc957ce949ce517c94007fe3fcba7bdac0ddebbd2a0095eee4d72b75`）。

### 1. 已安装版根本开不了这个功能（门控被编译关闭）

反汇编安装版 `/usr/bin/memreduct-zamak`：

```
mr_xproc_capability_enabled:  xor %eax,%eax ; ret     → 0
```

即装了生产清理版（页缓存 gate=1），但**跨进程 gate 仍是 0**。
两个 gate 刻意独立（注释写明：启用页缓存不得顺带开启跨进程回收）。
故 `scripts/ci/build-production-crossproc-gui.sh m0+m1-candidate` 重建：

```
mr_production_clean_enabled: mov $1,%eax ; ret   → 1
mr_xproc_capability_enabled: mov $1,%eax ; ret   → 1
strings | grep -c 'MEMREDUCT_CI|MEMREDUCT_TEST'  → 0
```

### 2. GUI 真实枚举本机进程（不是模拟）

点击 Clean → 选 "Select user app to reclaim (experimental)"，面板显示真实数据：

```
omarchy-hyprlan  PID 1474  UID 1000  app bash    sleeping  27 private regions · 0 bytes resident estimate
udiskie          PID 1475  UID 1000  app python314 sleeping 493 private regions · 1.7 MiB resident estimate
socat            PID 2517  UID 1000  app socat1   sleeping  60 private regions · 0 bytes resident estimate
karing           PID 4190  UID 1000  app karing   sleeping 715 private regions · 9.0 MiB resident estimate
```

滚动后继续显示 `qq`/`chrome` 等真实条目并可见到底（`chrome PID 3733352`），
滚动窗口实现为 `xproc_scroll`（可见 4 行，`index >= scroll+4` 时滚动）。

面板文案与 Issue #13 的术语纪律一致：
- "Preview only, advice is not bytes freed."
- "PAGEOUT may use swap I/O and stall the target."
- "Only user app scopes are listed, identity is checked again…"

### 3. 与独立只读扫描的一致性交叉验证

自建扫描工具（同一 `mr_cand_scan`）：

```
scanned=578 offered=80 skipped_self=1 other_uid=354 protected=139 limit=0
self-made target: pid=1338319 name=xproc-target
   rss=298033152 private_resident=295981056 reclaimable=338268160 regions=17   → 被收录
```

目标是自建的 320 MiB 匿名进程（`app-xtarget2.scope`），被正确收录为合格候选，
证明 ≥256 MiB 的大额度目标确实进入可选集合。

### 4. 仍未做的一步（需特权批准）

真实 `process_madvise(MADV_PAGEOUT)`。同 UID 直呼复验返回 `EPERM`，
必须经 pkexec 调用 `/usr/libexec/memreduct-zamak-crossproc-helper reclaim pageout …`。

### 5. 一个顺带观测

把目标进程静置后，其 VmRSS 从 329796 kB 自行降到 2096 kB ——
内核在没有任何 `MADV_PAGEOUT` 的情况下就把匿名页换出了。
这说明「内存压力下内核自己也会回收」，任何把 MemAvailable 差值全记在软件头上的说法都不成立。
