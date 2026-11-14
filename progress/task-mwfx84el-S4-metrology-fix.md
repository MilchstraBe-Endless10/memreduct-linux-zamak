# task-mwfx84el S4 计量修正 — 换气落盘（2026-11-14 12:50）

## 做到哪
- 调研完成，未动笔写代码。仓库改动为零。
- 真实代码位置：worktree `/home/arlune_innersea/.codex/worktrees/q12-dof/memreduct-linux-zamak`（分支 q12/integration @ a55c08e）。主仓库 cwd 是 feat/issue-3-p2c-settings-ui 分支，没有 S4 代码。
- 关键事实：
  - `src/main.zm:2344-2369`：S4 段 `delta_mib = if observed_mib>0: observed_mib else: pressure_mib`，两口径混用。
  - `native/balance_bridge.c:700 mr_bal_bridge_pressure_run()`：只累加 MemAvailable 正差值（负值被丢弃）、EAGAIN/首轮 fclose 失败时 `return -1` 丢掉部分回收采样、无整轮净值口径。返回单一 int64。
  - Zamak FFI 多返回值惯例：C 侧全局 struct + 逐个 getter（balance_bridge.h result_set 模式）。
  - 浮点显示先例：tenths 整数拼接 `f"{t//10}.{t%10} GiB"`（main.zm:529/533/1581/1585），无 .1f。
  - tests/test_native.py 无 CI_DEEP_CLEAN 断言；zam.toml 无需改。
- 开工闸口已走：board_report 子代理不可用，已用 send_message 向父会话上报『计划改动：』清单。

## 下一步（新 Worker 按此接手）
1. `native/balance_bridge.h` + `.c`：pressure_run 重构为填充全局 result struct，新增 4 getter：observed_end_to_end（整轮净值 avail_end-avail_start，可负）、pressure_accumulated_positive、rounds_completed、partial_flags（位标志：S4 部分回收/中断）。EAGAIN 短写时保留采样记 partial 位，不再丢首轮。
2. `src/main.zm` S4 段：两口径分开显示互不覆盖；GiB 换算按 tenths 先例；partial 标注；CI_DEEP_CLEAN 行拆 `net_mib= pressure_pos_mib= rounds= partial=`。
3. `memory/SHORT_TERM.md` 每轮记录。
4. 自测：`zam check .`、`zam build .`、`python3 -m unittest discover -s tests -v`；真机可选 `MEMREDUCT_CI_DEEP=1` 观察 CI_DEEP_CLEAN 新字段。
5. 完成时 board_report 不可用则向父会话发 complete 分段报告；git 红线：不 add/commit/restore。

## 父会话批准（2026-11-14 12:50，session-dad7b6ae…）
计划已批准，三点补充必须遵守：
1. getter 命名按 balance_bridge.h 现有 result_set 前缀风格：`mr_bal_bridge_pressure_*`。
2. main.zm 两口径显示用 `label(s.zh, en, zh)` 双语，与现有深度清理 handler 风格一致。
3. 整轮净值 = activate(104) 进入时 avail_before 到 handler 尾 avail_after（复用现有变量，不新开计时点）。
验收三件套：`zam check` + `zam test` + `build-arch.sh`（在 q12-dof worktree 跑），结果报父会话；不提交（集成由主会话统一做）。

## 卡在哪
- 换气熔断：思考额度耗尽，本轮止步于闸口上报后、动笔前。
- board_report / task_context 看板工具在子代理会话不可用（not found），一切上报走 send_message 到父会话 `session-dad7b6ae-b58c-4e51-af83-3e5b95a4c0a5`。
- 索引 seq 拿不到，账本指针用检索词 `task-mwfx84el S4 计量`，不猜 seq。
