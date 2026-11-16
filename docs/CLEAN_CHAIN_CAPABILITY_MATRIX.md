# 清理链路能力矩阵（C 线交付物）

状态：证据核查稿，非验收通过。本矩阵只记录「代码里实际做了什么 + 证据在哪」，不做产品结论。
生成日期：2026-11-15。核查基线：worktree `q12-dof`，分支 `q12/integration`，HEAD `2e8e1f1`。
已安装二进制：`/usr/bin/memreduct-zamak`，sha256 `1c3ef7e06631828ede11a742d66fecd5213b7c8a7348d0c1cf4120e389eb468e`。

## 0. 规格来源与基线（按用户 2026-11-15 指示记录）

| 项 | 内容 |
|---|---|
| 清理链路规格来源 | Issue #13 原文（`gh issue view 13`），正文 8483 字符 |
| 原文已取得 | 是 |
| 取用时间 | 2026-11-15T00:23:29+08:00（本机） |
| 是否回读评论区 | 是，33 条评论全部回读（缩略逐条过目 + 关键节点全文） |
| UI/交互规格来源 | Issue #3，正文 7353 字符 + 27 条评论 |
| Issue #3 取用时间 | 2026-11-15T00:24:35+08:00 |

未降级：两份 Issue 原文均为在线直取，非二手转述。

## 1. 上游基线 SHA 不一致 —— 已澄清

结论标记：**已澄清**。依据文档：`docs/UPSTREAM_VERSION_DRIFT_3.5.2_vs_04402de8.md`（2026-10-25 归档）。

该文档第 1 节、第 2 节、第 6 节给出明确结论：权威发布源码固定为 tag v.3.5.2 =
`2f5c55d15f69a2c88859e036b16c6f4c971e1968`；旧调查快照
`04402de8b6a8ff9d1c5517c675d87c346655323c` 位于该 tag 之后 46 个提交，仅作发布后漂移分析，
不得再用作 3.5.2 规范。该文档另附证据：`04402de8` 的 `src/app.h` 已写 3.5.3，
而 `VERSION` 仍写 3.5.2 —— 即快照版本元数据自相矛盾，不能代表发布版。

### 两个 SHA 各自的来源（如实并列，不作裁决）

| SHA | 出处 | 角色 |
|---|---|---|
| `04402de8b6a8ff9d1c5517c675d87c346655323c` | Issue #3 **正文**（原始规格）。正文全文只出现这一个 SHA，`2f5c55d` 在正文中出现 0 次 | 最初被当作 3.5.2 基准的提交 |
| `2f5c55d15f69a2c88859e036b16c6f4c971e1968` | Issue #3 **评论[2]**（2026-10-25T07:41:10Z，id `IC_kwDOVWhK688AAAABfSvOLQ`，「ChatGPT P2A preliminary architecture review」的 P0 finding）；其后 AGENTS.md:8、docs/PRD.md:8、docs/ARCHITECTURE.md:33、docs/UPSTREAM_UI_BASELINE.md:9 一致采用 | 现行权威基线（官方 tag `refs/tags/v.3.5.2`） |

裁决原文（评论[2]）：「GitHub contains **official upstream Git tag** `refs/tags/v.3.5.2` -> commit
`2f5c55d15f69a2c88859e036b16c6f4c971e1968`. On that commit both `src/app.h` and `VERSION` say 3.5.2.
By contrast, P2A's provisional `04402de8…` is 46 commits after this tag」。

补充事实：评论[3]（Codex，2026-10-25T08:21:04Z）已接受该纠正并执行。
即：**这不是未解决的分歧，是已在评论区完成、并被仓库文档固化的纠正过程。**
矩阵据此记「已澄清」；哪个 SHA 作为最终产品基线属 owner/reviewer 决策，本矩阵不代裁。

## 2. 四阶段链路：范围区分（S1 与 S2-S4 不是同一范围）

这是本矩阵最重要的一条：`src/main.zm:2375` 的范围文案把四阶段说成「后续阶段」统一作用，
但代码显示 S2/S3 与 S4 的作用域语义并不相同。

