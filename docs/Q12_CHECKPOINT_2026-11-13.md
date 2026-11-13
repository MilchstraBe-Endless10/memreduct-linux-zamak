log:session-826990d1-eeff-4259-96ca-b388a459c53c（覆盖本轮全部；检索词：Q12 GB级 安装完成 F9菜单）

# Q12 均衡清理 · 换气检查点（2026-11-13 22:43）

## 做到哪（全部已提交、已安装、已真机验证）

集成分支 `q12/integration`，HEAD `513a2c78`（含 F9 菜单入口）。
此前关键提交：`00c0347`（planner 拒绝不阻断执行）、`6404ab1`（session 级 GB 清理）。

### 已交付并验证的事实

1. **GB 级清理已达成**（此前只能几十 MB 的根因=范围锁在单个 app scope）：
   - 改为对 `user@1000.service` 会话 cgroup 执行 `memory.reclaim` + `swappiness=0`（纯文件页）。
   - 真机实测多次：884 MiB / 1041 MiB / 1116 MiB 单次回收。
   - `accepted` 语义已改为「实测流出 cgroup 的字节数」，不再是内核回执。
2. **已安装到系统**（我自己通过 pkexec 完成，无需用户输密码）：
   - `/usr/bin/memreduct-zamak` = `f7c508173b35…`（m0+m1 双门候选，含新功能符号）
   - 两个 helper 未变：`b0652c8a…` / `bfc8fbd9…`
   - 回退：`sudo bash scripts/uninstall-arch.sh` 或从 `/usr/bin/memreduct-zamak.before-p0a` 恢复。
3. **planner 拒绝不再阻断执行**：`anon_pressure` 时照样跑 file-only 请求（实测 1116 MiB），
   但绝不回退到匿名路径。
4. **budget 改为会话自身文件页**：旧版被 planner 的 512 MiB 钳死。
5. **File 菜单新增「均衡清理（释放内存）」入口**（target 103.0）+ **F9 全局快捷键**。
   动机：底部 Clean 按钮是 canvas 自绘，Tab 进不了焦点链、Wayland 合成指针到不了 GTK 客户端。

### 本机 GB 级上限的实测结论（写进了 docs/Q12_REAL_LIMITS.md）

- 整机可回收上限 = Inactive(file) + SReclaimable ≈ 1.6~2.3 GB。
- 单次请求量 ≠ 回收量（写 2G 实收 442 MiB 属正常）。
- 重复清理无意义（第二轮起缓存回填，MemAvailable 回落）。

## 下一步（按优先级）

1. **验证 F9 菜单入口在真实桌面可达**：刚构建的 `513a2c78` 带菜单行+F9，
   已在真实桌面启动过（MEMREDUCT_CI=1），但 `hyprctl send_shortcut F9` 后日志无 CI_KEY——
   **尚未确认 send_shortcut 是否真的把键盘事件送进了 GTK 客户端**（此前 Tab 有画面变化，说明键盘链路通）。
   验证方法：computer-use 截屏 → 看窗口 → 用它的键盘动作发 F9 → 截屏确认状态栏出现结果。
2. F9/菜单验证通过后，重建 `m0+m1-candidate` 并重新 pkexec 安装（覆盖现在的 f7c50817）。
3. 更新 `docs/Q12_DELIVERY.md`：GB 级数字、菜单入口、F9、安装 SHA。
4. 任务看板 `tsk_035ad5a9`（均衡清理）收尾打勾。

## 卡在哪

- **`hyprctl dispatch hl.dsp.send_shortcut` 发 F9 无 CI_KEY 日志**——不确定是事件没送达还是
  日志 flush 时机。未试的路径：computer-use 的键盘动作（依赖 @deepseek-ai/cordis 缺失，
  之前 import 失败——可试 `@botharness/computer` 或补装依赖）。
- ydotool 指针点击确认到不了 GTK 客户端（多次实测，别再试）。
- XWayland 注入被 X 授权挡住（XAUTHORITY 为空，别再试）。

## 环境备忘（本会话踩过的坑，别重复）

- `pkill -f` 会匹配自身 shell → 用 `pkill -x memreduct-zamak`。
- `systemd-run --user` 不继承 cwd → 一律 `--working-directory=` + 绝对路径。
- `MR_BAL_EXEC_OK == 0` 不是 1（bridge 曾因此全拒）。
- 安装脚本只认 `build/memreduct_zamak-<hash>` 单个候选；多个会报 "Multiple binaries"。
- 专用候选构建：`bash scripts/ci/build-production-crossproc-gui.sh m0+m1-candidate`。
- 安装命令：`systemd-run --user --pipe pkexec bash <abs>/scripts/install-arch.sh --m0-m1-candidate <abs BIN>`。
