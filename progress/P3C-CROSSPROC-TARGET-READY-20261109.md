## P3C 跨进程回收：受控测试目标已就绪并通过只读准入（待特权一步）

2026-11-09 在本机完成，除最后一步真实 `process_madvise` 外全部为非特权验证。

### 1. 同 UID 直呼确认需要特权（历史结论复验）

对自建目标进程直接调用 `process_madvise(pidfd, iovec, 1, MADV_PAGEOUT, 0)`
（x86_64 syscall 440）→ `rc=-1 errno=1 (EPERM)`。
证实必须有 `CAP_SYS_NICE` 的固定 helper 经 Polkit 授权，不能由 GUI 直接调用。

### 2. 受控测试目标的准入发现（重要）

初次把目标进程作为 DSH 会话子进程启动，其 cgroup 为
`…/app.slice/dsh-subprocess-529389-<id>.scope`，
**被 `mr_xproc_cgroup_path_is_user_app` 判为不可回收**
（该规则要求路径最后一段必须是 `app-*.scope` 形式）。

这是**有意的保护设计**，不是缺陷：把任意 `*-subprocess-*.scope` 排除，
避免对 DSH/宿主自己派生的进程组动手。

用 `systemd-run --user --scope --unit=app-xtarget` 放入标准 app scope 后：

```
cgroup = /user.slice/user-1000.slice/user@1000.service/app.slice/app-xtarget.scope
pid=1332092 name=xproc-target rss=337711104 private_resident=335622144
       reclaimable=338268160 regions=17
```

**已被 `mr_cand_scan` 收录为合格候选**，private_resident = 320 MiB，
满足 ≥256 MiB 的大额度要求。

### 3. 只读扫描基线（本机真实数据）

```
scanned=578 offered=80 skipped_self=1 other_uid=354 protected=139
no_regions=0 ancestor=0 limit=0
```

候选上限 `MR_CAND_MAX=256`，本次 80 个，未触顶 —— 目标未被收录并非名额问题。

### 4. 待批准的唯一特权步

在 `pid=1332092`（`app-xtarget.scope`，320 MiB 匿名，UID=1000 本用户）上，
经 `pkexec` 调用 `/usr/libexec/memreduct-zamak-crossproc-helper reclaim pageout …`
执行一次真实 `MADV_PAGEOUT`。

- 范围：单一进程、单一 region 集、跑完即终止该 scope
- 影响：320 MiB 匿名页向 swap 写出；本机 swap 余量约 19.5 GiB
- 风险：低；最坏是被 helper 的 claim 校验 fail-closed 拒绝
- 撤销：无持久变更，`systemd-run --user --scope` 单元随进程结束消失

批准后即执行，并把 helper 返回的 accepted/advised 字节与前后 RSS 观察值一并上报。