| 阶段 | 入口/实现位置 | 内核接口 | 实际作用范围 | 文件页限制 |
|---|---|---|---|---|
| S1 | `src/main.zm:2386-2395` → `native/async_privileged.c:114` → `native/memreduct_helper.c:24,32` | 向 `/proc/sys/vm/drop_caches` 写 `1\n` | **全机**全局页缓存（含其他用户） | 仅干净页缓存（写 1） |
| S2 | `src/main.zm:2396-2400` → `native/balance_bridge.c:564-624` | cgroup v2 `memory.reclaim` | **本会话 cgroup**（`mr_bal_exec_session_root()`，非选定应用） | **是**：`native/balance_exec.c:179` 固定写 `"<bytes> swappiness=0"` |
| S3 | `src/main.zm:2401-2406`（与 S2 同一函数，settle 后再跑一遍） | 同 S2 | 同 S2（本会话 cgroup） | **是**：同 S2 |
| S4 | `src/main.zm:2407-2426` + `native/balance_bridge.c:745-834` | cgroup v2 `memory.reclaim` | **本会话 cgroup**（`balance_bridge.c:748,754` 拼 `<session_root>/memory.reclaim`） | **否**：`balance_bridge.c:801-803` 只写裸字节 `fprintf(f, "%" PRIu64, batch)`，**不带 swappiness=0** |

由此可验证的技术结论：

1. S1 是全机动作，S2/S3/S4 是会话级动作 —— 「全机 vs 会话」这层区分在代码里成立。
2. 「冷匿名页可能压入 ZRAM/swap」只对 **S4** 成立。S2/S3 因写死 `swappiness=0`
   被限制在文件页（`native/balance_reclaim.h:21-24` 明确要求不得退化为裸字节写入）。
   当前 `src/main.zm:2375` 把这句话挂在「later stages」（后续阶段）整体上，对 S2/S3 是过度陈述。
3. 反向也成立：S4 的裸字节写入确实可能涉及匿名页，文案在这一点上没有说谎。

该项记为 **已知缺陷（范围文案过度概括）**，详见第 5 节 D-1。

## 3. 能力矩阵（每条「已实现」均附可验证证据位置）

证据类型约定：`文件:行号` = 源码事实；`测试名` = 可复跑的断言；
`CI 脚本:行` = CI 标记断言。**没有三者之一者不标「已实现」。**

