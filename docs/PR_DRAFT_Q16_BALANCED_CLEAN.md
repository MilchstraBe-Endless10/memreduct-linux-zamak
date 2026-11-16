# feat: add scoped manual balanced memory reclaim

> PR 标题：`feat: add scoped manual balanced memory reclaim`　目标分支：`main`



> 状态：草稿，未推送、未合并、未安装、未发版。
> 可审分支：`review/q16-balanced-clean`（组织者：本机，未 push）

## 这个 PR 是什么

给 Linux 版加一条**按应用定向、仅文件页**的回收路径，以及一条能被事后核对的执行记录。
用户从菜单选一个应用，程序只对那个 cgroup 的 `memory.reclaim` 写入，不碰匿名页，
不扫全机缓存，不换出进程。

## 发布口径

**按需回收所选应用的部分文件缓存；后续使用可能使缓存重新增长。**

这一句同时是产品文案和本 PR 的承诺边界：它承诺"回收动作会发生"，
不承诺"回收之后内存保持低位"。

面板上的原文就是这样写的（`src/main.zm` 的 `bal_policy_result_text`），
并有测试禁止把它改回"持续保持低位"之类的话。

## 入口可达性（以本候选产物为准，不以源码里有函数推断）

产物：`memreduct-zamak-production`，ELF x86-64 PIE，1,291,848 字节，
SHA-256 `373c8a271d5c728abfb960d5d858b7f002338e8a712923cb9e615d9d81ef675d`。

### 本 PR 唯一列为已验收的清理能力

| 能力 | 状态 | 依据 |
|---|---|---|
| **手动定向均衡清理**（File → Balanced clean） | **已验收，可用** | 产物含 `mr_bal_bridge_*` 全套符号与 `Balanced clean — choose one application` 文案；一次真实鼠标点击产生了到 `memory.reclaim` 的两次 `errno=0` 写入（见下节） |

### 此构建禁用（不是"授权后可用"）

| 能力 | 状态 | 依据 |
|---|---|---|
| **全局页缓存清理** | **此构建禁用** | `mr_production_clean_enabled` 在产物里是 `xor %eax,%eax; ret`（恒返回 0） |
| **跨进程 PAGEOUT**（Select user app） | **此构建禁用** | `mr_xproc_capability_enabled` 同为 `xor %eax,%eax; ret`（恒返回 0） |

这两个 gate 是**编进二进制的常量**，不是运行时开关，环境变量打不开，
源码里有对应函数也不代表这个二进制能走到。要启用必须改构建配置并重新出包。

### 深度清理：合并前待决项

**这是本 PR 唯一的开放决策，需要评审者拍板，不由本 PR 单方面决定。**

**建议：本构建禁用入口，保留后端代码。**
理由：后端逻辑（护栏、分批、EAGAIN 归类、阶段记账）有价值，删掉是浪费；
但按下面三条实测事实，它现在不适合以"可用功能"出现在菜单里。
**本次只准备送审，不改深度入口代码** —— 这一条留作评审决议后的后续改动。

按实际代码追踪，逐条说明为什么不能把入口当作可用能力：

**S1 构建期禁用。** 由 GUI 层驱动，走 `pkexec` 到特权 helper，落到全局页缓存路径，
而该路径受 `mr_production_clean_enabled` 拦截，本构建恒返回 0。
**不是"拿到授权就能跑"** —— 这一步在这个二进制里出不去。

**S2 / S3 作用于整会话，不是单个应用。** 两个阶段都调用
`mr_bal_bridge_session_run(0)`，作用对象是 `mr_bal_exec_session_root()`，
即整个 user session。这与本 PR"按应用、按需"的定位相反。

**S4 可能换出匿名页。** 写入是裸字节数，**没有 `swappiness=0`**：

```c
const uint64_t batch = 512ull * 1024ull * 1024ull;   /* 512 MiB */
const int max_rounds = 8;
int batch_len = snprintf(batch_text, sizeof(batch_text), "%" PRIu64, batch);
mr_bal_write(wfd, batch_text, (size_t)batch_len);     /* 裸字节数，无 swappiness */
```

对比均衡清理的写入（`native/balance_exec.c:182`）：`"%llu swappiness=0"`。
`balance_reclaim.c` 的注释写明"there is no code path that writes a non-zero swappiness"，
所以均衡清理只碰文件页；S4 无此约束，内核可以自由选择换出匿名页。

S4 已有的护栏（每条不满足就整阶段不跑）：PSI 与 pswpout 须可读，否则
`MR_DEEP_STOP_PSI_UNREADABLE`；每批之前检查 swap-out 速率与 PSI 增长
（超 8.0 avg10 点即停）；最多 8 批 × 512 MiB；用户取消在下一批之前生效。

**结论：本 PR 不把深度清理的任何阶段列为已验收。** 入口是否存在、符号是否存在，
都不构成可用性证据。能确认的只是上面这些代码事实；
这条管线**在本次交付里没有执行过，也没有验收证据**。

