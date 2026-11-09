## P3C：无特权时 helper 拒绝路径实测 —— 不需密码即可验证

2026-09-11-09。跨进程那一步因为需要管理员密码一直阻塞。但**"没密码时会怎样"**
这件事根本不需要密码就能验证，而且它恰恰是安全边界的关键。本轮补上。

### 前置：helper 没有 file capability

```
/usr/libexec/memreduct-zamak-crossproc-helper
-rwxr-xr-x root:root 48000
getcap → 无输出
find /usr/libexec -name 'memreduct*' -perm -4000 → 无
```

**既无 setuid 也无 file capability**，只能靠 pkexec 提权。
这排除了"普通用户直接运行即获特权"的低级风险。

### 实验一：合法数字但哈希猜测错误

```
memreduct-zamak-crossproc-helper reclaim pageout 1 <pid> <start> 1000 1 1 1 1000 1 4096
```

结果（完整 46 行协议输出，存 `artifacts/p3c/security/refused.txt`）：

```
protocol_version=4
status=refused
reason=not_root
start_time_verified=false
identity_hash_verified=false
scope_hash_verified=false
region_digest_verified=false
call_count=0
error_code=0
bytes_advised_is_not_bytes_freed=true
exit=1
```

关键三点：

1. **`status=refused` + `reason=not_root`**：明确拒绝，原因写明。
2. **`call_count=0`**：**一次 `process_madvise` 都没发起**。拒绝发生在系统调用之前。
3. **`identity_hash_verified=false`**：即便我给了合法数字，它也不接受我的哈希，
   而是自己去内核重算——与文档描述一致。

### 实验二：参数层面的拒绝

| 输入 | 退出码 | 判定 |
|---|---|---|
| 全零哈希（token=0）| 2 | usage 拒绝 |
| 64 字节随机哈希（非十进制）| 2 | parse 失败拒绝 |
| `max_bytes=4095`（小于一页）| 2 | 预算不足拒绝 |
| `pid=1` | 2 | PID 1 永不作为目标 |

四项全部在**解析阶段**被拒绝，未进入任何系统动作。

### 为什么这个结论重要

跨进程回收是**会真的调用内核 MADV_PAGEOUT** 的危险动作。
如果 helper 的拒绝逻辑有漏洞，那么任何人都能在没有授权的情况下对他人进程做页回收。

本轮证明了：

- 无特权时**拒绝且零调用**
- 哈希必须由内权重算，外部猜的不认
- 参数层有多重前置门槛（页大小、PID 1、非零 token）

这不能替代"授权后确实成功"的验证（那仍需密码），
但**能证明失败路径是安全的**——而这在建「真实可用」的产品里同样必须成立。

### 与待办的关系

- 仍需密码：授权后**真实执行一次**并观测 advised/accepted 字节。
- 本轮已完成：无密码时**确实拒绝**。

两者合起来才算"权限路径完整"。现在缺的只是成功侧的样本。