| # | 能力 | 结论 | 证据位置 |
|---|---|---|---|
| C-01 | S1 经 pkexec 调专用 helper，GUI 保持非 root | 已实现 | `native/async_privileged.c:26,30`（只传固定 `pagecache` 枚举）、`:114`（固定 helper 绝对路径 `/usr/libexec/memreduct-zamak-helper`） |
| C-02 | S1 helper 只接受固定动作 `pagecache`，拒绝 metadata/all | 已实现 | `native/memreduct_helper.c:20-23`；测试 `test_native.py::test_test_helper_rejects_metadata_and_all_without_writing`、`::test_helper_dry_run_and_reject_unknown_mode` |
| C-03 | S1 helper 只写固定路径 `/proc/sys/vm/drop_caches`，拒绝 symlink/非常规文件 | 已实现 | `native/memreduct_helper.c:32`（固定路径）、`:41`（`O_NOFOLLOW`）、`:54-58`（`S_ISREG` 检查）；测试 `::test_test_helper_rejects_symlink_target`、`::test_test_helper_rejects_non_regular_fifo_target` |
| C-04 | S1 短写/close 失败报「结果不确定」且不自动重试 | 已实现 | `native/memreduct_helper.c:70-75`（返回码 3）；测试 `::test_test_helper_reports_partial_write_as_uncertain_without_retry`、`::test_test_helper_reports_close_error_as_uncertain`、`::test_test_helper_retries_eintr_once` |
| C-05 | S1 为编译期能力闸（默认构建不可达） | 已实现 | `zam.toml` sources 含 `production_clean_gate_default.c`；`native/production_clean_gate_default.c`（返回 0）、`native/production_clean_gate_enabled.c`（返回 1）；`src/main.zm:2386` 调用点；测试 `test_native.py::test_production_clean_is_a_compile_time_gate` |
| C-06 | S1 异步启动 + 轮询，不阻塞画布 | 已实现 | `src/main.zm:2388-2395`（`mr_start_reclaim(0)` 后 `while attempt < 200 and polled == -1` 轮询，非阻塞事件循环） |
| C-07 | S2/S3 作用于本会话 cgroup，且预算夹到会话自身文件页上限 | 已实现 | `native/balance_bridge.c:564-599`（`mr_bal_exec_session_root()`、`file_bytes` 为唯一上限）；测试 `test_balance_bridge.py::test_scope_root_is_absolute_and_under_the_cgroup_mount` |
| C-08 | S2/S3 每次写入必带 `swappiness=0`，不退化 | 已实现 | `native/balance_exec.c:179-180`；测试 `test_balance_reclaim.py::test_every_write_carries_swappiness_zero`、`::test_eagain_never_falls_back_to_a_bare_byte_count`、`::test_swappiness_zero_takes_no_anonymous_page` |
| C-09 | 内核不支持 swappiness=0 时报「不支持」，不静默降级 | 已实现 | `native/balance_reclaim.h:21-24,35`、`native/balance_reclaim.c:343`；测试 `::test_no_swappiness_support_refuses_instead_of_degrading` |
| C-10 | S4 在 worker 线程分批执行，GUI 轮询不阻塞 | 已实现 | `native/balance_bridge.c:745,841,852`（worker + start/poll）；`src/main.zm:2412-2421`（start 后立即返回，交给 tick） |
| C-11 | S4 每批前检查 PSI 与 swap 速率守卫，越线即停 | 已实现 | `native/balance_bridge.c:762`（`psi_growth_limit=8.0`）、`:782-798`（批前守卫，swap 速率 >20000 页/s 置 `stopped_by_guard`） |
| C-12 | S4 缺 PSI 遥测时拒绝运行，而非无守卫硬跑 | 已实现 | `native/balance_bridge.c:765-769`（返回 -3） |
| C-13 | S4 EAGAIN 轮次保留采样并标 partial，不丢弃 | 已实现 | `native/balance_bridge.c:804-818`（`partial_flags \|= 1` 后仍采样累加） |
| C-14 | S4 三态 outcome（1 完成 / 2 提前停止 / 3 未运行） | 已实现 | `native/balance_bridge.c:872-882`（`mr_bal_bridge_pressure_outcome()`）；`src/main.zm:2155,2162-2185`（三态文案） |
| C-15 | 结果口径区分「全程净增量」与「累计正增量」 | 已实现 | `native/balance_bridge.c:832`（end-to-end）、`:827`（accumulated）；`src/main.zm:2156-2165`（两个字段分别展示） |
| C-16 | 深度清理有二次确认，先展示范围再执行 | 已实现 | `src/main.zm:2372-2379`（`deep_scope_ack` 首按只显示范围并 return，再按才执行） |
| C-17 | S4 完成后进度文案在 tick 中收尾，不在 handler 内读取 | 已实现 | `src/main.zm:2151-2195`（tick 内 poll==1 时收尾）、`:2408-2411` 注释明确「handler 从不读 pressure getter，避免竞态」 |
| C-18 | 入口可达：File 菜单第 3 行 + F10 快捷键 | 已实现 | `src/main.zm:398`（`row == 2 → 104.0`）、`:1435-1436`（菜单项 + F10）、`:2760-2764`（F10 分派到 `activate(s, 104.0)`） |
| C-19 | S1 全机清除的事实有用户级规格依据（非实现断言） | 规格要求 | Issue #13 §2 功能 A：固定只写 `1\n` 到 `/proc/sys/vm/drop_caches`；ADR-0005 §Proposed boundary 1-6 |

### 证据缺口（不得标「已实现」的项）

