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

## 2026-11-21 凌晨 CI 追凶记录（Production UI marker 挂死）

- 六轮 CI 取证后定位：runner 上生产 GUI 卡在 GTK/glib 初始化（main() 前的
  library ctor），2 线程 glib poll 循环 45s 不动、client.log 0 字节。
- 已修/已试：set -x trace、启动取证（state/wchan/threads）、60s marker 窗口、
  session dbus（dbus-x11 包已加进 p2b 依赖）。最后一轮 trace 输出因
  GitHub API 限流（共享出口 IP 60/h）+ TLS 抖动无法回读——**等网络恢复
  拉 job 136489986506 的日志看 trace 停在哪行即可**。
- 检查命令：gh run view 44794384707 --repo MilchstraBe-Endless10/memreduct-linux-zamak --job 136489986506 --log
- 已确认不受影响：linux 两个 job 持续 PASS（Q17 pulse 测试含在内）、
  GTK Canvas 两个 job PASS。
- PR 分支 pr/q16-scoped-reclaim 当前 HEAD=90ff6b1（trace 版 p2b 脚本）。

## 2026-11-21 04:25 CI 追凶结案（Production UI 两 job）

完整证据链（11 轮 CI、strace、artifact 下载）：

1. 误诊史：以为 GUI 卡死。strace 证明 GUI 活跃：gtk_init 11 秒完成
   （AT-SPI 警告）、X11 请求持续写入（CreatePixmap 批量渲染）、
   glib 主循环 50ms 轮询 X11+dbus fd。
2. 根因一（必然失败）：Q16 引入的 build-production-gui.sh 把
   gui_test.c 替换为 gui_runtime_prod.c，后者 mr_runtime_emit 是空
   stub —— 生产二进制永不 emit CI_PRODUCTION_START，p2b 等 marker
   必超时。main 分支无此替换（直接 zam build），同 job 在 main 是绿的。
3. 根因二（窗口未 map）：GDK_BACKEND=x11 下窗口从未在 X server 出现
   （xdotool 300 秒找不到；fd3 只发请求无事件回流）。本机此前"复现
   成功"实际走 Wayland 后端（桌面 WAYLAND_DISPLAY 泄漏进环境），不是
   真 X11 复现。X11 路径在 Xvfb+openbox 下疑似卡在 CreatePixmap 批量
   后的同步点——待下次会话用 env -i 的 X11-only 环境本机复测。
4. 已修复入库：60s/300s 窗口、dbus-x11 + session bus、strace 取证、
   ctor marker（gui_test.c，CI 二进制不含）、raw write(2) emit、
   ZAMAK_CACHE_DIR 每 run 独立缓存、测试期望更新。
5. 未修（需 Codex 裁决）：生产 stub 吞 marker 与 p2b 等 marker 的设计
   矛盾——p2b 对 production 二进制应等窗口出现而非 marker，或生产
   gate 只替换 mock/特权行为而保留 emit。X11-only 复现步骤：
   env -i DISPLAY=:99 HOME=<隔离目录> GDK_BACKEND=x11 ./binary
   （环境里不得有 WAYLAND_DISPLAY）。