## 已获得的真机证据（复用，未重复执行）

一次真实鼠标点击（修复后的候选 `527b41b1…`）产生的记录：

```
MEMREDUCT-BALANCED id=1 scopes=4
  target=.../app-com.google.Chrome-729959.scope/memory.reclaim
  writes=2
  w[0]={bytes=14 text="0 swappiness=0"        ret=14 errno=0 class=accepted}
  w[1]={bytes=21 text="23899136 swappiness=0" ret=21 errno=0 class=accepted}
  reason=none
```

第一次写入是能力探测（`0` 字节），第二次是真实请求 23,899,136 字节，两次 `errno=0`。
这三件事必须分开读，不能合并成一个"释放了多少"：

| 问题 | 答案 | 依据 |
|---|---|---|
| 内核接受请求了吗 | **是** | 两次写入 `ret=14` / `ret=21`、`errno=0`、`class=accepted` |
| 目标文件页短时变了吗 | **是，但不持久** | P1 表格：−26.5 MB → 60 秒内涨回并超过起点 |
| 持续收益成立吗 | **未证实** | 目标文件页 60 秒内回升并超过起点；未观察到大量已驱逐文件页被重新访问；回升来源未确定 |

"内核接受 23,899,136 字节的请求"与"释放了 23.9 MB 物理内存"是两件事，
本 PR 只主张前者。

## P1：短时目标计数（只报这两项）

目标 `app-org.chromium.Chromium-9007.scope`，请求 33,554,432 字节，
对齐记录里的 `t_mono_ms` 采样：

| 时刻 | file | inactive_file | anon | refault |
|---|---|---|---|---|
| 前 | 124.6 MB | 105.4 MB | 48.7 MB | 12.29 MB |
| +1 s | 98.1 MB | 75.8 MB | 50.6 MB | 12.29 MB |
| +10 s | 107.2 MB | 84.9 MB | 49.6 MB | 12.30 MB |
| +30 s | 124.9 MB | 102.0 MB | 51.3 MB | 12.30 MB |
| +60 s | 158.8 MB | 128.6 MB | 48.2 MB | 12.32 MB |

一句话：第一秒掉了 26.5 MB，一分钟内回升并超过起点。

`workingset_refault_file` 全程只动 0.03 MB（12.29 → 12.32），
即**未观察到大量已驱逐文件页被重新访问**。
但回升的来源**未确定**。`workingset_refault_file` 计数低，只能排除
"大量已驱逐页被立即访问"这一种解释，不能反推回升一定来自新写缓存——
也可能来自映射变化、其他 scope 的合并计账或采样间隔漏掉的短时重读。
**持续收益未证实。**

**不把请求量换算成整机释放量。**

## P2：留存收益与切回代价

- **留存收益未证实。** 观察臂被弃用：当时 PSI some avg10 = 5.81%（超过策略自己的 5.00%），
  MemAvailable 708 MB / 15.4 GB，目标 scope 的 file 页在五分钟内单步波动 ±18.7 MB。
  整机正在主动回收这个 scope，对照臂和清理臂都会被同一只外来的手改写，差值无法归因。
- **切回代价未测。** 没有成立对照，单独测切回没有比较对象。
- 没有为此强行制造条件：未挂起进程、未造负载、未改用匿名页、未关闭任何应用。

## 差异构成

本 PR 从 `origin/main`（`83dbf58`）直接分出，**只有一个提交**。
这是刻意的：审查分支上原有 329 个提交，夹带了 6 个 ELF（含
`deploy-backup/memreduct-zamak.pre-deploy`）、26 张截图、19 个验收日志和
37 个 `progress/` 笔记；那些文件即便在最终树里被删掉，仍会随新增历史一起推上去。
所以本分支不含任何产物、备份或本机证据。

实测：从本分支重建生产产物得到
`373c8a271d5c728abfb960d5d858b7f002338e8a712923cb9e615d9d81ef675d`，
与既有验收记录所用的二进制**逐字节相同**。源码未改动，验收证据成立。

## 测试

```
python3 -m unittest discover -s tests  ->  439 tests, OK (skipped=4, expected failures=1)
zam check .                            ->  CHECK OK memreduct_zamak
```

**构建会因该用例而中止（实测）：** 首次运行 `build-production-gui.sh` 时它就是
`FAILED (failures=1)`，构建随之退出；第二次重跑才 `OK`。

**已知的顺序依赖（保留，不掩盖）：**
`test_s4_outcome.S4OutcomeTest.test_completed_run_reports_no_stop_reason`
在整套运行时偶发失败，单独运行必过——它受前序测试留下的进程内 C 状态影响。
这是既有问题，本 PR 不修。**单独通过不等于整套稳定**，评审时请以整套结果为准
并预期该用例可能抖动。