| # | 声称 | 为什么不能标已实现 | 现状 |
|---|---|---|---|
| G-01 | S4 三态结果有自动化验收 | `tests/test_s4_outcome.py` 六条断言：EAGAIN 必须记为 PARTIAL(5) 而非 ERROR(4)、取消是独立结局、正常结束无停止原因、S2/S3/S4 各记 ran、完成后不再提交、六个停止原因值互不冲突。断言落在 bridge 层（结局实际被决定的层），GUI 只负责打印 | **已修复** |
| G-02 | S4 守卫停在自动化中可复现 | `tests/test_s4_outcome.py` 用可注入的写接缝 + 会话根覆盖，在 tmpfs 目录上驱动 S4 循环：拒发后统计数冻结、`batches_completed` 为 0、停止原因可区分 | **已修复** |
| G-03 | 已安装二进制对应 HEAD `2e8e1f1` | 已安装 `1c3ef7e0…` mtime 22:33:38，`7cf3c4f` 提交时间 22:41:45 —— 二进制早于该提交；其含 `CI_DEEP_S4` 说明含 `7cf3c4f` 内容。本轮新产物哈希已登记（`54e1f22a…`，见第 4 节），但**已安装那份是从哪次源码构建的、对应哪个 commit，仍无记录可核验**；且已证实其文案为修复前版本（第 4 节）| **已知缺陷（已安装二进制的构建溯源缺记录）** |
| G-04 | S4 走会话 cgroup 有单元测试覆盖 | 新增 `mr_bal_bridge_test_set_session_root()` 覆盖点；`tests/test_s4_path.py` 用临时目录假扮 cgroup，断言 S4 能到达、缺 `memory.reclaim` 时不提交批次且留下停止原因、清除覆盖后恢复发现 | **已修复** |
| G-05 | 四阶段整体净增量的「端到端」在对用户陈述时无测量区间说明 | 文案改为「运行期间可用内存增加 X MiB（这是观测值，不是承诺：同一时间其他程序也在分配内存）」，中英双语；不再把观测差值表述为净释放量 | **已修复** |
| G-06 | 文案修复已随新产物交付 | 新产物 `235e0471…` 经 `grep -aF` 核验：旧的过度概括串 `later stages reclaim the whole session` **0 处**，`Available memory rose` 1 处、`观测值` 4 处。代码与产物均已修复 | **已修复（产物层）；部署待用户确认** |

## 4. 发布差异检查

