# S4 守卫速率化+遥测缺失跳过+三态判定 · Worker 断点（task-mwg594lc · 2026-11-14 16:33）

状态：调研完成、代码未动笔。思考额度熔断中断，非技术阻塞。恢复后从「下一步」直接继续。

## 已查明的关键事实（一手验证）

1. 改动落点是并行 worktree `/home/arlune_innersea/.codex/worktrees/q12-dof/memreduct-linux-zamak`
   （分支 q12/integration，HEAD e8ca63f，含 1527543 修复）。主工作区（feat/issue-3-p2c-settings-ui）
   没有 balance_bridge.c，勿在主工作区动手。
2. `native/balance_bridge.c`（847 行）现状：
   - `mr_pressure_swap_delta()`（约 725 行）：pswpout 全机计数差，没除以时间，不是速率。
   - `mr_pressure_worker()`（约 744 行）：守卫用 avg10 涨超 8.0 + swap_delta>60000 停；
     psi_base<0 或读不到时守卫条件被跳过（`psi_now >= 0.0 && psi_base >= 0.0` 才判）继续跑；
     EAGAIN break 发生在采样该批正增量之前（rounds_completed 在写成功后赋值，EAGAIN 那批
     的 avail 差值被丢弃，只置 partial_flags）；root/path 检查失败只置 observed_end_to_end=-1。
   - `mr_bal_bridge_pressure_start()`（811 行）：成功返回 0，已运行 -2，pthread_create 失败 -1。
3. `src/main.zm`：:96-101 extern 声明（6 个 pressure 桥函数）；:2364-2374 S4 调用
   （start_rc==0 轮询 600×100ms，poll_rc==1 就 stages_ok+=1——即使 0 批完成）；
   :2377-2390 取 accumulated_positive/rounds/partial 出报告与 CI_DEEP_CLEAN。
4. 任务五点修法（来自主窗口调研笔记，直接采信）：①swap_delta 除以实际经过秒数
   （clock_gettime MONOTONIC 记窗口起点）得 pages/s，阈值 ~20000 pages/s；
   ②psi_base<0 或 PSI 读不到 → start() 返回 -3 专属码，S4 整个不启动；
   ③「批前检查不中断已提交内核写入」语义写进注释/文档；
   ④新增 getter `mr_bal_bridge_pressure_outcome()`：0=未运行/1=成功(全批完成)/
   2=部分成功(partial 或 rounds<max 因守卫停)/3=失败(start 失败或 rounds==0)；
   ⑤main.zm stages_ok 判定改 outcome==1 才 +1；EAGAIN break 后先读一次 avail 差值再退出。

## 下一步（依序）

1. board_report(kind=progress, taskId=task-mwg594lc)，question 以『计划改动：』开头报清单：
   worktree 内 `native/balance_bridge.c`、`src/main.zm`（touches 边界以任务文本为准）。
2. 改 balance_bridge.c：swap 速率化、PSI 缺失 → start 返回 -3、outcome getter、
   EAGAIN 后补采样、注释写明不可中断已提交写入。
3. 改 main.zm：新增 outcome extern 声明；S4 段 start_rc==-3 时 stages_ran 不增（或计未运行）
   并在状态/CI 行注明 skipped；stages_ok 判定改 outcome==1。
4. 验证：zam check . + zam test .（85 tests）+ zam build .；内核行为单测无法覆盖，标 NOT RUN 说明。
5. board_report(kind=complete) + send_message 给 parent（session-dad7b6ae-b58c-4e51-af83-3e5b95a4c0a5）。

## 检索入口

- memq 检索词：`task-mwg594lc S4 守卫 速率 outcome 断点`
- 本文件：`progress/S4-guard-telemetry-outcome-checkpoint-20261114.md`（q12-dof worktree）
- 账本指针：`log:session-未知子会话id#seq=未知`（子 worker 拿不到自身 seq，用检索词回取）
