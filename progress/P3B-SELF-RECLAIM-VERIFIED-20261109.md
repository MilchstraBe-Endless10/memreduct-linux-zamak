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

### 追加：1 GiB 规模下的覆盖率（`MR_SELF_BATCH` 上限不是总量瓶颈）

有人会怀疑 `MR_SELF_BATCH = 32` 意味着"只处理 32 个映射"。读源码可以确认不是：
`batch` 只是单次 `process_madvise` 调用最多带 32 个 iovec，**装满就立刻调用并清空，
然后继续读 `/proc/self/maps` 到文件末尾**。

1 GiB 实测：

```
eligible_regions=25  eligible_bytes=1076801536  (batches_of_32=1)
bytes_paged_out=1076375552   failed_batches=0   errno=0
sentinel=5a
```

覆盖率 = 1076375552 / 1076801536 = **99.96%**。
差额约 430 kB 来自内核不愿换出的少量页（例如被锁定的栈页），属正常。

同时注意：25 个映射只占 1 个 batch，说明 32 这个上限在实际进程里极少被触到；
即使触到也只是多调一次，不影响覆盖范围。

### 过滤条件（决定"能不能碰"的边界）

源码只挑 `perms[0]=='r'`（可读）且 `perms[3]=='p'`（私有）的映射：

- 私有匿名 / 私有文件背衬 → 是候选
- **共享映射被显式排除**：注释写明"shared mappings belong to other processes
  and must not be pushed out from under them"

这是一条很要紧的安全约束：共享库和共享内存属于别的进程语义，
对自己做 PAGEOUT 时把它们一起推出会连带影响他人。代码选择了不碰。