| 检查项 | 方法 | 结果 |
|---|---|---|
| 已安装二进制 vs 源码 HEAD | `sha256sum` + `grep -aF` 比对标记 | 已安装 `/usr/bin/memreduct-zamak` sha256 `1c3ef7e0…`；含 `CI_DEEP_S4`/`CI_DEEP_CLEAN`（证明含 `7cf3c4f` 内容）；但 mtime 早于该提交，构建产物无登记哈希 → 见 G-03 |
| 二进制不含 `CI_DEEP_BLOCKED` | `grep -aF` | absent（该标记在源码中亦不存在，一致） |
| **文案修复是否已部署到用户机** | `grep -aF` 独有长串，对比已安装二进制与新构建产物 | **未部署**。已安装二进制：旧串 `later stages reclaim the whole session` 命中 **1** 处，新串 `anonymous pages untouched` 命中 **0** 处 → 用户机上仍是过度概括版本。新构建产物 `artifacts/p0a/…`：旧串 **0** 处，新串 **1** 处 → 修复只在产物里，尚未安装 |
| 新产物内四项修正确证 | `grep -aF` 独有长串 | `anonymous pages untouched`=1、`unrelated allocation and reclaim`=1、`batches completed so far`=1、`the pressure guard fired`=2（含合并版），均 ≥1 处 |
| 桩/门控文件是否混入生产构建 | 读 `zam.toml` | 默认构建含 `production_clean_gate_default.c`（返回 0），不含 `_enabled`；生产脚本 `scripts/ci/build-production-clean-gui.sh` 换入 enabled 版 |
| 构建产物哈希登记 | `sha256sum` | 产物 `54e1f22a…b1ef74`、helper `b0652c8a…c45a5e`，与 `artifacts/p0a/production-clean-sha256.txt` 一致；但与已安装二进制哈希不同（见上一行）|
| 不该进发布包的东西 | 逐项核对 | ①`artifacts/p0a/` 整个目录（含 1.2 MB 二进制、symbols dump 157 KB）：构建产物，本次**不纳入提交**（`git status` 中保持 untracked）；②`production_clean_gate_enabled.c` 只由生产构建脚本换入，不得进默认构建；③`docs/CLEAN_CHAIN_CAPABILITY_MATRIX.md` 为本轮交付文档，应提交；④`progress/*.md` 两个未跟踪断点文件属其他 Worker 的运行记录，本次不纳入 |
| 测试基线 | `python3 -m unittest discover -s tests` | 326 tests，OK（skipped=4），20.566s；另 `zam test .` 85 passed；`zam check` CHECK OK |
| 工作区改动范围 | `git status --short` / `git diff --stat` | `M src/main.zm`（+27/−8，4 处文案 + 1 处进度显示）；`?? docs/CLEAN_CHAIN_CAPABILITY_MATRIX.md`（本文件）；`?? artifacts/p0a/`（构建产物，未纳入版本控制）。未触碰 native/、tests/、scripts/ |
| 桩/门控文件是否混入生产构建 | 读 `zam.toml` | 默认构建含 `production_clean_gate_default.c`（返回 0），不含 `_enabled`；生产脚本 `scripts/ci/build-production-clean-gui.sh` 换入 enabled 版 |
| 测试基线 | `python3 -m unittest discover -s tests` | 326 tests，OK（skipped=4），20.566s |
| 工作区改动范围 | `git status --short` / `git diff --stat` | `M src/main.zm`（+27/−8，仅 4 处文案与 1 处进度显示）；`?? docs/CLEAN_CHAIN_CAPABILITY_MATRIX.md`（本文件）；`?? artifacts/p0a/`（构建产物，未纳入版本控制）。未触碰 native/、tests/、scripts/ |

## 4.1 交付物清单

| 类型 | 路径 |
|---|---|
| 能力矩阵（本文） | `docs/CLEAN_CHAIN_CAPABILITY_MATRIX.md` |
| 源码改动 | `src/main.zm`（4 处文案：D-1 范围、D-2 口径、D-3 停止原因、D-4 进度） |
| 构建产物 | `artifacts/p0a/memreduct-zamak-production-clean`、`artifacts/p0a/memreduct-zamak-helper` |

提交状态：本轮已在 `q12/integration` 分支本地提交（未 push）；集成与推送由主会话统一处理。

## 5. 需修正的文案（四类）

本表记录**修复前**的状态，因此「位置」列使用修复前 HEAD `2e8e1f1` 的行号；
修复后位置见第 5.1 节。两套行号相差 27 行（本次改动净新增行数，集中在 tick 与 activate 两段）。

| 编号 | 位置（修复前 HEAD `2e8e1f1`） | 现状 | 问题类型 | 建议 |
|---|---|---|---|---|
| D-1 | `src/main.zm:2356-2357` | 「stage 1 clears the GLOBAL page cache machine-wide; later stages reclaim the whole session cgroup … and may push cold anonymous pages into ZRAM/swap」 | **范围过度概括** | 把「可能压入 ZRAM/swap」限定到 S4（压力阶段），S2/S3 改述为「只回收文件页（swappiness=0）」 |
| D-2 | `src/main.zm:2416-2422` | 「net +X.X GiB so far」在 S1-S3 结束时即报出 | **口径未披露并发** | 补一句该值含并发回收/分配影响，非本次动作净释放 |
| D-3 | `src/main.zm:2166-2169` | 「stopped early (guard fired or a batch was refused)」 | **两类停止原因合并** | 分别显示：`stopped_by_guard=1` 时说守卫触发；仅 `partial_flags!=0` 时说批次被拒 |
| D-4 | `src/main.zm:2175-2176`（S4 tick 轮询段，长任务） | 运行中 `poll_rc == 0` 仅 `redraw()`，无「已完成 N 批」进度 | **进度不可见** | 用 `mr_bal_bridge_pressure_rounds_completed()` 显示已跑批次数 |

