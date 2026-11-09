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

---

## 补充：凭据校验链的完整顺序（源码，crossproc_helper.c:935-960）

```c
static const char *validate_helper_credentials(uint64_t real_uid,
                                               uint64_t effective_uid,
                                               const char *pkexec_uid,
                                               uint64_t *requester_uid)
{
    if (effective_uid != 0)   return "not_root";                        // ← 我在这一关
    if (real_uid != 0)        return "invalid_pkexec_root_credentials";
    if (pkexec_uid == NULL || pkexec_uid[0] == '\0')
                              return "not_launched_via_pkexec";
    if (parse_u64(pkexec_uid, ...) != 0)
                              return "malformed_requester_uid";
    if (parsed == 0)          return "invalid_requester_uid";
    ...
}
```

代码注释写得很清楚：

> *pkexec executes a root target after setreuid(0, 0), so the correct helper
> credentials are ruid=0/euid=0. PKEXEC_UID is only a caller declaration;
> the target's kernel-reported UIDs are independently checked below.*

### 逐条说明

| 关卡 | 条件 | 为什么必要 |
|---|---|---|
| 1 | `euid != 0` → 拒绝 | 未提权时直接挡在门外 |
| 2 | `ruid != 0` → 拒绝 | 被 setuid 篡改过的进程要拦 |
| 3 | `PKEXEC_UID` 为空 → 拒绝 | 必须真由 pkexec 拉起，非该路径不许 |
| 4 | `PKEXEC_UID` 非数字 → 拒绝 | 防格式化/溢出 |
| 5 | `PKEXEC_UID == 0` → 拒绝 | root 不能作为调用者 |

### 关键设计：声明值与内核值分离

注释点明 `PKEXEC_UID` **只是调用者的声明**（环境变量，可伪造），
真正的 UID 取自内核报告，且"independently checked below"。

这是整个安全模型的核心：**环境变量不可信，内核数据才是权威**。

### 我这一轮的验证边界（必须说清）

我在第 1 关就被拦下。因此：

| 关卡 | 我是否实测 |
|---|---|
| 1 `not_root` | ✅ 实测（本轮）|
| 2 `invalid_pkexec_root_credentials` | ❌ 需要提权后才能构造 |
| 3-5 `PKEXEC_UID` 相关 | ❌ 同上 |
| 目标所有权 / cgroup 校验（1737-1788）| ❌ 全在凭据校验之后 |

**所以我只证明了"未提权时拒绝"，没有证明"提权后其余关卡仍会拦住伪造请求"。**
后面的关卡需要能进入 root 上下文才能测，那正是被密码卡住的部分。

这条界限必须写清，否则容易把"第一关通过"读成"整个校验链没问题"。

---

## 补充二：重复请求与审计覆盖

### 重复请求：三次完全一致

同一 `request_id=7`、同一目标、连续三次：

```
第1次: status=refused reason=not_root request_id=7 call_count=0
第2次: status=refused reason=not_root request_id=7 call_count=0
第3次: status=refused reason=not_root request_id=7 call_count=0
```

源码确认 `request_id` 只被解析、回显、传给审计，**不参与去重判定**
（`grep request_id` 只有 parse / `outcome.request_id =` / `emit` / 审计四处）。

含义：这个 helper 是**无状态的一次性进程**——每次 pkexec 都拉起一个新进程，
不存在"同 id 已处理过"的记忆，所以天然幂等的是**结果**而非**状态**。
调用方重试是安全的：最坏情况是被再拒一次，不会叠加副作用（`call_count=0` 已证）。

### 审计只覆盖"即将真正执行"的时刻

审计调用在源码里只有两处：

```c
mr_xproc_audit_emit("helper_pre_syscall", ...);   // 恰在 process_madvise 之前
...
mr_xproc_audit_emit("helper_pre_scope_write", ...);
```

两者都在**所有前置校验通过之后**。因此：

| 事件 | 是否留审计 |
|---|---|
| 被 `not_root` 拒绝 | **否** |
| 被参数解析拒绝 | **否** |
| 被 cgroup/所有权校验拒绝 | **否**（若在此之前 return）|
| 即将真正发起 `process_madvise` | **是** |
| app_scope 即将写内存统计 | **是** |

### 这个设计的选择与代价

**选择**：审计记录"实际要动系统"的动作，不记录"被挡在门外"的尝试。

**好处**：审计文件不会因任意用户反复调用而膨胀——
任何一个 uid 1000 的进程都能触发 `not_root`，如果记录它，日志可被无成本刷爆。

**代价**：**拒绝事件不可追溯**。想回答"是谁在反复尝试跨进程回收但没有权限"，
从审计里查不到。这类需求需要另外的机制（如 pkexec 自身的日志）。

这不是缺陷，是个明确的权衡。写清楚，免得日后有人按"审计应覆盖所有请求"的预期去查而落空。

### 本轮累计验证的边界（更新）

| 项 | 状态 |
|---|---|
| helper 无 setuid / 无 file capability | ✅ 实测 |
| 未提权时 `not_root` 拒绝、`call_count=0` | ✅ 实测 |
| 参数层四类拒绝（零 token / 非十进制 / 子页预算 / pid 1）| ✅ 实测 |
| 重复请求结果一致、无叠加副作用 | ✅ 实测 |
| 审计仅在 syscall 前触发 | ✅ 源码确认 |
| 提权后凭据 2-5 关 | ❌ 需密码 |
| 提权后所有权 / cgroup / 哈希重算校验 | ❌ 需密码 |
| 提权后成功回收 | ❌ 需密码 |
