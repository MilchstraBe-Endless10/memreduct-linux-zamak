## P3B：自进程 PAGEOUT（`mr_self_reclaim_run`）实测生效

2026-11-09。这条路径此前一直只被间接覆盖（GUI 里"清理本进程"），本轮单独实测。

关键前提：**只作用于自己进程**，所以不需要 `CAP_SYS_NICE`、不需要 Polkit、不需要密码。
源码注释也明确写着 "Cannot touch any other process; that needs CAP_SYS_NICE and a privileged helper."

### 实测

探针先 `malloc` + `memset` 一块 192 MiB 私有匿名区，再调 `mr_self_reclaim_run()`：

```
== before ==
  VmRSS:   198344 kB
  RssAnon: 196724 kB
  VmSwap:        0 kB

== result ==
  status=0  bytes_paged_out=203960320  failed_batches=0  errno=0

== after ==
  VmRSS:     2024 kB
  RssAnon:      84 kB
  VmSwap:  196640 kB
  sentinel=5a   (buf still mapped, 数据未丢失)
```

### 怎么读这组数

- `bytes_paged_out = 203960320` ≈ **194.5 MiB**，是内核**接受的建议量**，
  与 `VmSwap` 增量（196640 kB ≈ 192 MiB）同量级，说明这些页确实落到了 swap。
- `VmRSS` 减少 196320 kB ≈ **191.7 MiB**，与写入量一致。
- `sentinel=5a` 证明缓冲区**仍在映射、内容可读**——这不是 free 掉内存，是页被换出。
  这一点必须写明，否则「RSS 降了」会被误读成「内存被释放了」。

### 术语纪律

这是**匿名页换出（page-out）**，不是「释放」。页还在 swap 里，回访时仍会缺页读回。
对系统的意义是：这些冷页让出了物理内存，代价是 swap 占用与后续回读延迟。