D-2/D-3/D-4 与 `src/main.zm` 的 S1 断点任务（task-mwg59ej0）目标一致，属同一批收尾。

### 5.1 四类文案修正状态（2026-11-15 已落码并验证）

改后验证：`zam check` PASS（`CHECK OK memreduct_zamak`）、`scripts/ci/build-production-clean-gui.sh`
构建成功、`python3 -m unittest discover -s tests` 326 tests OK（skipped=4）。

产物：`artifacts/p0a/memreduct-zamak-production-clean`，sha256
`54e1f22a8c2ed91b0a22b7c2d9bdb0e3dfa95f6b8be19f22f0fa89f74bb1ef74`；
helper sha256 `b0652c8ac3ae70576e39b71fbdcd047a7c57f23873fec2e45b1b691b34c45a5e`。

| 编号 | 修正后文案要点 | 产物内确证串（`grep -aF`，1 处命中） |
|---|---|---|
| D-1 | 阶段 1 全机含所有用户；阶段 2-3 只回收会话 cgroup 文件页（swappiness=0，不动匿名页）；阶段 4 才可能压入 ZRAM/swap | `anonymous pages untouched` |
| D-2 | 净增量标注为「该区间 MemAvailable 观测值，同时含无关的分配与回收」 | `unrelated allocation and reclaim` |
| D-3 | 停止原因四选一（守卫+拒批 / 仅守卫 / 仅拒批 / 其它），不再合并陈述 | `the pressure guard fired`（2 处，含合并版） |
| D-4 | 运行中显示已完成批次数 | `batches completed so far` |

旧文案已在产物中消失：`later stages reclaim the whole session`、`guard fired or a batch was refused`
均为 0 处。

关于产物字符串核对的限制（如实记录）：`strings -a` 对本产物的判定不可靠 —— 编译器会做常量合并，
例如 `Deep clean pressure finished`、`no batches completed` 这两个确实存在于源码的串在 `strings`
输出里为 0 处，而 `grep -aF` 在部分短串上又受同池拼接影响。因此本次以「新文案独有的长确证串各命中 1 处」
为准，未把 `strings` 的缺失当作改动未生效的证据。

### 5.2 修正未覆盖的部分（明确留白）

- `src/main.zm:2386` 起 S1 仍只在 `mr_production_clean_enabled() == 1` 时执行，默认构建不含该能力；
  这是设计意图（ADR-0005），非遗漏。
- 停止原因的细分只影响 `outcome == 2` 分支；`outcome == 1`/`3` 文案未改动。
- D-4 新增的进度文案每 2s tick 覆盖一次 `s.status`，会覆盖 S4 启动时的初始提示 —— 属预期（进度优先）。

## 6. 本矩阵的自我约束声明

- 每条「已实现」均可由第三方按「证据位置」列复现：源码行号可 `sed -n` 直查，测试名可单跑，CI 脚本行可核对。
- 行号约定（2026-11-15 校准）：第 3 节能力矩阵、第 4 节发布差异检查使用**修复后工作树**行号（对照 `src/main.zm` 当前内容）；
  第 5 节「需修正的文案」记录修复前状态，使用修复前 HEAD `2e8e1f1` 行号，已在表头注明。两套行号相差 27 行。
  校准方法：对每处引用跑断言（`sed -n` 取该范围并检查是否含声称内容），全部命中后才写入。
- 本矩阵**没有**把「有测试」写成「已在真机验收」；真机证据只在 G-02 中如实标为人工观测且缺自动化。
- 本矩阵**没有**对两个上游 SHA 的正确性作裁决，只记录各自来源与已归档的澄清文档。
- 未运行真机特权清理（`drop_caches` 实际写入、S4 实际压力回收）—— 本矩阵全部结论来自源码、测试与已构建产物，不越权做现场破坏性操作。
