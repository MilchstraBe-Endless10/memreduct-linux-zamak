# SHORT_TERM

当前阶段的短期工作记录。

本项目的历史开发笔记不随功能变更进入仓库；与均衡清理相关的证据和结论见
`docs/PR_DRAFT_Q16_BALANCED_CLEAN.md`。

## 2026-11-21 Q17 高压回收（feat/q17-high-pressure-reclaim）

- ADR-0009 立项 + 两轮真机实测（98557e1, 7457b78）：memory.reclaim 在持续
  高压机上净收益为负；per-scope memory.high 脉冲瞬时 +1.06 GiB（PSI 3.09），
  1 分钟内回落（机器满载所致）。价值主张定为"为前台争取呼吸空间"。
- 用户四条硬规（2026-11-20 23:52 口头确立，已写入代码契约）：
  ①不伤害任何进程（只写 memory.high，无信号/kill/冻结）
  ②不准卡死进程（下限=占用 40%，脉冲 ≤10s，PSI 每步检查）
  ③清理中软件不能卡（PSI 上涨即提前恢复）
  ④清理后软件必须可用（写回+回读验证；watchdog 15s 兜底强恢复）
- balance_pulse.c/.h 已实现并通过 gcc -Wall -Wextra -Werror -pedantic；
  tests/test_balance_pulse.py 7 项（fake cgroup，不碰真实 scope）；
  真机驱动验证 Chromium scope：restored=1、high 回 max、进程存活。
- zam.toml sources 已加 balance_pulse.c；verify_project PASS；zam check OK。
- 整套 discover OK（skipped=5, expected failures=1）。
- 下一步：GUI 菜单接线（Deep clean 下新增 pulse 档）、CI 挂 pulse 测试。
