# 短期记忆 / Latest Handoff

## 2026-11-05 — M1.3 目标绑定修复与 M0+M1 本地候选

- **代码提交：**本地分支 `codex/m13-fault-repair`，提交 `247b51465a9a067b269d595741cfa5cd29ff2ca6`，Tree `54be50458f111cfd5df55355367dab0d33d3def9`，父 `425d87df41364d73627bccd8dac8c1db707da8c4`。本提交加入单次预算选择、Space 键盘处理及 Xvfb 回归；父提交 `425d87d` 实现目标快照绑定。文档与记忆分开提交。
- **重要安全背景：**所有者报告此前一次 M1.3 内核调用接受了 16 MiB 建议，但实际 PID 与那次授权的 PID 不一致。该次结果按目标身份完整性失败记录，不能算日常清理成功；本轮没有重试该目标或任何其他目标。
- **代码修复：**选中候选时固定 PID、start_time、UID、comm/exe/cmdline/cgroup 指纹以及映射摘要；重扫排序不会更换目标，发起授权前及 helper 内再次独立核验。目标退出或身份、scope、映射变化即拒绝，不回退到同名进程。每次操作可选 16/32/64 MiB，默认 16 MiB；helper 仍强制 64 MiB 上限，实际建议量取预算与复核映射量较小值；新目标重置预算，单次调用，不自动重试。
- **隔离交互证据：**CI GUI 的 Xvfb 测试覆盖鼠标 64 MiB、键盘 32 MiB、确认页取消及 mock 部分结果；输出中 `requested_bytes=33554432`、`advised_bytes=16777216`、`mock=true`、`call_count=0`，没有 helper 或 syscall。截图及哈希在 AI 产出目录 `报告与研究/MemReduct-M1-3-2026-11-05/continuation-2026-11-05-02/xvfb-budget-ui-r3/`。本轮的生产候选在隔离 Xvfb/X11、`MEMREDUCT_CI`/`MEMREDUCT_TEST` 未设置时打开可见窗口，按 GTK 关闭按钮退出 0，日志为空；没有触发清理。这些不是生产 Polkit/真实回收证据。
- **验证：**治理 34 项 PASS；Python/C 安全测试 203 项执行，202 PASS、1 项因文件系统拒绝超长路径而 skip；所有 `scripts/*.sh` 与 `scripts/ci/*.sh` 语法通过；Zamak 0.2.0 check/build 通过、test 81/81。候选只读安装预检通过，`ldd` 无缺失库。M0+M1 GUI 候选二进制 SHA-256 `55a533c028e0faf845df074cb01f13c45889bab4df4d5feb076a9309208b4e34`；page-cache helper `b0652c8ac3ae70576e39b71fbdcd047a7c57f23873fec2e45b1b691b34c45a5e`，Crossproc helper `03044920f438892e3279f276921d673f5a2cb996699e8b599acafb0a8997867a`。完整新候选、组件 SHA、日志及生产 GUI smoke 结果在 AI 产出目录 `报告与研究/MemReduct-M1-3-2026-11-05/continuation-2026-11-05-03/m0-m1-candidate-final/`。
- **只读安装状态：**主机 page-cache helper 是 root:root 0755，SHA 与候选相同；两份策略为 root:root 0644、SHA 与候选相同，两个 Polkit action 均被识别。但主机现有 Crossproc helper 虽为 root:root 0755，其 SHA `ae7fecede5dd7230e4fc6769f3b408ba7fb4a7f95088ed6a026d2f6d5f787339` 与候选 `03044920f438892e3279f276921d673f5a2cb996699e8b599acafb0a8997867a` 不同。不要用新 M0+M1 GUI 调用这份旧 helper；需另行授权更新并验证。此次没有写系统文件。
- **边界与完成度：**这不是安装后产品验收。此次没有启动当前版本 Hyprland GUI，没有运行 Polkit、helper、page-cache 写入或 `MADV_PAGEOUT`，没有安装/卸载，也没有 GitHub push。整页 Windows 3.5.2 1:1 仍 **NOT PASS**；新修复尚缺一轮目标身份一致的生产端到端清理验收。

## 历史快照（2026-11-05 — M1.3 新隔离 Pinta 目标准备）

### 目标准备历史快照（身份不一致事件之前）

- 在新的唯一用户 scope 下启动了空白 Pinta，使用 AI 输出目录内独立 XDG profile，没有传入文件路径。只读核验确认命令行为 dotnet + Pinta.dll、UID 四元组一致、启动时间及专属 scope 与预期相符；目标身份和当时的 smaps/maps 观察保存在私有文件 `报告与研究/MemReduct-M1-3-2026-11-05/pkcheck-diagnostic/test-target-r1/target-candidate.md`，避免把本机 PID 写入公开仓库记忆。
- 只读内存映射统计不代表可回收量；拟申请的操作上限为 16 MiB。未启动 Mem Reduct GUI、pkexec、Crossproc helper 或任何回收 syscall；旧 Pinta 实例未触碰。
- 该新目标的 PID/start_time 一次性回收授权尚未取得。执行前必须重新核验全部身份字段；任何不匹配或退出都停止，不换目标。当前等待所有者对该具体实例批准一次调用。

## 2026-11-05 — 专属 Crossproc action 的 pkcheck 交互授权通过

- **授权和 action：**所有者批准一次仅授权检查。已核实 action 为 io.github.milchstrasse_endless10.memreduct.zamak.crossproc，活动会话策略为 auth_admin，固定 executable 路径为 /usr/libexec/memreduct-zamak-crossproc-helper。
- **执行结果：**第一次将 --process 与 PID 写在同一个参数中的调用，被本地 pkcheck 参数解析拒绝（126），没有联系 Polkit。按 pkcheck 语法更正后，使用 --allow-user-interaction 的单次检查返回 0，输出 polkit.result=auth_admin。Hyprland Polkit agent 在对应时间配置了认证窗口；systemd 的 Polkit agent helper unit 随后以 Result=success、ExecMainStatus=0 结束。pkcheck --list-temp 没有列出此 action 的临时授权。综合 action 的 auth_admin（非 keep）策略、交互 helper 成功及 pkcheck 0，证据支持这次是新的交互认证并已被 Polkit 接受。
- **PAM 证据边界：**该时间范围没有 pam_unix 成功/失败文本日志；系统只记录 PAM helper 服务成功退出。故记录为“交互授权成功，未见 PAM 模块级成功行”，不伪造 pam_unix 日志。
- **安全边界：**pkcheck 只查询当前 shell 对该 action 的授权，没有启动 Crossproc helper、pkexec 或对任何目标进程操作；process_madvise/MADV_PAGEOUT 调用 0。没有修改 PAM、Polkit、helper、系统配置、Pinta 或其他桌面进程。原 Pinta 一次性回收授权仍已耗尽。

## 2026-11-04 — 获批的 pkexec 无副作用诊断结果

- **授权操作：**所有者单独批准一次 pkexec /usr/bin/true，目的为检查 Polkit 认证通道；严格未运行 Mem Reduct helper、未操作 Pinta、未执行内存 syscall 或系统写入。
- **结果：**命令在 Wayland + session D-Bus 环境中返回 0。系统日志于 23:10:22 记录 pkexec 以 root 打开会话并执行固定的 /usr/bin/true。该时段没有新的 Polkit PAM helper 或 PAM 认证日志，因此命令很可能使用已有授权/缓存，未触发新的认证挑战。
- **结论边界：**这证明当前 Polkit 可以执行该固定 root no-op，但不证明新的密码认证对话成功，也不证明专属 crossproc action 的 PAM 问题已修复。没有重试已耗尽的 Pinta 授权；后续若需强制新认证挑战，必须另行批准具体的只读授权检查。

## 2026-11-04 — M1.3 认证诊断、清理路由与 143 归因修正

- **本地代码提交：**`3dd412bd8bb60185b132f6fef89e946d9bd8f1d1`，基于 `9ff3ccfe3df950c1b917934f8cf38d0bfc848091`，分支 `codex/m13-fault-repair`。仅本地提交；未推送、未改变 PR 状态，也未触碰其他 worktree。
- **Polkit 失败层级：**本机 system journal 对应的一次性 Pinta 请求记录了正确 action `io.github.milchstrasse_endless10.memreduct.zamak.crossproc`、调用 UID 1000 和 `unix-session:2`；`hyprpolkitagent.service` 在该时段运行并启动了认证 helper。没有发现 `No session for cookie`。随后 `pam_unix(polkit-1:auth)` 报 `conversation failed`、`auth could not identify password`，helper 又记 `pam_authenticate failed: Authentication token manipulation error`。证据把失败定位到 PAM 凭据对话，没有证明是用户取消、错误输入还是 agent 未把输入交给 PAM；未修改 PAM、Polkit、认证代理或桌面配置。该 Pinta 单次授权已经使用，不得重试；本轮 helper 和 `process_madvise` 调用均为 0。
- **源代码修复：**Clean 按钮正文和箭头现在都只打开动作菜单，默认不触发 M0；只有明确选页缓存才进入页缓存确认，选择“用户应用回收”才扫描并打开跨进程候选选择器。异步跨进程桥现保留并清除/设置 pkexec 子进程退出码及信号，GUI 区分 126 授权取消、127 授权/启动失败及信号导致的结果不确定；测试断言退出状态传递。它不能替代 PAM 修复或新的认证实测。
- **143 来源：**真实 Wayland 采集脚本 `capture_harness_r3.py` 的 `finally` 在 GUI 仍运行时调用 `child.terminate()`；这能使 GUI 以 SIGTERM 结束，但现有 artifact 没有记录 controller 为何进入清理段，因此原实机 143 的触发原因仍未证实。P3B/P3C Xvfb harness 已改为记录外部 SIGTERM、清理信号及子进程 wait status，并将成功用例改为 GTK 窗口关闭。一次错误使用 production 二进制的门禁测试未产生 CI 标记而失败，测试 harness 进入清理；改用测试二进制后通过，GUI 记录 GTK close/exit 0，无 SIGTERM。
- **验证：**治理 34 项 PASS；Python/C 192 项执行，191 PASS、1 项受文件系统路径长度限制跳过；Shell 20 个脚本语法 PASS；Zamak 0.2.0 `check` PASS、`test` 81/81 PASS、`build` PASS；CI/生产 GUI 与现有 C helper 构建 PASS；`git diff --check` PASS。Xvfb P3C 菜单→明确页缓存确认取消、菜单→用户进程选择→取消 PASS；真实 pkexec/helper/syscall 均为 0，另一个隔离 mock 结果用例实际运行测试 helper，未执行 syscall，GUI exit 0。P3B 模拟闭环 PASS（3,145 个 changed pixels，mock helper，GUI exit 0）；默认门禁 PASS（显式页缓存选择后仍禁用，无确认/无真实 helper，GTK close exit 0）。日志与截图位于 AI 输出目录 `报告与研究/MemReduct-M1-3-2026-11-04/continuation-2026-11-04-05/fault-repair/`。
- **本轮边界：**没有在 Hyprland 活动桌面重试认证，没有重用已耗尽的一次性授权；没有运行 `MADV_PAGEOUT`、helper、M0 页缓存清理或任何其他特权/内核操作。真实认证是否可用仍待单独的全新认证授权及结果；M1.3 日常应用回收未完成。

## 2026-11-04 — M1.3 production Wayland Pinta candidate selection and cancel

- **Real target instance:** with owner approval, launched an empty Pinta instance in an isolated XDG profile and user app scope. PID `1874718`; UID real/effective/saved/fs all `1000`; `/proc` start_time `41781042` ticks; exe `/usr/share/dotnet/dotnet`; command `/usr/bin/dotnet /usr/lib/pinta/Pinta.dll`; cgroup `/user.slice/user-1000.slice/user@1000.service/app.slice/app-pinta-m13-test-r2.scope`.
- **Natural candidate scanner:** production C scanner returned success after 512 proc entries, 32 candidates, and found PID `1874718` at candidate index 22. It had 790 private regions; the pre-GUI mapping-length sum was `704548864` bytes. The live picker later displayed `704925696` bytes as a candidate mapping estimate; this changed with the mapping snapshot and is not expected reclaimed memory. The scanner also reported `candidate_limit=18`, so the 32-row cap is now visible instead of silent.
- **Normal Wayland GUI:** built from source commit `4929faebeadded436eca6bb303b7696b3c4dac3d`, profile `m0+m1-candidate`, binary SHA-256 `6eff5b252237ed346f3b6e77907a9393323ee46f39d246bd4bdee3888176bbd9`. Runtime was UID 1000, `XDG_SESSION_TYPE=wayland`, `WAYLAND_DISPLAY=wayland-1`, `GDK_BACKEND=wayland`; `MEMREDUCT_CI` and `MEMREDUCT_TEST` were absent. The picker showed `dotnet` PID/UID, and 23 Down events selected index 22. Continue opened the risk page with the 16 MiB ceiling and PAGEOUT swap/I/O/latency warning.
- **Cancel result:** Cancel returned the application to the result page stating “Confirmation cancelled; no request was started”; advised bytes 0, requested bytes 0, regions 0, helper calls 0. “Authorize and run” was never activated; no Polkit dialog or helper started. This confirms the M1.3 selection→risk→cancel path only, not a daily-app reclaim.
- **Harness failure retained:** the first untethered controller run ended with exit status 143 after the picker capture and before risk-page confirmation; the cause was not established. The target did not receive an authorization action. The rerun in a named user scope completed selection, risk, cancel and close.
- **Close:** the Mem Reduct test window received the targeted GTK close callback and exited 0. The Pinta test scope was then stopped with SIGTERM; PID `1874718` disappeared and both test scopes reported inactive/dead. Its isolated temporary profile remains outside the repository and was not removed.
- **Evidence:** report and SHA-256 manifest are under `报告与研究/MemReduct-M1-3-2026-11-04/continuation-2026-11-04-02/m0-m1-candidate/real-wayland/` within the configured AI output root; captures include `main.png`, `picker-target.png`, `risk-page.png`, and `picker-cancelled.png`. A temporary GTK test module injected events only into the exact Mem Reduct window and captured its Wayland buffer; no desktop-wide input injection was used.
- **Boundary:** no helper invocation, Polkit request, `process_madvise`/`MADV_PAGEOUT`, page-cache operation, Swap/ZRAM change, or operation on another process. The 1:1 Windows visual target remains NOT PASS. PR status was not changed.

## 2026-11-04 earlier snapshot — M1.3 Wayland cancel/close evidence and scanner limit diagnosis (before Pinta rerun)

- **Wayland interaction (continuation evidence):** the normal production GUI opened its natural `/proc` candidate picker, a row was selected, the risk page was shown, and Cancel returned without a request. Helper starts and advised bytes remained zero. The exact test window was subsequently closed through the approved Hyprland compositor path and exited with status 0. This verifies selection/risk/cancel and window-manager close only; it does not verify page-cache cleaning, Polkit, or `MADV_PAGEOUT`.
- **Target status:** the earlier temporary Pinta process PID `1473854` is no longer present. No current `dotnet`/Pinta target was found by the read-only production scanner. A fixture using `/usr/share/dotnet/dotnet` in `user@1000.service/app.slice/app-pinta-m13-test.scope` is eligible under the current rules, so the historical omission cannot be attributed to executable or cgroup protection. Because the old PID exited, whether it was beyond the 32-row cap or rejected by a transient procfs read remains unproven. No replacement GUI was launched and no target-specific authorization was requested or used.
- **Scanner diagnostic fix:** the candidate scanner declared `MR_CAND_SKIP_LIMIT` but did not store or expose the count when more than 32 otherwise eligible rows were encountered. `native/crossproc_candidates.{c,h}` now records and returns that count; eligibility, scanning bounds, and helper behavior are unchanged. Regression coverage includes 33 fixture candidates (32 offered, one reported as limit-skipped) and the Pinta-shaped dotnet app scope.
- **Read-only live scan:** the just-built production scanner completed 512 proc entries with 29 candidates and skip counts: self 1, other UID 352, unreadable 7, protected/non-app 121, kernel thread 0, ancestor 0, no eligible regions 0, zombie 1, candidate-list limit 0. These are current host observations; they do not explain the earlier Pinta PID.
- **Verification on local source:** project governance 34 PASS; Python/C suite 192 passed, 1 existing path-length skip; Zamak 0.2.0 `check` PASS, 80/80 tests, build PASS; production cross-process GUI build and test-hook exclusion PASS; shell syntax and `git diff --check` PASS. Build outputs were confined to an isolated temporary directory and removed with it.
- **Git / boundary:** base source HEAD remains `681e03bb3390e1a3ce953615b10eb73c67d454ce`. The candidate-limit source/test change and this handoff note are local only. No push, PR state change, Polkit request, helper invocation, page-cache operation, `process_madvise`, Swap/ZRAM modification, or daily-app reclaim occurred. M1.3 daily-app reclaim remains incomplete.

## 2026-11-03 — M0-preserving production profiles and M1.3 manual GUI gate

- **Build profiles:** reused `build-production-clean-gui.sh` as `m0-installed-release` (page-cache=1, cross-process=0); extended the existing `build-production-crossproc-gui.sh` with the optional `m0+m1-candidate` profile while keeping its no-argument `m1-opt-in-candidate` behavior. No third duplicate build flow was added.
- **ELF gate matrix, checked from actual x86-64 machine code:** `safe-default` 0/0; `m0-installed-release` 1/0; `m1-opt-in-candidate` 0/1; `m0+m1-candidate` 1/1 (page-cache/cross-process). All production candidates use inert runtime hooks and omit test/fake authorization entry points. `m0+m1-candidate` SHA-256 is `34ede383a2f81fe8507e620c208ccf2e90baa15de684fc8f9d7a9fa4efa85f18`; `m0-installed-release` is `0c193a1161fd0a2084ff08473f5c9177bfab413786bdc20cec7ac5eefe95795b`.
- **Installed M0 was preserved and checked read-only:** `/usr/bin/memreduct-zamak` remained SHA `83af5a9caac1814046309cdae962ad6eb6072a2588675f00f4a5375d256c537d`, with page-cache gate returning 1. The installed M0 helper is root-owned mode 0755; its SHA `b0652c8ac3ae70576e39b71fbdcd047a7c57f23873fec2e45b1b691b34c45a5e` matches the locally built helper. The root-owned policy is mode 0644, its fixed helper path matches, and Polkit recognizes the `.pagecache` action. The cross-process helper/policy are likewise present root-owned and Polkit recognizes the `.crossproc` action. No file was installed or changed.
- **Hyprland GUI:** production selection/risk/cancel is still awaiting manual interaction. This Codex session has no usable native window-control surface; the existing user Hyprland session was not blindly targeted. The prepared `m0+m1-candidate` has no `MEMREDUCT_CI` hooks and is suitable for the requested no-reclaim UI check. No page-cache action, Polkit request, helper launch, or `MADV_PAGEOUT` occurred in this pass.
- **Artifacts:** the safe-default and M1-only hashes remain as recorded in the following entry; all new build outputs are in the configured external AI output folder. No Git push/merge/release was performed.

## 2026-11-03 — M1.3 production build isolation recheck

- **Normal production build:** `scripts/ci/build-production-gui.sh` passed governance (34 checks), shell syntax (20 scripts), Python safety (190 passed / 1 filesystem path-length skip), Zamak 0.2.0 check, Zamak GUI tests (80/80), CI GUI build, and production GUI build. It builds the CI executable separately as `memreduct-zamak-ci-test`; production uses inert runtime hooks and production-only async bridge.
- **ELF gate evidence:** default production candidate `memreduct-zamak-production` has `mr_xproc_capability_enabled() == 0` and `mr_production_clean_enabled() == 0`; the separately built `memreduct-zamak-production-crossproc-candidate` has cross-process gate `1` while page-cache gate remains `0`. The opt-in candidate also has no CI/test environment strings, fixture symbols, or mock authorization entry point. Neither candidate was run against a target or invoked Polkit.
- **Artifacts:** build logs, manifests, symbols and binaries were kept in the configured external AI output folder. SHA-256: default production `bb6c98c20502f6b20fdb04fe618db20349ddabce276ad3d6c1a05cf61544e04d`; CI test `82d32568716b0932201875c9f75c367798e27f87f229c90b73959841bd320f94`; cross-process opt-in `c519d79ff6878580155267721c41f4e2a4579d27a5cdf5244b445f53cd4f8ec1`.
- **Real Hyprland interaction:** `XDG_SESSION_TYPE=wayland`, `WAYLAND_DISPLAY=wayland-1`, and a Hyprland process were observed. No native app/window control surface was available in this Codex session, so the production GUI was not launched or interactively inspected; do not call this a Hyprland acceptance. The live picker, risk prompt, and cancellation acceptance remain pending. No Polkit request, helper invocation, `MADV_PAGEOUT`, or M0 operation occurred.
- **Git:** this worktree remains on `feat/issue-12-m1-3-manual-reclaim`; no remote action was performed. Windows 3.5.2 whole-page 1:1 fidelity remains NOT PASS.

## 2026-11-03 — M1.2 owner-reported success; M1.3 isolated GUI simulation verified

- **M1.2 evidence:** Per owner-published [Issue #12 record](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/12#issuecomment-6599614790), one authenticated `process_madvise(MADV_PAGEOUT)` call advised 1 MiB on the project's temporary child; residency changed 256/256 → 0/256 pages; child stayed alive and data integrity passed; temporary resources were removed. This is not a net-RAM-freed measurement. The one-call authorization is consumed; this worktree did not repeat it.
- **M1.3 code:** branch `feat/issue-12-m1-3-manual-reclaim`; base implementation `3bd2a7f1b3bcb975bac3d31302fc146e61475bee`, followed by honest simulation labels `e32c2ba3b802a6b90c696a2af127d04a5fcf033e` and the GTK titlebar lifecycle fix `74574a93c4059946ddc446e7787efa0a8a0e9adc`. The picker remains read-only, starts with no target selected, limits the request to one eligible same-user app, confirms swap/I/O/latency risks, invokes only the fixed helper bridge, and separates advised bytes from observed `MemAvailable`.
- **Isolated GUI evidence:** `scripts/ci/p3c-crossproc-simulation-xvfb.sh` passed against the opt-in candidate in a private Xvfb display. It used two fixed fixtures only: mouse selection, confirmation cancellation with zero starts, keyboard selection, Tab/Enter confirmation, mock authorization/helper stages, partial-result feedback, Escape and normal titlebar close. Process exited 0; the mock reported 2 MiB advised of a 4 MiB request, with `call_count=0`, `no_syscall=true`, and `no_system_write=true`. Host `MemAvailable` was sampled before/after only as an observation. No real procfs process list was read.
- **Live picker / scanner preflight (2026-11-03):** A temporary non-root harness scanned fixed `/proc`: status 0, 512 entries scanned, 32 eligible rows at the configured bound; the opt-in bridge reported gate 1 and helper preflight 0. The existing opt-in GUI binary (`c8777d78f86899e27f338a7df0c750a6d6d9a5173a33080a5270a4d23bf53bb3`) was then run in private Xvfb with `MEMREDUCT_CI=1` and live enumeration (simulation unset). The picker opened with 32 rows, one row was selected and canceled with Escape, and the app closed without GTK warnings. No confirmation was accepted; no helper, Polkit, or syscall started. PID/name logs and screenshots were not retained. This verifies live picker selection/cancel in Xvfb, not real Hyprland interaction or everyday-app reclaim.
- **Defects found and fixed during that run:** simulation output previously said “Kernel accepted”; it now names the test helper and labels mock byte/call counts. GTK emitted `gtk_window_set_titlebar() called on a realized window`; the bridge now unrealizes the still-unmapped top-level, attaches the header bar, and realizes it again. The Xvfb test now fails on GTK/GDK/GLib warnings or criticals; the post-fix run emitted none.
- **Verification:** governance 34/34; Python 190 passed / 1 existing filesystem-long-path skip; Zamak 0.2.0 `check` PASS, tests 80/80, build PASS; 20 shell scripts parse; default GUI and cross-process opt-in candidate build PASS; production helper build and `git diff --check` PASS. Build/test logs and five PNG captures are under `/mnt/Server_Data_Center/Personal_Files(个人文件)/Cosmos_File/AI产出数据文件/报告与研究/MemReduct-M1-3-2026-11-03/continuation-2026-11-03-01/`.
- **Geometry caveat:** the same Xvfb log still reports the main Canvas at `327×359` against the current provisional `337×369` request (`delta=-10×-10`). This was not corrected or accepted as a visual pass; full-page Windows fidelity remains NOT PASS.
- **Build boundary:** the default GUI keeps both privileged gates disabled. The opt-in review candidate enables only cross-process reclaim; page-cache cleaning remains disabled. Current binary SHA-256: default `e942da128fc40ab0e0e2320cf3254ca061e58a147c146e9a723c63c8f876a1f3`; opt-in `c8777d78f86899e27f338a7df0c750a6d6d9a5173a33080a5270a4d23bf53bb3`.
- **Not run / remaining:** no Polkit request, real helper invocation, target-specific daily-app syscall, system write, real Hyprland input or Weston run. The live picker selection test ran only in Xvfb; no target was confirmed for action. M1.3 remains incomplete until real Hyprland UI review and separate authorization naming one everyday-app target for one real operation. ADR-0007 remains PROPOSED, and Windows 3.5.2 full-page pixel identity remains NOT PASS. No remote push or PR change.
- The Issue #12 `SystemExit(0)` orchestration misreport source was not found in this project worktree or the inspected `p3c-integrate` worktree; no speculative replacement was added and no real syscall was repeated.

## 2026-11-02 — Issue #12 M1.1 安全审查修正（本地提交，未安装）

- **代码提交：**`8f236dcd77325c171ff2e76771e1f948710d4ce6`，Tree `ba31f0fc6910284fb3ffdb897bc7fc75f7353b36`，父 `1c4565f6e5f4808fe4f2cf376b0508ac93dcc416`。只在隔离分支 `feat/issue-12-crossproc-helper` 本地提交；未 push、未 merge。
- **凭据修正：**pkexec root-target helper 按 `ruid=0/euid=0` 接受，不再要求两者不同；继续要求有效 `CAP_SYS_NICE`，并严格解析非零 `PKEXEC_UID`。该值仅表示请求者声明；目标 `/proc/<pid>/status` 的 real/effective/saved/fs UID 必须全部一致、非 root 且等于该声明。
- **目标保护：**候选扫描与 root helper 共享截断感知名单；必须成功读取完整 `/proc/<pid>/exe`、匹配用户 UID、用户应用 cgroup、PID/start_time 和 pidfd 身份，保护进程及未知/不可验证状态一律不提供或拒绝。
- **安装校验：**Arch 安装脚本验证专属 action ID 与固定 `exec.path`，安装后比对 policy 与已校验源文件、复查绑定并要求 `pkaction --action-id` 能识别该 action。脚本未实际运行；本机当前无已加载的 cross-process action。
- **回归：**治理 34 项；Python 186 passed / 1 个既有超长路径用例按环境限制 skip；Zamak 0.2.0 check、76/76 test、build；`scripts/build-arch.sh`、所有 Shell 语法及 `git diff --check` 均 PASS。候选夹具重复 PID 的 symlink 错误已修；完整套件发现并修正安装文件清单把目录误作文件的问题。
- **安全边界：**没有安装 helper/Policy，没有执行真实 pkexec、`process_madvise` 或 `MADV_PAGEOUT`，也没有推送。

## 2026-11-02 — M1.1 初始实现历史快照（安全复审修正前）

- **分支与提交：** 新工作树 `/home/arlune_innersea/.codex/worktrees/p3c-crossproc-helper`，分支 `feat/issue-12-crossproc-helper`，基点 `p3c/integration` @ `57dc1fa`。本地提交 `77f21f5`（功能）、`f7b4172`（两处实跑发现的缺陷修复）。**未 push、未 merge、未安装、未跑任何真实跨进程 syscall。**
- **新建的生产代码：** `native/crossproc_helper.c/.h`（唯一特权动作：一个动词 + 三个十进制数 + 一次 `process_madvise`，所有身份判断由内核重新推导）、`native/crossproc_candidates.c/.h`（只读枚举用户自己的进程）、`native/async_crossproc.c/.h`（GUI 侧唯一 pkexec 调用者，字面量 argv）、`production_crossproc_gate_{default,enabled}.c`（与 page-cache 门控**独立**）、`packaging/polkit-1/actions/...crossproc.policy`（action id 与 exec.path 均与 page-cache 完全分离，`auth_admin` 每次，无 `*_keep`）。
- **实跑发现的两个真实缺陷（均已修复并回归）：**
  1. **comm 截断绕过保护名单。** 内核把 `/proc/<pid>/stat` 的 comm 截到 15 字节，本机 pid 1188 的 `/usr/bin/dbus-broker-launch` 报成 `dbus-broker-lau`，精确比较直接放行了一个桌面服务。现在：匹配对 15 字节截断敏感 + 显式列出长名 + 用未截断的 `/proc/<pid>/exe` basename 作第二来源；helper 与枚举用同一规则，有测试断言两份名单与规则不得漂移。
  2. **拒绝可能报成"没有拒绝"。** `mr_xproc_start()` 入口清空 `local_refusal`，但某条拒绝路径在赋值前就返回，调用方读到 `0`（即 `MR_XPROC_LOCAL_NONE`）。现在每条路径都设唯一原因，并新增 `mr_xproc_local_refusal_name()` 稳定 token。
- **设计修正：** `mr_xproc_test_classify()` 原先在所有构建里导出，GUI 可调用它来决定"特权侧说了什么"。现在只在 `MEMREDUCT_TESTING` 下编译，并有符号扫描测试断言生产 ELF 里不存在任何 `mr_xproc_test_*` / `mr_cand_test_*`。
- **GUI：** `run_self_reclaim` 通过窗口用的同一条标量桥做只读枚举，并发出逐行 CI 标记（带 helper 将要核验的 start_time）。仍然只读：无 pidfd、无 advice。
- **本机实测回归：** Python 167 项（1 项因 >PATH_MAX 被文件系统拒绝而 SKIP）、Zamak 76/76、`zam check .` CHECK OK、GUI 构建 PASS、全部 shell 语法 PASS。生产二进制内无 `MEMREDUCT_TEST_*` 开关、无测试符号。默认门控实测 `mr_xproc_capability_enabled()=0`，enabled 构建为 `1`。
- **真实枚举（本机）：** 扫描 512 条 procfs 条目，32 个候选，跳过其他 UID 339、受保护 7、无空地址空间 3；候选含 pipewire / wireplumber / gnome-keyring-daemon / sh 等。
- **仍未完成（需用户单独批准）：** ① 把 root 属主的 crossproc helper 与 Polkit action 装到本机；② 一次交互认证的真实 `MADV_PAGEOUT`。上次那份批准已用掉，不复用。M1.2 / M1.3 均未宣称完成。
- **产物：** `progress/CHECKPOINT-crossproc-helper.md` 保留原始取证；`docs/ADR/0007-*.md` 已追加 M1.1 修订段。

## 2026-10-31 — P3B 第二轮：授权语义修复

- **提交：**`48522c2`（授权归因修复），前一轮 `676d73f`。分支 `feat/issue-10-p3b-clean-closure`，未 push。
- **修的缺陷：**pkexec 会转发子进程退出码，126/127 既可能是 pkexec 自身的取消/授权失败，也可能是子进程以相同数值退出。桥上原本把 126 直接映射成 `AUTH_CANCELLED`、127 映射成 `AUTH_OR_PKEXEC_FAILED`，GUI 显示成“已取消授权”——这是在断言无法证明的原因。现在：无子进程自报证据时 126/127 → `OUTCOME_UNCERTAIN`；有证据时 → `HELPER_FAILED`。不解析 pkexec 的 stderr（那是不可信输入）。GUI 状态 6 改名为 `authorization-unconfirmed`，文案改为“授权或执行结果未确认”。`mr_reclaim_child_reported_failure()` 是记录子进程自述的通道。
- **生产警告文案：**不再复用模拟句子，改为 `仅丢弃干净页缓存。` / `文件将变慢重读；不保证释放量。`
- **无法完成项（工具限制，已如实记录）：**Issue 要求渲染生产警告，但 `zam` 不支持 `-D`、`zam.toml` 无 defines 段，且构建缓存按源码内容键控——PATH 上的 cc 包装器只在缓存探测时被调用一次，真正编译复用缓存。证据：用包装器构建出的二进制与不用的 **sha256 完全相同**（`16ffda1f…`）。宏本身在源码层有效（直接编译该 .c：无宏返回 0，带宏返回 1）。曾写过一个包装 cc 的脚本，发现它会产出误导性的成功构建，已删除而非保留。**渲染后的生产字符串 = NOT RUN。**
- **回归：**Python 66 passed；`zam test .` 76/0 失败；`zam check .` OK；shell 语法干净。变异验证：把归因改回“信任数字”会有 4 条断言失败。
- **方案（只准备未执行）：**`docs/REAL_DEVICE_CLEAN_TEST_PLAN.md` —— 含命令、每步预期输出、成本、回滚、以及该方案不授权的范围。未在任何设备上执行。
- **需要用户授权：**①指名设备与动作的真实设备测试；②一个能定义 `MEMREDUCT_ENABLE_PRODUCTION_CLEAN` 的构建路径；③push / PR Ready / Merge。

## 2026-10-31 — P3B 生产 GUI 与隔离清理闭环

- **分支与提交：**`feat/issue-10-p3b-clean-closure`，起点 `d8ca307090782182355892d2777a44c1ddac68fa`。本轮本地提交依次为 `71621ac`（Polkit action 固定 helper 路径）、`1f8434f`（CI 标记改走原生无缓冲输出）、`db7cf23`（隔离清理闭环脚本）、`b3c8936`（默认构建拒绝清理的门控脚本）、`07dd2a4`（ADR/报告/记忆）、`ace6396`（确认框文字截断修复）。无 push / Ready / merge / release。
- **修复的缺陷（均有回归或实跑证据）：**①`src/main.zm` 全部标记原走 Zamak 运行时缓冲 `println`，进程被限时定时器击杀时整批丢失 —— 新增原生 `mr_ci_emit`（写 stderr 并 flush），74 处标记全部迁移；`tests/test_native.py` 断言无标记使用 `println`，并断言无 `MEMREDUCT_CI` 时静默、有时刷新；用变异法验证（把发射器改成空操作即失败）。②清理按钮标签颜色只判断 `clean_simulation`，启用生产构建时会被画成禁用态 → 条件加 `or clean_production`。③取消分支原对后端无条件打模拟标记 → 改为按后端选择 marker/note。④确认框第二行文字宽约 290px，超出固定面板 229px 内宽被截断成 `…no system memory chan` → 拆成两行短句并把面板加高到容得下三行文字，窗口新增 `CI_CLEAN_CONFIRM_TEXT` 上报实测宽度，脚本要求 `fits=true`。
- **隔离闭环（真实 GUI 事件）：**`scripts/ci/p3b-clean-simulation-xvfb.sh` 在私有 Xvfb 内用真实 XTEST 点击驱动：点击 Clean → 确认框 → Tab 双按钮焦点 → 点击 Confirm → 模拟授权接受 → helper 阶段 → 结果回显。断言假 helper 收到生产契约 `/usr/libexec/memreduct-zamak-helper pagecache`、结果改变渲染像素（约 2900）、第二次点击不自动重试、全程无特权操作。**不涉及 pkexec、root helper、内核写入。**
- **默认构建门控：**`scripts/ci/p3b-production-gate-xvfb.sh` 设 `MEMREDUCT_CI=1` 但不开模拟后端，断言点击确实落在按钮上（`CI_MOUSE_ACTION target=19`）后被执行分支拒绝（`CI_CLEAN_DISABLED action=none`），且无确认框、无 helper。注意：无窗口管理器时 GTK 按缩放因子绘制为 674×738，固定像素坐标会点空导致假通过 —— 已改为按客户区比例取点。
- **普通模式：**`MEMREDUCT_CI` 未设置下启动，stdout/stderr 均 0 字节、0 个 `CI_` 标记、0 次特权提及、0 条 GTK 警告，窗口 337×369 正常驻留。
- **回归：**`python3 -m unittest discover -s tests` → 61 passed；`zam test .` → 76 passed / 0 failed；`bash -n scripts/*.sh scripts/ci/*.sh` → 通过；`zam check .` → CHECK OK。可运行二进制 `build/memreduct_zamak-7582f543dc00123fb8100b4df5092a9532fd518594e23bf19b01f23f95cbd3f1`（对话框修复后重建；此前含标记修复的 `-c4cd63d6…` 已被取代）。
- **未验收（构造性未验证）：**真实 Polkit 授权、已安装的 root helper、任何真实页缓存写入 —— 均需产品所有者另行明示授权与设备测试，ADR-0005 仍为 PROPOSED。真实 Hyprland 输入注入未做，所有事件流均在隔离 Xvfb 内完成。整页 Windows v3.5.2 像素 1:1 仍 NOT PASS。
- **工作区边界：**`artifacts/` 仍为原有未跟踪目录，未暂存未提交；未触碰其他 worktree、真实 `/proc/sys/vm/drop_caches`、Swap/ZRAM 或 root GUI。

## 2026-10-31 — 普通模式偏好持久化与安全清理模拟集成

- **当前本地代码提交：**`ba85775ecfe3b1be072e34eb2ee0f0304201eb76`，Tree `f4e84ef30525f377554be659a7cce3ea8c8795ad`，父 `d51cbf7b6572c7b386b1d77eb1cc418a015f432f`。分支 `codex/memred-parallel-integrator`。代码改动 8 个文件：`src/main.zm`、`zam.toml`、C 偏好/模拟桥、两个 GUI 测试脚本及生产构建脚本。无远端 push/PR/Base/merge 操作。
- **普通模式设置验证：**生产 GUI 以 UID 1000 在隔离 Xvfb 中启动，整个用例未设置 `MEMREDUCT_CI`，`XDG_CONFIG_HOME` 指向临时目录。English/简中 × Light/Dark 四种组合均通过保存、进程重启恢复；Reset 取消保留原配置字节，确认后新进程恢复默认 English + Light。主题菜单与语言 ComboBox 用 Escape 取消时配置不变；Windows 专属资源 116 明确只读且不写入。五个设置页均完成鼠标导航，Tab/方向键/Enter 截图与对应状态一致；Escape、Alt+F4 关闭后可重新打开；设置窗打开时语言/主题同步通过。配置样本为 schema 1，文件 0600、UID 1000。该默认组合是当前实现值，若产品默认应为简中仍需所有者确认。
- **清理闭环：**仅在隔离 Xvfb 的 `MEMREDUCT_CI=1` 测试运行中，完成确认取消、模拟授权成功/取消/失败、测试 helper 成功/执行失败/不支持/结果不确定、重复调用防护及退出。模拟授权和 helper 均为当前 UID 并仅写入私有临时文件；结果不确定不自动重试。前后 `/proc/meminfo` 的 `MemAvailable` 仅记录观察值，不解释成已清理字节。**正常模式 Clean 仍禁用，真实 Polkit、helper 安装和内核清理未启用或执行。**
- **回归：**治理 34/34、安全 C/Python 41/41、Zamak 0.2.0 `check` / `test`（76/76）/ `build`、13 个 shell 脚本语法检查、生产 GUI/helper 构建、`git diff --check` 均 PASS。普通模式测试与模拟清理报告/截图、源码和构建日志存于 `/mnt/Server_Data_Center/Personal_Files(个人文件)/Cosmos_File/AI产出数据文件/报告与研究/MemReduct-Production-GUI-2026-10-31-P3A/`。生产 GUI SHA-256 `2e09abfa42ab92fd0318d4a72f7c0f1d751ca6b8e21dba35c0063f74fd1cc27d`；测试 helper SHA-256 `b0652c8ac3ae70576e39b71fbdcd047a7c57f23873fec2e45b1b691b34c45a5e`。
- **未验收：**本轮真实键鼠使用隔离 Xvfb，不代表活动 Hyprland 输入；真实 Wayland 窗口交互未运行。整页 Windows v3.5.2 像素 1:1 仍 NOT PASS，字体与原生控件栅格差异未收敛。Canvas 独立 12 项探针未在本轮重跑，既有证据保持独立，不用来替代当前生产 GUI 测试。
- **工作区边界：**源代码已提交；仓库内 `artifacts/` 仍为原有未跟踪目录且未暂存/提交。没有触碰 PR #6 审计工作树、其他 worktree、活动桌面或任何特权系统接口。

## 2026-10-31 — P2C 并行产品代码集成候选

- **远端：**只读核实 `main=83dbf58fb039645f08e08bc89170adcdaa7997be`、PR #7 OPEN / Draft、Base `main`、Head `db05ecd8ca2d49130f21a86cb5f233d378e914ad`。没有 push、Ready、merge 或新 Actions；PR #6 审计 worktree 与原 `p2c-main-integration-r1` worktree 均未改。
- **本地最终代码候选：**`1ba589b86d53fa0909febcce4871fb939d23c715`，Tree `afa501db8b91f98908209ebeb33274afc627ac6c`，父 `4e826f3619a1caef41775bafea8a62cb08f44946`；其祖先包含集成代码 `1d74922437558c96b76b069aae4afe27ce333316`。文档仍有本地未提交修订，源代码无未提交改动。
- **并行线：**UI `10b3c21e3bb120f185958e37f5de4b77968d637e` 增加 Reset 确认框 Tab/Shift+Tab/Enter 与可见焦点；核心 `bc3ce482dd2192e4fb2c780816678e03718ed9aa` 新增固定 pagecache bridge 归一化状态码及 fake-pkexec → 测试 helper 临时文件 E2E；视觉 `6f8147f7f101e14a56cfbaae3a53252d2af1e888` 将两个 Tray ComboBox 闭合高度由 12 调到 13 DLU，历史量测误差由 1.5 px 降至 0.125 px。三条代理线在 Integrator 主窗/CI 集成期间独立并行。该轮 CI/test 由 Integrator 负责，没有第四名 Agent D 同时开发；当前复审另开 CI/test 审查，不回写成上一轮四路并行。
- **GUI 清理闭环：**生产 Clean 按钮仍禁用，`mr_start_reclaim` / `mr_poll_reclaim` 尚未接入生产 UI，ADR-0005 仍 PROPOSED。增加仅 `MEMREDUCT_CI=1` 且明确打开 mock 标志的模拟确认→取消/授权→mock helper→状态反馈；成功路径采样 `MemAvailable` 前后观察值并显式标成无因果归属。新增 XTEST 流程覆盖确认取消、键盘确认、忙碌防重复、成功状态和 WM 关闭；本机缺 Xvfb/Openbox，故该实际事件流尚未运行。
- **集成候选：**从 `db50fd15bc8254240f70c476c0aa5a3cab954a68` 的三条代理线合入本地 `codex/memred-parallel-integrator`，集成 merge 点 `90a3acf348b1bfe13a4aa04ac15959d9ecb3044a`；Integrator 主窗/CI 代码提交 `d6394996828189c6cb30a896070cb64b8974da52`、状态反馈 `1d74922437558c96b76b069aae4afe27ce333316`。UI 复核发现 Reset/Close/确认按钮未处理 Space，且未识别 GTK 小写 `space`；修复提交 `4e826f3619a1caef41775bafea8a62cb08f44946`、`1ba589b86d53fa0909febcce4871fb939d23c715` 已快进集成。三条代理实际并行；CI/test 由 Integrator 负责。没有改远端分支或生产清理行为。
- **CI Agent D 后续审查：**以 `1d74922437558c96b76b069aae4afe27ce333316` 为基线在隔离 worktree 审查 P2B 故障门槛、P2C 136 状态机/XTEST、Windows DLU 实验；未发现可由本机安全复现的脚本根因，未改代码或提交。治理 34、Shell 12 个脚本、diff check PASS；Xvfb/Openbox/Weston/PowerShell 缺失，相关 GUI/Windows 实测仍 NOT RUN。
- **本机回归：**最终代码 HEAD 治理 34 PASS；Python/C 安全 15/15；Zamak 0.2.0 check PASS、test 75/75、build PASS；production GUI/helper build PASS；全部 shell syntax 曾在集成树通过，Agent D 对 12 个 CI shell 脚本再次检查 PASS；Canvas probe 12/12 的源码未变，已有 exact-source artifact PASS；`git diff --check` PASS。
- **实际 Wayland 启动：**通过当前 `wayland-1` 会话以普通用户启动最终 exact-source 二进制两次，各设 7000 ms 自动退出、无键鼠注入。主窗单独运行和主窗+Settings/Appearance (resource 103) 均 `CI_APP_RETURNED`、exit 0、无 GTK WARNING/CRITICAL/ERROR；第二次资源 136 为 CI-only checked preview，`persistence=none side_effects=none`。主窗截图 337×369 SHA-256 `7b170ecd3b5e861be171253e5da2d8e481595ce410915e1ae207c250e90700e9`；Appearance 截图 558×335 SHA-256 `0dde7b2ee89174316ee9b61259abf45829623248cd4d6045cc3965e2d3005783`。本次只验证启动/绘制/实时采样/定时关闭，不代表交互测试。
- **仍未运行：**缺少 Xvfb、Openbox、Weston、PowerShell，故 Reset/Clean XTEST、Weston 生命周期、Windows DLU/采集/视觉比较 NOT RUN；没有新 GitHub CI。真实 Hyprland 鼠标键盘输入、五页交互、简中/深色配对均 NOT RUN。全页 1:1 NOT PASS；同源字体差异、Windows 控件栅格和 12 项简中资源 `DECISION_REQUIRED` 仍未解决。
- **产物：**前一候选证据位于 `报告与研究/MemReduct-Parallel-Delivery-2026-10-31/integrator-checks/`；最终 GUI/helper、构建日志和源码/二进制 SHA 位于 `报告与研究/MemReduct-Parallel-Delivery-2026-10-31/integrator-final-1ba589b/`，Wayland app-owned screenshots、日志、环境与 SHA manifest 位于其 `real-wayland/`。未触碰真实 `/proc/sys/vm/drop_caches`、Polkit、Swap/ZRAM、root GUI 或其他窗口输入。

## 历史快照（2026-10-31 — P2C-R8 三项 CI 阻塞修复候选）

- **远端状态：**PR #7 远端仍 OPEN / Draft、Base `main@83dbf58fb039645f08e08bc89170adcdaa7997be`、Head `db05ecd8ca2d49130f21a86cb5f233d378e914ad`；main 未改，无推送、Ready、Merge 或新 Actions。远端引用核验仍精确匹配以上 SHA。
- **本地代码提交：**`bbe588ccd24bef7828dda34066f0743b24caf350`，Tree `61dea19a945f59f9a207bf568c731ce2877b76c5`，父 `b2827621a1045ba1ebcd81e84cd81f7fc41c7c2a`；修改 9 个工作流、C/Zamak、CI 脚本和测试文件。之后的记忆更新单独提交。
- **P2B 修复：**删除基于 elapsed-ms 的自动注入。`native/gui_test.c` 仅在 `MEMREDUCT_CI=1` 下接受当前 UID、单链接、普通文件、模式 0600 的私有 token；`O_NOFOLLOW|O_NONBLOCK` 拒绝符号链接与 FIFO 等风险路径。脚本用私有临时文件原子重命名，先确认 `/proc/meminfo` 与简中文本已绘制并捕获故障前图，再触发。失败后 CI 状态锁存至进程退出；断言唯一错误来源、旧值清除、不可用重绘、无后续成功采样及截图 changed-pixel。
- **P2C 修复：**resource 136 启动时已选 Appearance (IDD 103)；脚本核对默认勾选，不再等待同页重复导航。为了验证 NAV 事件，先导航到 General 再回 Appearance，并回归原勾选状态。新增控件区域 crop 的 hover/down/up、Tab/Space截图与 changed-pixel 断言；测试验证无持久化和副作用。
- **Windows 修复：**截图进程不再在自身上下文直接调用外部页面 HWND 的 MapDialogRect。截图后由独立 C# 子进程记录外部 HWND PID/类/有效性/父级/DPI/P/Invoke RECT 布局并复现访问冲突；受控 `CreateDialogIndirectParamW` 同进程对话框依据页面 `WM_GETFONT`/LOGFONT 脸名和上游 8pt 字号测量 260×168 DLU，匹配 DPI 后以 52 字母 `GetTextExtentPoint32W` 与原版页面真实 390×273 等像素矩形交叉验证。比较工作流在采集失败后也执行，以留出失败证据。Windows 采集脚本中的 helper 创建 API、P/Invoke ABI/退出码与全部 10 组校准尚未通过 hosted run。
- **本机回归：**治理 34 PASS；C/Python 安全测试 14/14；Shell syntax 12/12；Zamak 0.2.0 `check` PASS、`test` 69/69、`build` PASS；`scripts/ci/build-production-gui.sh` PASS（GUI 与 helper）；`scripts/build-arch.sh` PASS；Windows 工作流 YAML parse 与 `git diff --check` PASS。构建产物/日志在隔离 `/tmp`，未覆盖仓库中已有 artifacts。
- **未运行：**本机没有 Xvfb、Weston、PowerShell/.NET SDK，故 P2B/P2C 真正 XTEST、Checkbox 116/136 CI 事件、Windows MapDialogRect/DLU、Windows 原版截图及 Linux/Windows 对照均 NOT RUN；旧 HEAD `db05ecd` CI 状态不能代表此本地提交。
- **保护边界：**已有 `artifacts/` 21 个文件保留且 manifest 摘要 SHA-256 前后为 `d906a5f4606cb83a1cb3eb6f4b1ab79adfaed6ceab78805610fba5d65984e6b5`。PR #6 审计 worktree 未改。没有实际内存清理、Polkit、root GUI、Swap/ZRAM、活动 Hyprland 输入。整页 1:1 继续 NOT PASS。
- **下一步：**等待对本地候选独立复审及新增推送授权；只有新 HEAD 四条 Actions 和 P2B/P2C/Windows artifacts 全部真实通过后，才可关闭这些 CI 阻塞。

## 历史快照（2026-10-30 — PR #7 db05ecd 检查前的状态记录）

- **远端状态：**授权提交 `db05ecd8ca2d49130f21a86cb5f233d378e914ad` 已普通快进推送到 PR #7。PR 仍 OPEN / Draft、Base `main@83dbf58fb039645f08e08bc89170adcdaa7997be`、Head `db05ecd8ca2d49130f21a86cb5f233d378e914ad`、`mergeable=true`；未合并、未改 Base/main。GitHub merge ref `b5e152af280df50a4eaf3e37ef3a9a4b189cb8b1`。
- **新 HEAD CI：**Build [41029656984](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656984) PASS；P2A [41029657044](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029657044) Xvfb 与 Weston 两个 Job PASS，独立 Canvas 探针 12/12；P2B [41029656992](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656992) Weston PASS、Xvfb FAIL；P2C [41029656981](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656981) Windows 和 Linux Xvfb 采集 FAIL，比较 Job SKIPPED。不能把该 HEAD 报为全绿。
- **Artifacts：**P2A X11 [12964958179](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029657044/artifacts/12964958179) SHA-256 `e6e24cea43181727f8783df8b95eeb5e892bb4d954a7492ca7d1b09097149908`；P2A Weston [12966090064](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029657044/artifacts/12966090064) `0acac4b305e277aad4d1a4c0250399499585137b9d95428650d741f3f90c5931`。P2B X11 [12965232047](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656992/artifacts/12965232047) `8672359dd4dd2e6335c9854d2cab7a0135f26c486026d085eb6c2fe2cf73da45`；P2B Weston [12965561181](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656992/artifacts/12965561181) `0f22dbfa87fc5d68f573dd2fc3ffb64e8b547290eec73db15f4be752f7900be6`。P2C Windows [12964669276](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656981/artifacts/12964669276) `3ce779fc5c57a4c33cac02b1e2070f550375c1a55efdd37377db4af7a0c2a4e3`；P2C Linux X11 [12965088985](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656981/artifacts/12965088985) `a1c84abd71c0995800db407e22f77ca25aa1dda4da95e1e055015244ed3204ed`；P2C APT [12965981001](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/41029656981/artifacts/12965981001) `711a542fb7bff827f6eb7e3a9a98d29b5a686f1a991eaadc703956abaafb6906`。
- **故障定位：**P2B 日志显示应用启动约 19 秒后才到“故障前”截图，而 CI 注入阈值为 15 秒；失败已先发生，故两张裁剪图无像素变化，`assert_changed` 失败。此前 regex 已适配 `collapsed=false`，GroupBox 折叠/展开、菜单和键盘交互顺序已执行到内存失败对照。P2C Xvfb 已捕获五页中英亮色/深色截图；resource 116 Checkbox 鼠标及 Space/Enter 测试通过。resource 136 Appearance 预览启动时已由生产测试门控直接选中 `IDD_SETTINGS_APPEARANCE=103`，脚本又点击相同导航项并等待只在状态变化时生成的 NAV marker，导致误判失败，后续 resource 136 事件未运行。Windows Runner 完成官方 v3.5.2 来源校验和菜单/初始设置截图后，在 `MapDialogRect` 的 page-child HWND 调用处以 `0xC0000005` 崩溃；P2C 深色四实验和比较作业未执行。P2C APT update/install 退出码为 0。
- **本地修复候选：**代码提交 `b2827621a1045ba1ebcd81e84cd81f7fc41c7c2a`、Tree `f404eb9aa74e6fc9d056d8a635836066d35c2fec`、父 `db05ecd8ca2d49130f21a86cb5f233d378e914ad`，只改三个 CI 采集脚本：P2B 延后注入到 35 秒并在“故障前”截图后断言还未失效；P2C Appearance checkbox preview 核验初始页面 marker、不重复点击已经选中的导航项；Windows DLU 校准改从已核实的顶层设置对话框调用 `MapDialogRect`，并在 CSV 记录测量来源。**Windows 变更尚未由 Windows Runner 验证，原因未完全确认；需要下一次 hosted run 证实或继续调整。**该候选未推送。
- **本地验证：**治理 34 项 PASS，C/Python 安全 13/13 PASS，Zamak 0.2.0 check PASS、test 68/68 PASS、build PASS；修改的两个 Shell 脚本 `bash -n` PASS，`git diff --check` PASS。当前机器无 Xvfb、Weston、PowerShell，因此修复后的 GUI/Windows lane 未本地复现。工作树仍保留原有未跟踪 `artifacts/`，未触碰 PR #6 审计工作树。
- **边界：**当前 GitHub CI 仍有 P2B/P2C 失败，以上本地候选等待审查与新的推送授权；没有新的远端推送、Ready 或 Merge。整页像素 1:1 仍 NOT PASS；真实 Hyprland 键鼠、实际内存清理、Polkit、Swap/ZRAM 未执行。

## 历史快照（2026-10-30；PR #7 HEAD faa3426）— P2C-R8 指定推送后的 CI 故障与本地修正

- **远端状态：**PR #7 OPEN / Draft、Base main、Head  faa3426a6b17f50b088c86ab56e7d7cae689af5e，main 83dbf58fb039645f08e08bc89170adcdaa7997be，mergeable=true。R8 源码提交 3c41879d7589472fd64b7f8563268d60bc0b99bb Tree c0cdd138aa1633dc46893e11e13edb60a64338e7；指定文档提交 FAA 的 Tree 60b212bca5e6b3c9d632adfdead3dbf81fb8a83b，父为源码提交。仅该指定提交已普通快进推送。
- **新 HEAD Actions：**Build [40997247457](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40997247457) PASS，68/68 Zamak 测试；P2A [40997247341](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40997247341) 两个 Job PASS、Canvas probe 12/12；P2B [40997247380](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40997247380) Weston 生命周期 PASS，Xvfb FAIL；P2C [40997247232](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40997247232) Linux Xvfb 与 Windows 采集 FAIL，compare 因依赖失败跳过。Artifacts：P2A X11 12951842817 SHA 10b619ee05b90d4a209e9012f9f415d70227e1c4a32fbcc1d4cb5d4388e80d3c，Weston 12952255887 SHA da8b1eaa77755d2b6636ef4d31efebd4de84dab1c51ec3430f6f14a30a97b743；P2B X11 12952017231 SHA 4eec193940773cebbd5a7f5d8888c9d576a7fd50b6711a740f7f2d99959a0dcc、Weston 12951947271 SHA 473176db67722b42664d75210f25803f9c0591c8346fba0e85851a9c05818e16；P2C Windows 12952140838 SHA 617a239530a8179cf1a5d42d02d852b1290d80a8d401c925909e6383e4d5d4b8、Linux 12951349916 SHA 723f89e6f0562361562afacb26a02bc01d59efea1ff753ddf68a113e091fbe75、APT 12951364620 SHA 9aef4e5fb3b12efeab758ce4915b0933461a08ed7f950d23191f503839f59187。
- **复现与修复：**P2B 正则要求 title=Physical memory usage，而真实 R8 日志包含 title=Physical memory collapsed=false usage=...；对上传的 client.log，旧式匹配退出 1、新式匹配退出 0，所以旧脚本在任何主窗 XTEST 前退出，GroupBox 折叠/展开及随后设置页步骤未执行。P2C 的两个 xdotool key Space 被 xdotool 拒绝为未知 keysym；替换为有效 X11 名称 space，保留 Checkbox 键盘事件断言。Windows 在捕获初始英文 General 后，经 PowerShell [ref] RECT 调用 MapDialogRect 以 0xC0000005 崩溃；改为在 C# helper 内构造/传递 RECT，并向 PowerShell 返回值对象，避免 PowerShell 的 ref-struct 边界。
- **P2C 事件证据：**故障前 Linux artifact 已完成主菜单、五页中英浅/暗截图、语言 ComboBox 鼠标和键盘切换、列表滚轮与关闭等脚本断言；checkbox 预览录到了鼠标 hover/press/check/focus 图，失败发生在 Space 键之后，故键盘切换和第二个 Appearance checkbox 流程未通过。比较 Job 被跳过。首张 normal-preview PNG 含左上黑块，后续状态截图无此块；截图保留，根因未确认，不纳入视觉验收。P2B collapse 事件在新 HEAD 运行中计数为 0，不得宣称实际 XTEST 已验证。
- **本地修复候选：**提交 9b5895026ae1c48a301aaf285e3cffab8f17a446，Tree e79ecab33e11b6da06480bdb38f4f1ae56446d20，父为 FAA。变更 scripts/ci/p2b-production-xvfb.sh、scripts/ci/p2c-settings-xvfb.sh、scripts/ci/windows-native-baseline.ps1、src/settings_resources.zm（移除 EOF 空行）。该修复提交未推送；远端仍为 FAA，所以没有新 HEAD Actions。
- **本地验证：**治理 34 项、C/Python 13/13、Shell 全脚本语法、Zamak 0.2.0 check/test/build（68/68）、生产 GUI/helper 构建、对 main 的 diff check 均 PASS。当前主机无 Xvfb、pwsh；脚本真实交互和 Windows P/Invoke 修复未本机运行。Build evidence 输出在用户 AI 产出目录 报告与研究/Mem Reduct P2C-R8-2026-10-30/r8-ci-repair-local-20261030/。没有访问活动 Hyprland 或执行清理/特权操作；像素 1:1 仍 NOT PASS。
- **停止边界：**遵守本轮授权，不再推送任何后续提交。需要新 PR HEAD 的四条 Actions 才能证明这些 CI 修复；PR 保持 Draft、未合并。

## 历史快照（2026-10-30；P2C-R8 指定提交推送前）

- **Git：**继续使用 `codex/p2c-main-integration-r1`；PR #7 远端 HEAD 仍 `019a7f1e0755570c2c3a0b1202a6c764540f14e2`、Base `main`，main `83dbf58fb039645f08e08bc89170adcdaa7997be`。R8 源码提交 `3c41879d7589472fd64b7f8563268d60bc0b99bb`、Tree `c0cdd138aa1633dc46893e11e13edb60a64338e7`、父 `307cd889d9878ec01cf72937f84f67ee8252f50f`。未推送、未改 Base、未 Ready/Merge；PR #6 审计工作树未触碰。原有 untracked `artifacts/` 保留。
- **实际源码：**增加 `src/settings_resources.zm`，按固定上游资源列出五页 ID/class/style/exstyle/DLU/control defaults 与 Manifest/DPI 状态；在生产 Canvas 实现资源驱动 GroupBox、AutoCheckbox、ComboBox 绘制和相应焦点/hover/pressed/checked/dropdown/keyboard 状态；新增滚轮事件桥及设置列表派发；测试脚本覆盖鼠标 wheel、Checkbox 预览、语言下拉和 Win32 DLU/字体状态采集。安全边界保留：所有未实现设置只读/disabled，Checkbox preview 仅 CI allowlist、进程内、不保存、不清理。
- **前后对照：**历史 English/Light/General 558×335 图在 GroupBox 修正前全页差异 9.3388%，修正后 7.6649%；组框顶线 ROI 91.0256%→25.3846%，标题 ROI 39.0000%→28.6923%。截图二进制的 `settings_ui.zm` SHA 是 `c63264ad2d60fb109ddd2f6e92eea0d9f40554984ad0ae47654fff83d67af499`，早于最终 ComboBox 箭头改动，不能当作当前源的 exact-head 截图。CI Checkbox preview 的 ROI 50/483 pixels differ；是测试状态，非生产设置。最终箭头模板已更新并有单测，但没有箭头代码后的生产 PNG。
- **回归：**Zamak 0.2.0 68/68、项目治理 34、C/Python 13/13、全 Shell 12/12、生产 GUI/helper build、Arch/native build、Canvas probe 12/12、`git diff --check` PASS。Xvfb 与 Weston 测试脚本实际尝试后分别因缺 `Xvfb` / `weston` 在 GUI 启动前退出 2；`wine`/`winegcc`/`wrc` 不存在，R8 Winelib A/B NOT RUN。没有新 GitHub CI。
- **差异/安全：**原来 95/120 同源文字仍有 >2 px 差异，未更换字体；简中 12 项仍 `DECISION_REQUIRED`。184/184、1.5 px、导航 10/10、树线 102/102、栅格 36/36、滚动 2/2 仍是先前 checkpoint 历史值，本轮未对当前源通过新图片重测；整页 1:1 `NOT PASS`。当前会话有 Wayland/Hyprland 标记，但没有连接 IPC、启动 GUI、键鼠输入或触碰其他桌面窗口；没有内存清理、Polkit、Swap/ZRAM 或 root GUI。
- **证据：**测试日志、环境、源码/二进制 SHA、Canvas probe 与脚本缺依赖原始退出信息在用户 AI 产出目录 `报告与研究/Mem Reduct P2C-R8-2026-10-30/r8-closeout-20261030-v3/`；历史截图/overlay/CSV 保存在同主题 `r8-final-local-20261030-v1/` 与 `r8-final-verification-2026-10-30-v2/`。完整修复说明见 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。

## 2026-10-29 — 用户反馈后修复共享状态重绘并复核第三方路线

- **Git：**隔离分支 `codex/p2c-main-integration-r1`。本地代码提交 `640fb5df044475df827308388c3da5be25d4ffad`、Tree `f30cac1ce113016dfef58eca46417549253d2be2`、父 `f8bd6cc876b3cbb4623d6dbe6f5d4b94a348c05d`；文档提交 `84dcd46af45d0f30f7b64d25256054fefe569388`、Tree `f732194385b63a7faf2a142ab72bb45b8450d6c0`。远端 main `83dbf58fb039645f08e08bc89170adcdaa7997be`、PR #7 `019a7f1e0755570c2c3a0b1202a6c764540f14e2` 未变；PR #7 仍 OPEN / Draft / Base main，未推送。
- **实际修复：**主窗口语言或主题菜单更新后，现会通知已打开的设置 Canvas 重绘；Zamak 新增状态选择回归。fake `pkexec` 测试覆盖异步桥退出码 1、3、126，确认不重复启动；没有真实 Polkit 或内核写入。
- **Winelib 实测：**已有 R6 隔离 General 页 POC 以官方 `resource.rc` 和 Win32 控件树编译。背景/焦点状态对齐后，558×335 changed-pixel 英文 13.2531%、简中 12.2725%，背景样本 0/2500 不同；GroupBox 仍 20.6%–21.7%，按钮约 37.3%–37.6%。英文垂直字体度量与 Windows GDI 同为 11/2 px，但 CJK 使用 Noto Sans CJK SC (13/3 px) 且字形/栅格仍不同。Wine Common Controls v6 manifest 缺 assembly 的 warning 未解决。该 POC 不是完整原版或生产迁移，状态不能与 GTK 截图直接排名。
- **界面问题：**设置页同源文本仅 25/120 宽高差≤2 px、95/120 超差；全页 1:1 仍 NOT PASS。设置页禁用灰度源自未实现功能，不能假装启用；主窗旧 20.9436% 比较因动态值、边界和安全状态不匹配，不是验收率。详见 `docs/ZAMAK_UI_FEASIBILITY.md`。
- **验证：**治理 34 项、C/Python 12/12、Shell 全脚本语法、Zamak 0.2.0 44/44、生产 GUI/helper build、Canvas 探针 12/12、`git diff --check` 均 PASS。本轮另以真实 Wayland 会话变量启动本地 GUI 7 秒：主窗和 English/Light General 页捕获成功，退出码 0，无 GTK warning/critical/error。Xvfb/Openbox/Weston/Wine/winegcc/wrc 未安装；没有新 GitHub CI。真实键鼠、五页切换、主题/语言切换、内存清理、Polkit、Swap/ZRAM 和 root GUI均未执行。
- **证据：**R6 并排/Overlay/Difference/Heatmap、比较 CSV、字体输出与 PNG SHA 保存在用户 AI 产出数据目录的 `P2C-UI-Core-Fidelity-R6-20261029` artifact；R7 构建与探针日志在 `P2C-UI-Core-Fidelity-R7-20261029/verification-640fb5df/`。本轮 Wayland Main SHA `fe90da01aac4cfa96861d8db42bdbbe0127087b694071f893ec9d7b21f0e0f3a`，General SHA `ee33df415eacd6e69d448cc269b44be7fd17092ee436bab616e0d494798e1551`。原始证据未覆盖。

## 历史快照（2026-10-29，R6 Winelib 小样与重绘修复之前）— UI 路线与内存核心并行收口

- **远端边界：**当前隔离候选分支 `codex/p2c-main-integration-r1`。PR #7 仍 OPEN / Draft、Base `main`、远端 HEAD `019a7f1e0755570c2c3a0b1202a6c764540f14e2`；`main@83dbf58fb039645f08e08bc89170adcdaa7997be` 未改。本地代码提交 `f8bd6cc876b3cbb4623d6dbe6f5d4b94a348c05d`，Tree `eaa616a12bc17761881fbf86d5cc9a209a96a670`，父 `cd6703d670d82f9712e83012dde0325efec5e615`；文档/记忆收口在后续本地提交，未推送。
- **实修与并行：**生产 GUI 已在之前本地提交修复平铺窗口可拉伸及首屏焦点框；当前代码另行校验 `/proc/meminfo` 必需/可选字段必须使用 kB，并把 `Cached` 与 `SReclaimable` 分开。helper/异步桥只接受固定 `pagecache` 动作与固定生产路径；拒绝 `metadata/all`、符号链接与非普通测试目标；EINTR 仅重试一次，短写/close 错误返回不确定且不自动重试。UI 不调用 helper；未做 Polkit、真实内核写入、Swap/ZRAM 或 root GUI。
- **UI 事实：**P2C 几何 184/184、最大分量 1.5 px；文本 25/120 宽高差≤2 px、95/120 超过；有效深色配对 changed-pixel 5.4004%–12.6625%；全页 1:1 仍 NOT PASS；12 项简中来源差异维持 `DECISION_REQUIRED`。GTK 主窗客户区 337×369、设置客户区 558×335；历史主窗 20.9436% 图像差异是数据/状态/边界不一致的定位样本，不是验收率。R6 有界 Wayland 启动截图退出 0；本轮不重新打开用户窗口或注入输入。
- **第三方路线结论：**Qt 6.12 `QProxyStyle` 单页 POC 与 Wine Tahoma 字体测试没有降低整页差异；当前 GTK/Pango/Cairo 也没有 Win32 GDI 字体/控件。下一步以官方 v3.5.2 `.rc`/控件树做 Winelib 单页 POC。Wine 官方 Winelib 可用 `winegcc`/`wrc` 编译 Win32 源码；Wine 9.22 起 Wayland driver 进入默认配置，11.15 启动回归已在 11.16 标记修复，Arch 仓库当前 Wine 为 11.20-1。当前本机未安装 `wine`/`winegcc`/`wrc`、Xvfb、Weston；Winelib 与本机隔离 GUI 交互仍为 NOT RUN。Wine/Winelib 不等于 Windows GDI，不承诺像素一致。
- **本轮验证：**`scripts/ci/build-production-gui.sh` PASS：治理 34、C/Python 11/11、Shell 4 项、Zamak 0.2.0 check、43/43 tests、生产 GUI/helper build；`scripts/ci/build-canvas-probe.sh` 12/12；`scripts/build-arch.sh` check/test/native build PASS（未运行安装器）；全 Shell 语法与 `git diff --check origin/main...HEAD`、工作树/暂存区检查 PASS。没有本轮 GitHub CI；远端 PR #7 无变化。
- **证据限制：**主窗/设置页 PNG SHA-256 分别为 `afe45efcfd72317bc99880e7c6602ab71f6b8c7aebb5e0b56b3259df1875c573` / `ee33df415eacd6e69d448cc269b44be7fd17092ee436bab616e0d494798e1551`。当前会话环境报告 `GDK_SCALE=2`，但 GTK factor 1 及逻辑尺寸不变；未取得 compositor 装饰框物理尺寸，不能据此断言缩放为“大窗口”原因。完整上游路线资料见 `docs/ZAMAK_UI_FEASIBILITY.md`；独立报告和截图保存在用户 AI 产出目录。
- **本次最终复跑：**生产 GUI 检查治理 34、C/Python 11/11、Zamak 43/43、GUI/helper build PASS；独立 Canvas 探针 12/12；Arch check/test/native build PASS；全 Shell 语法和 diff check PASS。没有新的 GitHub CI、Winelib POC 或本机 GUI 输入结果；PR #7/main 未修改。

## 2026-10-28 P2C toolkit/core follow-up（本地未推送）

- 用户指出不应把 1:1 目标包装成已达成，并要求同时推进界面路线和内存核心。复核确认：设置页 `184/184` 只测控件矩形；当前 General 页配对 PNG 的 Qt POC 整页 English/简中差异为 `12.7155%/11.9740%`，GTK 为 `12.0184%/10.6617%`，截图状态不同，只用于定位，不作为工具包公平排名。Qt General group ROI 有局部改善，但导航/按钮没有；Qt Reset/Close ROI 差异约 55%，目前没有生产迁移证据。
- 当前主窗候选请求 GTK logical `337×369`、Canvas `313×345`；固定 Windows 原版主窗图为 `352×407`，截图/客户区边界并非相同类型，不能据此声称完全同尺寸。早期 R1 探针曾被平铺为 `941×1030`，已作废；R6 有界 Wayland 启动记录 scale factor 1，但未测 compositor 外框或物理显示比例，也未操作 Hyprland。
- 内存核心安全收敛：helper 与异步桥现只接收 `pagecache`（固定写值 `1`），拒绝 `metadata/all`；测试构建才允许临时 regular-file 目标；`EINTR` 一次重试，短写/close 错误返回不确定且不自动重试。GUI 未连接 helper；ADR-0005 仍为 PROPOSED；未运行真实内核写入。
- 修正字体统计：P2C 的 120 条同源设置文本为 25 项宽高差≤2 px、95 项>2 px。P2C 设置页 Pango 度量是 10/3 px；P2B 主窗口记录为 11/3 px，两者来自不同 Canvas 采集。历史“9/120”是误记，已按 `docs/P2C-3_VISUAL_FIDELITY_REPORT.md` 更正。
- 最终本机回归：Zamak `0.2.0` check/test/build PASS，测试 `43/43`；生产 GUI/helper build PASS；独立 Canvas probe `12/12` PASS；治理 34 项、C/Python 安全 `11/11`、Shell `12/12`、Arch build、候选相对 `origin/main` diff check 均 PASS。精确日志、PNG/Qt/Wine 字体 POC 与构建 SHA 在用户输出目录 `.../P2C-UI-Core-Fidelity-R4-20261028/`。
- Qt 6.12 可用；GTK3 为 3.24.52。Wine/winegcc、Xvfb/Openbox、Weston 本机缺失。未安装包、启动活动桌面 GUI、查询 Hyprland IPC 或注入输入；因此 real Hyprland interaction NOT RUN。PR #7/main 未修改。

## 2026-10-28 P2C 工具路线初次复核快照（helper 单动作收紧前；本地，未推送）

- **Git：**安全工作树 `codex/p2c-main-integration-r1`，开始 HEAD `cd6703d670d82f9712e83012dde0325efec5e615`；远端 PR #7 OPEN / Draft、Base `main@83dbf58fb039645f08e08bc89170adcdaa7997be`、Head `019a7f1e0755570c2c3a0b1202a6c764540f14e2`，本轮未推送/改 Base/Ready/合并。另一 `p2b-r6-integration-audit` 工作树有预暂存内容，本轮未触碰。
- **UI 调查：**正式 3.5.2 逻辑字体 `MS Shell Dlg 2` 在核验 Windows 文档中映射 Tahoma；P2C 设置页 Linux Pango 为 Sans 8 pt、度量 10/3 px，P2B 主窗口另记录 11/3 px；Windows GDI 为 8.25 pt、11/2 px。120 条同源设置文本中 25 项宽高差≤2 px、95 项仍不同；12 项中文来源差异依旧 `DECISION_REQUIRED`。历史“9/120”已更正。Liberation Sans 候选 English 64/66 改善，但 CJK 12 改善、12 退化，未采用。Qt 6.12 `QWindowsStyle` 离屏探针匹配 Liberation Sans、度量 10/2 px，控件图仍不接近原版；默认切换控件库不能得到 1:1。新增 `docs/ZAMAK_UI_FEASIBILITY.md` 的第三方路线、字体证据及单页 Qt `QProxyStyle`/`QCommonStyle` POC 建议。
- **差异诚实分级：**P2C 设置页 184/184 几何（最大分量 1.5 px）、10/10 导航、102/102 树线、36/36 栅格仍 PASS；有效暗色 changed-pixel 5.4004%–12.6625%，整页 1:1 NOT PASS。主窗旧 20.943604% 是数据/禁用状态/渲染未配对样本，不能作为等价验收率。真实完整外框和 Hyprland 键鼠交互当前 NOT RUN。
- **内存核心并行进展：**新增 PROPOSED `docs/ADR/0005-manual-page-cache-reclaim.md`；helper 固定生产路径 `/proc/sys/vm/drop_caches`，仅测试构建可写临时目标，拒绝 symlink/非 regular 文件，`EINTR` 最多重试一次，短写/close 错误返回“不确定”且不自动重试。7 项 C/Python 测试用临时文件/注入 write fault，无真实内核写入；UI 仍未连接 helper，清理仍禁用。
- **本轮最新构建：**生产脚本 PASS：治理 34 项、C/Python 安全 11/11、Zamak 0.2.0 check、主项目 43/43、production GUI/helper build；Canvas 独立探针 check/test/build 12/12 PASS。另有全仓 Shell 12/12 与 Arch build PASS。Artifacts 写入外部 `.../P2C-UI-Core-Fidelity-R4-20261028/final-build/` 和 `probe-final/`，没有覆盖原 `artifacts/`。Xvfb/Openbox/Weston/Wine 未安装，因此本机隔离交互 GUI/Winelib 未运行；活动 Hyprland 未触碰。当前文档修正后的最终回归已完成，远端 PR 与 main 未修改。

## 本地 P2B-R6 界面复验（2026-10-28；未推送）

- **Git 基线：** 当前安全工作树 `codex/p2c-main-integration-r1`。生产/测试代码已本地提交 `5101be104500e61a41d102747216c5ff407b7595`，Tree `36b5f2428a7663842d124b668101fc2dc17d5ac9`，父提交 `16eae08d95cd3f8ca19ccad205ae1cce3bcc4901`；提交只包含 8 个源码、测试与 GUI CI 文件。README、P2B 报告、交互矩阵及短/中期记忆已同步更新并本地提交，未推送。远端 PR #7 仍 `019a7f1e0755570c2c3a0b1202a6c764540f14e2`、`main` 仍 `83dbf58fb039645f08e08bc89170adcdaa7997be`；没有改 Base、Ready 或合并。
- **复现与修复：** 同尺寸官方 Windows/Linux 客户区对照发现 Linux 首屏错误地在 `File` 周围绘制键盘焦点框。原因是 `src/main.zm` 初始逻辑焦点为 index 0；现改为无焦点 `-1`，Tab 首次转到 File，方向键/Enter 从无焦点状态仍可进入 File，并新增焦点初始化/循环回归测试。旧截图保留，未覆盖。
- **本地回归：** 最终 `scripts/ci/build-production-gui.sh` PASS（治理 33/33、C/Python 安全 3/3、Shell 4/4、Zamak 0.2.0 check、测试 `41/41`、GTK GUI 与 C helper build）；独立 Canvas 探针 check/build PASS、`12/12` PASS；最终 `git diff --check` PASS。最终构建记录及二进制位于 AI 产出目录的 `P2B-Frontend-Cal-20261028-R6/build-r5/`，生产二进制 SHA-256 `50f17c01c5e4764b4febcafb1cb8e69ebf781208ef1e3d3dcbd88ede451af9a5`；`src/main.zm` SHA-256 `81c910ff5343ed5d3bdb7e2aeb028098fef61f8e0e4045c692cd72627f81799b`。独立探针最终产物与 12 项日志在该目录下的 `probe-build-r5/`。
- **真实 Wayland 有界截图：** 普通用户以 `GDK_BACKEND=wayland` 启动生产 GUI，测试计时器 6 秒后退出码 0；GTK 没有 `WARNING/CRITICAL`，报告 scale factor 1，主窗分配/客户端为 `337×369`、Canvas `325×357`。完成窗口是应用本身的 337×369 客户区；GTK 该次没有给出可用于核实 compositor 可见标题栏尺寸的独立数据。因此证据没有证明客户区“很大”，也没有测完整屏幕装饰尺寸。没有发送键鼠输入。
- **1:1 对照：** 官方 Windows v3.5.2 client crop 与 Linux 截图都是 `337×369`，English/light/第三组选中；Windows crop SHA-256 `d61e4c22d72c7fe647a5ed347873370140db1fd36256524ce600b0c95bd37857`，Linux 修正后 SHA-256 `8536d5de806916afeb11310b65572eb363fea11aa712f287804d59200e5fa9fc`。原始 exact RGB 参考差异为 `26,044/124,353 = 20.943604%`。该 pair 仍非语义像素验收：Linux 实时 `/proc/meminfo` 值不同、System working set 不支持故显示 em dash、清理按钮必须禁用，且 Win32/GDI 与 GTK/Pango 字体栅格不同。全页 1:1 `NOT PASS`，P2C 设置页 `184/184` 几何不代表主窗像素通过。
- **GUI 运行边界：** 本机无 `Xvfb`、`xvfb-run`、Openbox、Weston，未安装系统包；因此本地 XTEST、五页 GUI 自动切换和 Weston 生命周期未运行。旧 PR #7 HEAD `019a7f1…` 的 GitHub 8/8 PASS 不验证此未推送候选。真实 Wayland 鼠标/键盘、Alt+F4/WM_CLOSE、设置窗五页实机交互继续 `NOT RUN`。没有真实清理、Polkit、Swap/ZRAM、root GUI 或活动桌面输入。
- **证据：** R6 主窗截图、Windows/Linux 并排图、Overlay、Difference、Heatmap、像素指标 JSON、运行日志和 SHA 文件位于 AI 产出目录的 `P2B-Frontend-Cal-20261028-R6/visual/main-r6-final/`。含设计差异与迭代历史的 `design-qa.md` 位于同一项目目录。

## 本地 P2B-R2/R3 复验（2026-10-28；未推送）

- **Git 状态：** 在安全工作树分支 `codex/p2c-main-integration-r1`，本地 HEAD `16eae08d95cd3f8ca19ccad205ae1cce3bcc4901`（父 `55cba2275a35e4a0b889578143c506dff885b343`），比 PR #7 远端 `feat/issue-3-p2c-settings-ui@019a7f1e0755570c2c3a0b1202a6c764540f14e2` 多两条本地文档提交，另有未提交源码/文档修改。远端 HEAD、Base 和 main 未改；没有推送。旧 `p2b-r6-integration-audit` 工作树未触碰，`artifacts/` 未跟踪内容保留。
- **最终本地构建：** 在最后的 `src/main.zm` 和 `native/gui_test.c` 修改后再次运行 `scripts/ci/build-production-gui.sh`：治理 33/33、C/Python 安全 3/3、Zamak 0.2.0 check PASS、Zamak 37/37、Zamak build PASS、GTK 生产 GUI 与 C helper build PASS。独立 Canvas 探针 check/test/build PASS，12/12；所有 `scripts/*.sh` 和 `scripts/ci/*.sh` 语法 PASS；最终文档修改后的 `git diff --check` 仍待最后执行。无 Xvfb/Openbox/Weston，本机 GUI 自动化 lanes 未运行；该本地源没有新 GitHub CI。
- **真实 Wayland 启动和截图：** 最终二进制以普通用户、`GDK_BACKEND=wayland` 运行约 5 秒，测试计时器使应用正常退出码 0。主窗与 General 设置窗均被捕获，日志无 GTK warning/critical；这是实际 GTK/Wayland 启动和绘制证据，不是键鼠交互或 WM close 验收。主窗 client `337×369`、Canvas `325×357`；设置窗 client `558×335`；GTK `scale_factor=1`。
- **尺寸调查：** 外层环境有 `GDK_SCALE=2`，但 GTK 的 widget scale factor 是 1。分别以 `GDK_SCALE=1` 与 `GDK_SCALE=2` 启动得到同一 factor 和相同 client 尺寸，故 `GDK_SCALE` 没有解释本轮所说“窗口太大”；撤回此前“尺寸翻倍”的推断。GTK 应用捕获不含 compositor 外框，所以完整可见外框仍未测量；未擅自缩小 DLU 派生尺寸，也未调用 Hyprland IPC 或抓取整桌面。
- **本地改动：** 保留此前的紧凑固定窗口、Canvas 内缩修正、分组 disclosure/collapse、分隔线、Usage 百分比、隐藏未映射设置窗捕获保护；修正菜单快捷键宽度按 8 pt 绘制也按 8 pt 测量。最终源码哈希见输出目录 `source-and-binary-sha256.txt`。菜单弹出像素和 collapse 真实点击未在本机 GUI 自动化，本地 Xvfb/Openbox 不存在。
- **证据归档：** 原始报告、PNG、日志、build manifest 位于 AI 产出目录的 `P2B-Real-Wayland-20261028-R4/`。最终 main PNG SHA-256 `68c9f133bc73a38dabd485a3bfb299d45f9cf21e281934e6264e6e55d910241f`，General PNG `ee33df415eacd6e69d448cc269b44be7fd17092ee436bab616e0d494798e1551`，Wayland log `aad95dd5179e0871e91f8079fe658077bd5f1d4bc9ebd330d5f556fca4a2a570`。Windows HWND 与 Linux client 的自然尺寸并排图仅供观察，不是像素对齐差异图。Windows 3.5.2 全页 1:1 继续 NOT PASS；184/184、36/36 等既有 P2C 设置指标不为主窗背书。真实 Wayland 键鼠交互、五页实机切换、HiDPI 和完整外框比较仍 NOT RUN。

## 当前真实状态（2026-10-28；PR #7 main-base CI 已验收）

- **主线及 PR：** `main@83dbf58fb039645f08e08bc89170adcdaa7997be`。PR #7 为 OPEN / Draft、未合并，Base=`main`，Head=`019a7f1e0755570c2c3a0b1202a6c764540f14e2`，Tree=`5cd9e6fe978570bc63fc626e5bccaccf5a43cc5f`，双亲依序为 `b7ea31e6aea121b0b606866704f52ac0ddfe8be8` 和 `83dbf58fb039645f08e08bc89170adcdaa7997be`。GitHub 当前报告 `mergeable=true`。改 Base 没有修改 main、PR HEAD 或提交历史；未标记 Ready、未合并。
- **main-base CI（均针对 PR #7 Head `019a7f1…` 的新运行）：** [Build 40653975796](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975796)、[P2A GUI 40653976301](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653976301)、[P2B production GUI 40653975764](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975764)、[P2C Windows/Linux 40653975755](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755) 全部 SUCCESS。Build 的治理、Shell、安全、Zamak check/test/build 步骤均 PASS。P2A 的 Xvfb/XTEST 与 Weston/Pixman 两条 lane 均 PASS，Canvas 探针 12/12、APT update/install 退出码 0；Weston 仅验生命周期，不声称真实键鼠输入。P2B 的生产 Xvfb 与 Weston 两条 lane 均 PASS；Xvfb 30 项 Zamak tests、C 安全 3/3、Alt+F4/WM close 退出码 0。P2B workflow 内设置页步骤按分支条件跳过；设置页覆盖由 P2C 专用工作流执行，不构成覆盖缺失。
- **真实 Hyprland 启动检查（2026-10-28 早期历史状态，已由上方 R2/R3 复验更新）：** 本机环境为 `XDG_SESSION_TYPE=wayland`、`WAYLAND_DISPLAY=wayland-1`、Hyprland instance 存在，GTK 3.24.52、Zamak 0.2.0。早期以普通用户和 `GDK_BACKEND=wayland` 启动当前 PR #7 构建产物；`ss -xap` 显示进程与 Hyprland 的 `wayland-1` socket 已连接，持续运行约 8 分钟，无 stdout/stderr。随后只对该测试进程发送 SIGTERM；这不是 WM_CLOSE/Alt+F4 验证。该次没有截图或输入；当时记录的 tiled 窗口放大假设已被上方 R2/R3 截图、尺寸和 GTK scale factor 复验取代。
- **本机隔离 GUI 运行限制：** `Xvfb`、`openbox`、`weston` 均未安装。生产 Xvfb/XTEST 脚本和私有 Weston 生命周期脚本尝试后均因首个缺失工具退出码 2；未安装系统包。对应 PR HEAD 的 GitHub Actions 仍为上述 8/8 jobs PASS，不能替代本机实测。治理 33 项、C/Python 安全 3/3、Shell 语法、Zamak 0.2.0 check/test（30/30）/build、生产 GUI 构建本轮均 PASS。
- **P2C 视觉与交互结果：** Windows 原版来源、Linux Xvfb 截图与比较 job 全 PASS。几何 184/184、控件栅格 36/36、导航选中 10/10、树线像素 102/102、中间滚动 2/2；10 组 P2C-2 回归全部改善。深色语义配对 20/20，主题/语言切换顺序比较 10/10，深浅色几何 210 个资源矩形一致。整页像素 1:1 仍 **NOT PASS**。12 项简中来源差异继续 `DECISION_REQUIRED`；PR #7 的 8 条历史 Review threads 已逐条回复并 Resolved，当前 unresolved threads 为 0。完整指标和图片清单位于 [P2C 比较 artifact 12801450123](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755/artifacts/12801450123)，ZIP SHA-256 `9dfe05278ca8bca375ce9630f35d4d3480340aa7fdf368685b6d9deac3fc62d6`；[Windows artifact 12801170097](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755/artifacts/12801170097)，SHA-256 `0ce2c551f8b7081256c8cd76ac9a3568a965bb8e994aca108389ca783f50f201`；[Linux screenshot artifact 12801041004](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40653975755/artifacts/12801041004)，SHA-256 `b7d821bb58af856723b249a547b8c862940d3340be23a12732b626258d664b4a`。
- **阶段边界：** PR #7 尚待独立复审；真实 Wayland/Hyprland 键鼠输入 NOT RUN，托盘与清理确认/结果仍受限，实际清理、Polkit、Swap/ZRAM、root GUI 和活动桌面操作不在验收范围。ADR-0004 仍 PROPOSED。

## 历史快照（2026-10-28；PR #7 集成提交推送前的本地候选审计）

- **主线已核实：** main@83dbf58fb039645f08e08bc89170adcdaa7997be 是 PR #9 文档/记忆 Merge Commit，Tree a10f22817479a9e91052420f40921e8446fd3a24；父提交依次为 c0f616e5b0f3c7511cedcfe7d0b4536e8d54f0f2 和 de19d6aa7bb9eb7cf82fd253b6b6d4e4cff200aa。该 merge 相对第一父提交只改 memory/MEDIUM_TERM.md、memory/SHORT_TERM.md。PR #9 CLOSED/Merged，主线 Push Build [40642082611](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40642082611) SUCCESS。
- **PR #7 远端状态只读复核：** OPEN / Draft，head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`；base 分支仍为 `feat/issue-3-p2b-main-window`，该远端分支当前 HEAD 为 `483ac837cd51d3293baf6c3d8b72309dd02f2ff9`，且已是 main 的祖先。PR API 的 `base_sha=911a3d0b99b05f15bd0109dca475b0b5174e7888` 与旧 head 的共同祖先相符；PR 仍报告 `mergeable=false`。8 条历史 Review 线程均为 outdated / unresolved。本轮没有写入远端。
- **三方结果：** merge-base 为 911a3d0b99b05f15bd0109dca475b0b5174e7888。git merge-tree --write-tree 返回 tree 73ab4bed3a3a9c0d09f0c4b63caf996a007ab030、退出码 1，确证四个内容冲突：.github/workflows/p2b-production-gui.yml、memory/MEDIUM_TERM.md、memory/SESSION_LOG.md、memory/SHORT_TERM.md。.github/workflows/test.yml 自动合并；scripts/ci/apt-diagnostics.sh 两侧 blob SHA 均为 dabb4a04b74c8e9763ccfda7dc1bc170dede8971。
- **本地验证：** 治理 33 项、Python/C 安全 3/3、Shell 语法、Zamak 0.2.0 check/test（30/30）/build、Arch 原生构建、Canvas 探针 check/test/build（12/12）、四个 workflow YAML 解析、对 main 的 diff check 均 PASS。该环境没有 Xvfb、Weston 或 actionlint，故生产 XTEST/Weston 运行与 actionlint 为 NOT RUN；候选未推送，所以新 HEAD GitHub Actions 也 NOT RUN。
- **集成净差异：** 候选相对 PR #7 原 head 有 23 个路径变化；相对 main 有 29 个路径变化、无路径删除。PR #4 探针及 PR #6 主窗口验证脚本保留；`p2b-production-xvfb.sh` 仅调整已测量窗口内容内缩后的 XTEST 点击坐标；`p2b-production-wayland.sh` 在保留主窗生命周期检查的基础上增加私有 Weston 设置窗表面/退出验证，不声称 Wayland 输入；`build-production-gui.sh` 纳入设置 UI 与 GTK bridge 源文件 SHA。采样失败清空旧值、重绘和 `em-dash` 检查仍在。P2C 几何 184/184（最大分量偏差 1.5 px）与局部采样结果不代表整页像素 1:1；该项仍 NOT PASS。真实 Wayland/Hyprland 输入 NOT RUN，托盘与清理确认/结果仍受限，12 项简中来源差异仍 DECISION_REQUIRED。
- **安全边界：** 未执行内存清理、Polkit、Swap/ZRAM、root GUI 或活动 Hyprland 操作；ADR-0004 仍 PROPOSED，不实现偏好持久化。

## 历史快照（2026-10-28；PR #6 合并前的 main-base 验收）

- `main` 为 `b78785af233dbae7a80085fd7a9f45f30ae42711`。PR #6 仍 OPEN / Draft、未合并，Base 为 `main`，Head 为 `36d548d3dedcefffc37dceab13be887a1473187f`；当前 GitHub 报告 `mergeable=true`。Head `36d548d…` 的父提交是此前双亲集成提交 `3b73868287c9ed13d3e8fc25f3b9d6fbe06cdc5a`。
- 记忆修正前 Head `36d548d…` 的 main-base PR 检查通过：[Build 40632783618](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783618)、[P2A GUI 40632783623](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783623)、[P2B GUI 40632783657](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40632783657)。Build 的 Linux job、两条 GUI workflow 的 Xvfb/XTEST 与 Weston Headless/Pixman jobs 均成功；PR 事件针对 `main` 与该 Head。**这些 PASS 只属于 `36d548d…`；记忆修正产生的新 Head 必须独立复验，不能沿用旧结果。** P2A artifacts：X11 `12792291897` SHA-256 `7393d70168a11587a382bfece61a1bb1233c4a14c67e7c0b8c3850988c9ea242`，Weston `12792560638` SHA-256 `57d030b5ad64ebb89c9722e5b431e45caa8b34d32e45ad9c17b521ff30c53119`。P2B artifacts：X11 `12792660966` SHA-256 `8b2a9d099b8992c8901788eeb62ed294619472ef3e536b90dfae43e1684f20d2`，Weston `12791754327` SHA-256 `92063912e303491206bee5ccbb2a672d32578ec401a9868f4371a5cd31059815`。
- 本次复核发现两条尚未解决的记忆状态 Review：[`SHORT_TERM.md` #4396911245](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6#discussion_r4396911245) 与 [`MEDIUM_TERM.md` #4396911255](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6#discussion_r4396911255)。本次只修正状态记录并保留旧阶段历史；线程状态留待独立复审，不在这里代替审查者解决。
- PR #7 仍 OPEN / Draft，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，Base 分支仍指向 PR #6。该 PR 有独立集成冲突且未获本轮修改授权；本轮未改 PR #7。GitHub API 此次仍报告 `mergeable=false`，并返回旧 Base SHA `911a3d0b99b05f15bd0109dca475b0b5174e7888`，需后续单独核实其基线刷新与冲突解决。
- Windows Mem Reduct 3.5.2 整页 1:1 像素验收未通过；真实 Wayland/Hyprland 键鼠输入未验收。实际内存清理、Polkit、Swap/ZRAM 操作不属于本轮验收；相关功能继续保持未验收状态。

## 历史快照（2026-10-28；PR #6 Base 改为 main 前）

> 以下保留本轮授权改 Base 之前的完整状态与测试记录。“尚未改 Base”及 main-base CI 未运行仅描述该时点，不代表当前 GitHub 状态。

- `main` 为 `b78785af233dbae7a80085fd7a9f45f30ae42711`，PR #2、#4、#5 已合并；PR #6 的生产窗口仍未合入 `main`。
- PR #6 仍 OPEN / Draft，Base 仍为 `docs/issue-3-p2a-ui-spec`（该分支 HEAD `a19474ca3f79de65a18e76f6852ab503ce11b173`）。PR #6 已普通快进推送双亲集成 Merge Commit `3b73868287c9ed13d3e8fc25f3b9d6fbe06cdc5a`，Tree `22022b50f6233c2e729b02cf678b426e12af0d21`；父提交按序为原 PR #6 HEAD `911a3d0b99b05f15bd0109dca475b0b5174e7888`、当时 `main` `b78785af233dbae7a80085fd7a9f45f30ae42711`。GitHub 报告 `mergeable=true` **仅针对当前旧 Base**，不能据此视为新 main-base 验收。
- 该集成 Head 的 [GUI Push 40627628308](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627628308)、[GUI PR 40627633632](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627633632)、[Build Push 40627628264](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627628264)、[Build PR 40627633638](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40627633638) 全部 SUCCESS；Xvfb/XTEST、Weston Headless 各两条 GUI lane 通过，APT update/install 诊断 PASS，主项目 Zamak 13/13。PR GUI 事件 Base 仍为旧 P2A 分支；**PR #6 新 main-base CI NOT RUN**。本次文档状态修正的后续 HEAD 与 CI 应另以 GitHub 远端结果核验。
- PR #6 生产 UI 的三条原始缺陷 Review 已有对应证据并解决；2026-10-28 新增的两条记忆状态审查需要在本状态修正提交推送后复核。本次状态修正不改生产 GUI、清理权限或测试逻辑。
- PR #7 OPEN / Draft，HEAD `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，Base 分支指向 PR #6，但旧 HEAD 尚未继承本轮集成历史，GitHub 当前 `mergeable=false/dirty`。新 #6 与旧 #7 两点 Tree 比较有 46 个路径差异（含 10 个表观删除），三点 PR 比较为 30 个 P2C 变更路径；这些不是未来真实三方合并的删除结论。#7 未修改，后续必须独立冲突解决，保护 PR #4 探针与 PR #6 生产 GUI CI。
- P2C-4R1 几何 184/184 PASS、最大分量偏差 1.5 px，**整页 Windows/GTK 1:1 未通过、真实 Wayland/Hyprland 输入 NOT RUN**；托盘、清理确认/结果及特权清理未验收。12 项简中原版来源差异仍为 `DECISION_REQUIRED`；未经另行授权不得改 Base、标记 Ready、合并或修改 PR #7。

## 历史快照（2026-10-28；PR #6 本地集成审计、推送前）

> 以下记录属于当时仅完成本地冲突模拟、尚未推送的阶段；旧 HEAD、旧合并状态与旧 Review 线程信息不代表当前 GitHub 状态，当前状态以上方记录为准。

- `main` 当前为 `b78785af233dbae7a80085fd7a9f45f30ae42711`；PR #2、#4、#5 已合并。PR #5 的原分支 Head 为 `a19474ca3f79de65a18e76f6852ab503ce11b173`，其集成 Merge Commit 已进入 main。
- PR #6 OPEN / Draft，Head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，base 分支 `docs/issue-3-p2a-ui-spec`；远端该分支 Head 为 `a19474ca3f79de65a18e76f6852ab503ce11b173`。GitHub PR 状态报告 `mergeable=false / DIRTY`。本地 merge-tree 与合并模拟在 `docs/UI_FIDELITY_TEST_PLAN.md`、`memory/MEDIUM_TERM.md`、`memory/SHORT_TERM.md`、`memory/SESSION_LOG.md` 发现冲突；只在独立 worktree 形成解决草稿。
- PR #7 OPEN / Draft，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`，base 为 PR #6 Head，GitHub 当前报 CLEAN。任何 #6 Head 更新后必须重新审计 #7 的二点/三点差异。
- PR #6 的最终旧-base GUI/build Actions 为 [40208944364](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208944364) / [40208944120](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208944120)，均 PASS；这不构成 main-base CI。采样失败后清空旧值、重绘及禁用清理的证据见 P2B-2 Review、`src/main.zm` 与 Xvfb/XTEST 脚本；两条结构/菜单旧 Review 仍未解决。
- PR #6 生产 GUI CI 草稿已在独立 worktree 准备：保留 push/旧 base，加入 main 与 `opened/synchronize/reopened/edited`；APT update/install 复用 `apt-diagnostics.sh` 并分步限时，Runner job 60 分钟，Action 固定到 Node 24 版本，artifact 缺失时失败且并发不取消证据作业。尚未提交或推送。
- 本地合并候选验证：治理 29 项、C 安全单测 3 项、Shell 语法、Zamak 0.2.0 check/test/build（13/13）、Arch 原生构建、生产 GUI 构建、Canvas 探针（12/12）、PyYAML 工作流语法和 `git diff --check` 均 PASS。当前环境没有 Xvfb、Openbox、Weston/wayland-info、actionlint；生产 XTEST/Weston 图形运行和 actionlint 校验 NOT RUN，未安装依赖，也未操作活动桌面。以上是本地候选，不是新 GitHub main-base CI。
- P2B/P2C 功能仍不属于 main。P2C-4R1 几何 184/184 PASS（最大偏差 1.5 px），整页 1:1 NOT PASS，真实 Wayland/Hyprland 输入 NOT RUN，托盘/清理确认与结果仍受限；12 项简中来源差异为 `DECISION_REQUIRED`。

## 历史快照（2026-10-28；PR #5 集成前状态）

- main 为 `459415001a7e3538e9496eeb90b8ee75a7378f14`；PR #2、#4 已合并。PR #5 仍 OPEN / Draft，当前旧 Base 为 `fix/issue-1-zamak-build`（`11b0b9444cd94f0026b855abe3849a06106426f2`）。其 P2A 集成 Merge Commit `4996c83103c72484f20a435118711cddc0aea244` 已推送到 `docs/issue-3-p2a-ui-spec`，Tree 为 `3afdf06d4dd3e1375ba7864d78f9572229950cca`；按序双亲是原 PR #5 Head `d89d74fde433da1863b4de0223f974c824fb590e`、main `459415001a7e3538e9496eeb90b8ee75a7378f14`。
- 此快照记录的是 PR #5 改 Base 前状态，不能当作改 Base 后的当前目标。PR #6 仍 OPEN / Draft，Head `911a3d0b99b05f15bd0109dca475b0b5174e7888`，以 PR #5 分支为 Base；GitHub 当前报告 `mergeable=false`，需单独解决与复审。PR #7 仍 OPEN / Draft，Base 为 PR #6，Head `b7ea31e6aea121b0b606866704f52ac0ddfe8be8`。#5/#6/#7 均未合并。
- P2B/P2C 生产 UI、Windows 原生采集与视觉比较仍来自未合并的 #6/#7 分支，不代表当前 main；P2C-4R1 几何为 184/184 PASS，最大偏差 1.5 px；整页 1:1 NOT PASS，Wayland/Hyprland 真实输入 NOT RUN，托盘及清理确认/结果受限。
- 12 项简中来源差异仍为 `DECISION_REQUIRED`；未批准文案改动。
- 本地模拟集成的原始历史和来源 SHA 保留在下方及 `memory/SESSION_LOG.md`；该历史条目描述的是当时的本地模拟，不再代表 PR #5 当前远端状态。
- 已推送集成树的本地验证记录：治理 29 项、C/Python 安全测试 3 项、Shell 语法、根项目 Zamak 0.2.0 check/test（5/5）/build、独立探针 check/test（12/12）/build、`git diff --check` 与敏感内容审计均 PASS。探针源码 SHA-256 为 `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。这些结果来自改 Base 前的 `4996c831…`。


## 历史验收记录：PR #4 main Base 迁移（2026-10-27）
> 本记录描述 #4 合并前的 Base/CI 验收；当前合并状态以文件顶部快照为准。

- 所有者授权后，仅将 PR #4 base 从 `fix/issue-1-zamak-build` 调整为 `main`；没有改 main、rebase、merge、标记 Ready 或处理 PR #5/#6/#7。PR 保持 OPEN / Draft。
- 为保证 Base 编辑触发检查，提交 `ada1d82ef57ab76277dd6b088999e577e4f3a834`，在 `.github/workflows/p2a-r4-isolated-gui.yml` 显式加入 `pull_request.edited`；发现 `test.yml` 仍使用默认活动类型后，提交 `0067b97c6a9a3763af62e1a6990db413bc198820` 为 build-and-test 也加入 `edited`，并借真实 `synchronize` 事件取得最终 head 的完整 PR 检查。
- 在 `0067b97c6a9a3763af62e1a6990db413bc198820` 上，PR Base 为 `main` SHA `86169a1c4c1da0150ff3495290ae165b8c80d49a`；GitHub 为 PR 合成 checkout `3d8927eac872b791ac70e08e2ae182876c56dfca`。main-base GUI run [40501262596](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40501262596) 的 Weston 与 Xvfb job 均 PASS；build run [40501262352](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40501262352) PASS。治理、shell、C 安全、Zamak 5 项单测和 Arch 原生构建均执行通过；探针 check/test/build 通过，12/12 测试通过。
- Weston、Xvfb APT update/install 均有诊断记录且 `apt_exit_code=0`。Xvfb APT install 约 17 分钟后成功，尚无证据确认更深层原因；步骤设置 40 分钟时限。Alt+F4 触发 `CI_WINDOW_CLOSE` 与 `CI_APP_RETURNED`，两客户端退出码均为 0；Xvfb hover/press 与 press/release ROI 差各 8004 像素，语言切换差 4697，返回英文后差 0。
- 本次 Wayland artifact ID `12733767416`，GitHub digest/独立 ZIP SHA-256 `cf6b5fcd1740df07996fa3756f431788d5f086241d3ba6f95c7e155f6c21dab9`；X11 artifact ID `12734377907`，GitHub digest/独立 ZIP SHA-256 `d9aa083317660cba7c29212fb6ebd1d62ac8199f1d1197bc54d7743fd3bb1b7f`。两包中的探针二进制 SHA-256 均为 `9f7cb6db085fc3b247a62203b2e7f60d994620639c7dc5d51997c6edd9204ce6`，源文件哈希核验通过。
- 当前 PR #4 改动仍是隔离探针、报告/记忆及两个 CI 工作流；没有生产 UI 或特权功能改动。一个 inline review thread 仍未解决，内容是确保 Base 编辑触发检查；本轮已通过显式 `edited` 和 main-base PR CI 提供证据，留待 ChatGPT 正式复审。
- 本记录对应已完整验收的 `0067b97…`。短期记忆随后单独提交时，必须对新 HEAD 重新运行 CI；不得将上述成功结果冒称为该后续 HEAD 的检查。


## 历史快照：P2A-R7 规格修正及 P2B-1 开发（2026-10-25）
> 以下内容是 2026-10-25 的规格/开发阶段记录；其中 PR 状态与截图缺口不代表当前状态。

### 开工核验与纠正
- PR #2、PR #4、PR #5 均为 OPEN / Draft；PR #5 head `6b3862b0e6d105f42dbf8d2d6c679cdfb182ad16`，base `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #4 最终 head `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1`。没有合并任何 PR，未修改 main、P1 或 PR #4。
- PR #4 最终 GUI PR Actions `40192531809`、build PR Actions `40192531948` 已在规格文档中刷新；同 SHA push runs 为 `40192528601`、`40192528611`。
- 尺寸约束更正：`IDD_MAIN=224×214 DLU` 是主窗模板；每个 `IDD_SETTINGS_* =260×168 DLU` 是设置子页面模板，不能当作含导航、底部按钮和边框的完整设置窗口尺寸。P2B 可开发参数集中、明确未校准的紧凑候选，真实 Windows 像素验收独立保持 BLOCKED。

### 本轮开发状态
- 在规格 PR 分支更新了最终 CI 证据、DLU 页面/外框说明及 P2B 候选与最终像素验收的门槛区分。
- P2B-1 将从修正规格分支新建独立功能分支，实现生产 `src/main.zm` UI；PR #4 的 12 项探针测试保持原样。最终构建、隔离生产 GUI CI、截图哈希和各控件状态待实施后记录。
- 不在主机启动 GUI，不连接活动 Hyprland IPC，不执行清理、Polkit、Swap 或 ZRAM 操作。Windows 原版真实截图、DLU 实测换算及 2 px 对照仍 BLOCKED / NOT RUN。

## 历史记录：P2B-2 原版主窗口结构与数据可靠性修复（2026-10-25）

- 按用户任务核对 PR #6 最新 ChatGPT Review 与 3 条行内评论。PR #6 保持 OPEN / Draft；base `docs/issue-3-p2a-ui-spec` SHA `d89d74fde433da1863b4de0223f974c824fb590e`。修复后代码 HEAD `e4fccb6293c698015c8f4105947669bfdc4693d6` 与远端一致；未修改或合并 main、PR #2/#4/#5。
- 生产主窗口改回三个纵向分组，每组各自显示 Usage、Available、Total available 标签和值；简体中文 Pagefile/Swap 标题为“交换空间”。System working set 保持 `—`，说明放在状态区。DLU/控件/弹窗几何集中于 `src/ui_geometry.zm`，仍为 `PROVISIONAL`，不是 Windows 实测像素目标。
- 菜单弹窗矩形按 Canvas 视口翻转/约束，根菜单、英语/简中语言子菜单和清理菜单均有 `fits=true` 日志。必需 meminfo 字段缺失、非法、负数或关系不一致时采样失败；真实 SwapTotal=0 有效；失败会清空旧值并请求重绘。保留/新增 Zamak parser 与几何测试，13/13。
- 精确 HEAD `e4fccb6...` 的 GitHub Actions：生产 GUI run [40208121223](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121223) 两项作业 Xvfb/XTEST、Weston Headless/Pixman 均 PASS；build/test run [40208121224](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121224) PASS。X11 artifact `12604402691`，Wayland artifact `12604102361`。源码、二进制、截图 SHA-256 和精确映射记录在 `docs/P2B-1_PRODUCTION_UI_REPORT.md`。
- Xvfb 中实际生产窗口为 337×369，Canvas 313×345，scale 1；英语、简中三组布局及各菜单通过，边界都在 Canvas 内；鼠标、键盘、Esc 焦点恢复、WM close 通过，进程退出码 0。采样失败注入使旧值失效并显示 em dash；清理按钮和选项记录 `action=none`，未触发特权调用。
- Weston 私有 Headless/Pixman socket 下生产客户端连接、绘制、读取真实 `/proc/meminfo`、从 `app.run` 返回并以 0 退出。仍保留 1 条 `gdk_seat_get_keyboard` critical、无 `wl_seat` 广播、受控清理时 compositor layer BUG；因果未确定。未操作活动 Hyprland/桌面，未在本机启动 GUI。
- 本轮既有本机验证结果：项目检查 29 项、Python/C 单测 3 项、shell 语法、Zamak 0.2.0 check、test 13/13、build、Arch build、`git diff --check` PASS。没有执行清理/Polkit/内核写入、Swap/ZRAM 修改或 root GUI。
- Windows 3.5.2 原生截图、真实字体/DPI DLU 换算和 2 px overlay 继续 `BLOCKED / NOT RUN`。设置页、托盘等不在本轮实现。最新 Linux 英文/中文实际生产截图与来源 SHA 记录在测试报告；尚无原生 Windows 截图，不能宣称像素级或 1:1 验收。

## 历史记录：P2B-1 生产主窗口候选与 CI 复测（2026-10-25）

- PR #5 的 R7 规格修正 head `d89d74fde433da1863b4de0223f974c824fb590e` 已核对，仍 OPEN / Draft，CI 2/2 PASS；正式 `IDD_MAIN=224×214 DLU` 为主窗口模板，每个 `IDD_SETTINGS_* =260×168 DLU` 是子页模板。R6 Actions `40192531809` / `40192531948` 与对应 push runs `40192528601` / `40192528611` 已记录。没有合并 PR #2/#4/#5。
- P2B 独立分支 `feat/issue-3-p2b-main-window` 从 PR #5 修正规格分支派生；PR #6 OPEN / Draft，base 为 `docs/issue-3-p2a-ui-spec`。生产代码 head `a6df90389a69fc9cad12272c2daa2f273c834385` 通过 PR GUI run `40199653180`、build-and-test run `40199653440`；push GUI/build runs `40199649320` / `40199649284` 也通过。远端分支 SHA 已核对。
- 生产 `src/main.zm` 以真实 `/proc/meminfo` 显示 RAM/Swap，System working set 显示不可用；英文/简中独立切换；清理按钮和八项 Windows 清理选择全部禁用，不触发 Polkit 或内核写操作。DLU 转换参数集中在 `src/ui_geometry.zm`，仍为未校准估算。
- Xvfb/XTEST 实测正式窗口：top-level 337×369、Canvas 313×345，scale 1；点击语言菜单、split button、禁用区域、Tab/方向键/Enter/Escape 与 Alt+F4 close 均通过，退出码 0。实际截图及 SHA-256 在 `docs/P2B-1_PRODUCTION_UI_REPORT.md` 和 artifact `12601156413`。
- Weston 13.0.0 Headless/Pixman 私有 Wayland socket 下正式客户端连接、绘制、读 meminfo、退出码 0；不测试 Wayland 输入。诊断保留：一次 `gdk_seat_get_keyboard` critical、`wl_seat` 未公告；Weston SIGTERM 清理日志含 `BUG: finalizing a layer with views still on it.`，尚未确认为应用故障，不隐藏日志。
- 本机 `verify_project.py` 29 项、C 安全单测 3 项、shell syntax、Zamak 0.2.0 check/test/build（7/7）、`scripts/build-arch.sh`、`git diff --check` 均 PASS。没有在主机启动 GUI、访问活动 Hyprland IPC、操作用户桌面或执行特权清理。
- Windows 原生 v3.5.2 截图、真实字体/DPI DLU 校准、配对 overlay/2 px 验收仍 BLOCKED / NOT RUN；Hyprland 鼠标/键盘、HiDPI、设置页与托盘未实现。PR #4 保留独立 12/12 探针测试，未并入 PR #6。

## 历史记录：P2A-R6 规格入库与隔离 GUI CI（2026-10-25）

## 本轮基线与结果
- 文档分支 `docs/issue-3-p2a-ui-spec` 独立基于 P1 `11b0b9444cd94f0026b855abe3849a06106426f2`，不包含 PR #4 测试代码；已创建 [Draft PR #5](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/5)，base `fix/issue-1-zamak-build`，引用 Issue #3。PR 创建时 head `b94731b9a9bc52a4834358f9f71fcf86a2033925`；本次最后的记忆同步提交会推进 head，最终 SHA 以 PR 远端核验为准。
- PR #2 仍 OPEN / Draft，head `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #4 仍 OPEN / Draft，base `fix/issue-1-zamak-build`，最终本轮 head `e2d3c5e4481e9494472913f41636580369038cd8`，远端 SHA 已核对一致。
- R6 GUI Actions push [40191454446](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454446) 与 PR [40191456330](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456330) 均 PASS；build-and-test push [40191454361](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454361) 与 PR [40191456339](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456339) 均 PASS。Artifacts 已下载核验；含 X11/Wayland 日志、PNG、协议/几何状态和 SHA 清单。
- 9 项既有 Zamak 事件路由测试函数原样保留，新增 3 项紧凑几何/双语用例；Zamak 0.2.0 共 12/12 PASS。探针源码 SHA-256：`d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。
- Xvfb + Openbox 下实际 XTEST 验证 hover/pressed/release、按钮及箭头命中、菜单、Tab/Enter/Escape、双语和 resize；同按钮 ROI hover→pressed、pressed→release 差异均为 8004 像素。WM close 通过 Alt+F4 传递 WM_DELETE，记录 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`、退出码 0 且窗口消失。`xdotool windowclose` 实际用 `XDestroyWindow` 销毁 X 窗口，不作为 WM close 证据。
- Weston Headless/Pixman Wayland socket、312×324 Canvas 绘制、受控退出及进程退出码 0 均 PASS。每个运行记录一次 `gdk_seat_get_keyboard` critical，未公告 `wl_seat`；两者相符但因果未证实，未隐藏 GTK 诊断。

## 文档与尺寸约束
- 已整理 P2A 源码规格、互动矩阵、Linux 能力映射、版本差异、可行性及真实 Windows 截图采集清单；官方基准固定 `v.3.5.2` / `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 新增明确尺寸规则：主对话框 224×214 DLU、五个设置页模板各 260×168 DLU；生产 GUI 必须保持官方紧凑比例，不得放大。R5/R6 336×348 顶层、312×324 Canvas 仅为 96-DPI 估算的 CI 探针尺寸，不是 Windows 测量或产品目标。
- 官方 Windows 原生截图及真实 DLU→物理像素换算仍 BLOCKED；overlay/diff/2px 结果 NOT RUN。336×348 top-level / 312×324 Canvas 仅为 CI 探针尺寸。没有新增或公开私人截图/路径。

## 本机只读/非 GUI 验证
- `python3 scripts/verify_project.py` PASS（29 项）；`python3 -m unittest discover -s tests -v` PASS（3 项 C 安全测试）。
- 所有 `scripts/*.sh` 语法检查 PASS；Zamak `0.2.0`、`zam check .`、`zam test .`（5 passed）、`zam build .`、`bash scripts/build-arch.sh`、`git diff --check` PASS。
- 未启动 GUI/Compositor，未使用 Hyprland IPC、鼠标键盘注入或特权清理接口；未触碰 Swap/ZRAM/Polkit。
- 规格分支更新官方 3.5.2 源码规格、资源/交互矩阵、Linux 语义、安全边界、真实 Windows 截图缺口、Zamak 技术结论及紧凑尺寸约束；长期规则写入 AGENTS/PRD/ARCHITECTURE/LONG_TERM。PR #5 创建时的源码规格 head `90d62100e840aa408e4ea99cbbd866ef2025e99e` 上，build-and-test push `40191896559` 与 PR `40191901022` 均 PASS。记忆同步提交会使 head 前进，但不改程序源码；最终 branch SHA 以远端校验为准。Windows release 截图和真实像素比较仍 BLOCKED。
- 未启动本机 GUI 或 compositor，未访问 Hyprland IPC，未执行 Polkit、内存清理、Swap/ZRAM 操作。PR #2/#4/#5 不合并；P2B 不开始。下一步等待 PR #5 Actions 完成并交付两个 Draft PR 供复审。


## 历史记录：P2A-R6 隔离 GUI CI 与规格提交（2026-10-25）

- 规格分支 `docs/issue-3-p2a-ui-spec` 基于 P1 `11b0b9444cd94f0026b855abe3849a06106426f2`；已提交源码级规格首版 `367efce8dcdecb487595507ff7e9f275bedd96bc`，仍待更新最终 CI 记录和创建独立 Draft PR。
- 测试分支 `test/issue-3-isolated-gui` 的 R6 验证源码 SHA `1cb7b77246275962f092fa0c0ccc27fbf982ce8b`。PR #4 为 OPEN / Draft，base P1 分支；PR #2 为 OPEN / Draft。没有改 main、P1 功能或生产 UI。
- 保留原 9 个 Zamak 测试函数，新增 3 项紧凑几何/双语测试；Zamak 0.2.0 `check/test/build` PASS，12/12 probe tests PASS。探针 SHA `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。
- X11 修正：`xdotool windowclose` 的上游实现是 `XDestroyWindow`，不发 WM_DELETE，不能用于正常 GTK 关闭验收；调用它导致异常销毁和 GTK critical。隔离 Xvfb 下启动 Openbox 后，实际通过 XTEST Alt+F4 验证 WM close：`WM_PROTOCOLS` 含 `WM_DELETE_WINDOW`，日志顺序包含 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`，退出码 0 且 X 窗口消失。Openbox 运行后 `xdotool windowquit` 尝试仍未触发回调，未将其标为通过。hover-only 与 held-press ROI、pressed 与 release ROI 像素差各为 8004。
- R6 Push/PR CI：GUI runs [40191087455](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191087455) / [40191091618](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191091618) PASS；build runs [40191087528](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191087528) / [40191091733](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191091733) PASS。Artifacts 附有截图、ROI 差异、源码/二进制 hash、协议状态和日志。
- Weston 13 Headless/Pixman：Wayland socket、Canvas 312×324 绘制、事件循环返回、退出码 0 PASS；没有广告 `wl_seat`，每次记录到一次 `gdk_seat_get_keyboard` critical。缺失输入 seat 是相符解释但非已证因果；没有屏蔽 GTK 诊断。
- 文档尺寸硬约束已明示：Mem Reduct 3.5.2 主窗 224×214 DLU；五个设置页模板各 260×168 DLU。生产 GUI 必须维持原版官方紧凑尺寸，禁止放大；CI 336×348 仅为探针估值。真实 Windows 截图、DLU→物理像素实测、Hyprland 输入、HiDPI 和 2px overlay 均 BLOCKED / NOT RUN。
- 本机非 GUI 检查：`verify_project.py` 29 项、C 测试 3 项、4 个 shell 脚本语法、Zamak 0.2.0 check、5 项项目 tests/build、Arch helper build、`git diff --check` 均 PASS。未操作本机 GUI/Hyprland IPC，未执行 Polkit、drop_caches、Swap/ZRAM 操作。
- 下一步：更新规格分支记忆/文档，推送并创建独立 Issue #3 Draft PR；保留 PR #4 Draft，等待 ChatGPT 复审；不合并 PR #2/#4，不进入 P2B。
- 纠正文档/诊断后的 PR #4 head `6ad0f401d2b831ee71a1387a858e7456bbe444e5` 再次通过全部 CI：GUI push/PR `40192348460` / `40192351634`，build push/PR `40192348489` / `40192351591`。Artifacts 重新上传；probe SHA 未变。本轮没有生产 UI 或特权代码改动。

## 历史记录：P2A-R5 尺寸修正与隔离 GUI CI（2026-10-25）

- 工作分支 `test/issue-3-isolated-gui`，P1 基线 `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #2 仍 OPEN / Draft、未合并。PR #4 仍 OPEN / Draft，base 为 `fix/issue-1-zamak-build`。
- 用户要求 GUI 按官方紧凑尺寸，不要放大。核对 Mem Reduct v.3.5.2 tag 源码 `2f5c55d15f69a2c88859e036b16c6f4c971e1968`：`IDD_MAIN` 为 224×214 Dialog Units，字体 `8 pt Ms Shell Dlg`。以 96 DPI 常见 6×13 px base-unit 估算，测试窗口请求 336×348；GTK Canvas 实测 312×324。该换算尚未在真实 Windows 上测量，也不是像素级基准。
- 仅收紧无特权 Canvas 探针窗口、修正 GTK Tab/Enter 字符事件名称并缩短占位文案；没有改生产 UI、P1 构建链路或特权功能。9 项已有路由测试函数体保持不变。
- 本机：`python3 scripts/verify_project.py` PASS（29 项）；C unittest PASS（3 项）；7 个 shell 脚本 `bash -n` PASS；Zamak 0.2.0 probe check/test/build PASS；9/9 probe 测试 PASS；源哈希验证和 `git diff --check` PASS。没有在本机启动 GUI、触碰活动桌面、安装依赖或执行内存/Swap/ZRAM操作。
- GitHub：测试源 SHA `6e0a4a79d56633603a298e5e0ef6520028c71b80` 的 push run [40187178574](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40187178574) 与 PR run [40187181722](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40187181722) 均 PASS；Wayland Headless/Pixman 与 Xvfb/XTEST 两个 job 均通过，截图、版本、日志和测试记录已作为 artifact 上传。
- 仍未完成：真实 Windows 3.5.2 截图与像素对照、Hyprland 实机输入验收、正式生产 UI 复刻。P2A 尚不可宣称正式验收；等待 ChatGPT 复审 PR #4，不进入 P2B、不合并任何 PR。

---

## 历史记录：P1 Zamak 0.2.0 编译与原生构建验收（2026-10-25）


## 执行范围
按 Issue #1 修复 Zamak 0.2.0 编译/构建问题，保留现有 GUI 与特权边界；运行本机 Arch/Omarchy 验收，准备关联 Issue 的 Draft PR。工作分支 `fix/issue-1-zamak-build`，基于 `origin/main` / `080417e153d91bf2adac756e5154311fef49333f`。未修改 main，未扩展功能。

## 实际改动
- `zam.toml`：Zamak `project.name` 从非法连字符形式改为合法标识符 `memreduct_zamak`；产品显示名、仓库名、安装命令和 helper 仍保留 `memreduct-zamak`。
- `src/memory.zm`：将不存在的无参 `split()` 调整为 Zamak 0.2.0 支持的 `words()`，并正确传播解析错误。
- `scripts/install-arch.sh`：按 `memreduct_zamak-<hex-hash>` 匹配 GUI 二进制，避免把 `-test-` 测试产物当作应用。
- `scripts/verify_project.py`：将 manifest 标识符检查同步为 `memreduct_zamak`。

## 本机验证证据
环境：Omarchy 4.0.6、Hyprland 0.56.2、Wayland、GTK 3.24.52、Zamak 0.2.0。

| 命令 / 验收 | 状态与实际结果 |
|---|---|
| `python3 scripts/verify_project.py` | PASS，29 项检查。 |
| `python3 -m unittest discover -s tests -v` | PASS，3 项 C 安全测试。 |
| `bash -n`，逐个检查 `scripts/*.sh` | PASS，4 个脚本。 |
| `zam --version` | PASS，`0.2.0`。 |
| `zam check .` | PASS，`CHECK OK memreduct_zamak`。 |
| `zam test .` | PASS，5 passed，0 failed。 |
| `zam build .` | PASS，输出 `build/memreduct_zamak-00ee9d44b77125b6721133332f35be3f1f9d3d518e7ab20b40d8265940928d57`。 |
| `bash scripts/build-arch.sh` | PASS；Zamak 检查/测试/构建及 C Helper 均成功。 |
| Helper `--dry-run` | PASS；未修改内核状态。 |
| Hyprland GUI smoke test | PASS；普通用户 Wayland 窗口，中文/内存数据/控件/刷新可见，`xwayland=false`，正常关闭，退出码 0。未点击清理、未启用自动清理。 |

安装器候选扫描只匹配 GUI 产物；之前的动态库检查未发现缺失库。未执行涉及 `/usr` 的安装/卸载；Polkit E2E、真实 `drop_caches`、Swap/ZRAM 操作均未运行。无任何真实内核清理写入。

## GitHub / 下一步
- 当前已验证 `gh` 账号为目标 `MilchstraBe-Endless10`；目标仓库仅为 `MilchstraBe-Endless10/memreduct-linux-zamak`。
- 代码提交 `1d4654867f59154981010007288cff1fda42f457` 使用 GitHub noreply 邮箱；文档验收记录后续提交 `9b5177e992eb1c86b72643a0ae2c0cda9a570555`。记录该状态时，`git ls-remote` 与本地 HEAD 相同，工作树干净。
- Draft PR #2：<https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/2>，base `main`，该次核验的 head 为 `9b5177e992eb1c86b72643a0ae2c0cda9a570555`，正文 `Refs #1`。PR 仍为 OPEN / Draft。
- Actions `build-and-test` 对代码提交成功：push run [40170617141](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170617141)，pull_request run [40170634633](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170634633)。只含文档记录的后续提交也成功：push run [40170749833](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170749833)，pull_request run [40170752581](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170752581)。
- 等待 ChatGPT 复审及用户决定是否合并；不得自动合并或进入 P2。

## PR #7 P2C 分支历史快照（来源 head b7ea31e6；记录于 2026-10-27）

- 开始本轮时分支与 `origin/feat/issue-3-p2c-settings-ui` 同为 `663a9d3e627ccb2d9bcb884cd708f140006f58a5`；PR #7 OPEN / Draft，base #6 `911a3d0b99b05f15bd0109dca475b0b5174e7888`。本轮待提交内容是集成报告时间/文件数更正及记忆补录，未改生产 UI、测试源码或 workflow。
- PR #7 当前 checks 7/7 PASS。P2C run [40463959137](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463959137) 原版采集、生产 Xvfb、比较 3/3 PASS；P2B run [40463959086](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463959086) 最终 Xvfb/Weston 2/2 PASS。Weston 首次 attempt 因 15 秒内无有界退出标记超时，失败 artifact `12716375114` 保留；只重跑失败 job 后 artifact `12716440984` 对应成功 attempt 2，根因仍未确认、无 compositor 崩溃证据。构建 PR/push runs `40463959092` / `40463951002` PASS。Artifact IDs、digests 及视觉指标详见集成报告。
- 该 HEAD 的几何 184/184（最大偏差 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36、滚动比较 2/2；Windows 四组主题/语言实验 4/4、页面状态 20/20、设置窗重开 4/4。整页像素 1:1 NOT PASS，Wayland 输入 NOT RUN，Windows tray/清理弹窗 BLOCKED，12 项中文来源差异仍 `DECISION_REQUIRED`。
- PR #2/#4/#5/#6/#7 均 OPEN / Draft，远端 head/base 与集成报告表格一致；无 PR 合并、main 或其他分支修改。P3A Issue [#8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8) 已建；ADR-0004 为 PROPOSED、未批准且未实现。实际仍无清理、Polkit、Swap/ZRAM、活动 Hyprland 或主机桌面操作。
- 本轮本机验证：`verify_project.py` 33 项 PASS；C/Python 安全测试 3/3 PASS；Shell 语法 PASS；Zamak 0.2.0 `check` PASS、`test` 30/30、`build` PASS。仅文档和记忆有改动，未执行本机 GUI、Arch 安装构建或特权测试。

## P2C-EXIT / P3A-PRE 当前交接（2026-10-27）

- 当前工作分支 `feat/issue-3-p2c-settings-ui`；P2C-4R1 已复审 HEAD `556a727c100aad4af1a25c48c10513c4dd42c5c7`，生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`，图形采集/比较源码 checkpoint `37d07453d60c4d1046f5aad6577c9425c271180a`。P2C-EXIT/P3A-pre 首次文档提交 `b1f2fd95333a26af1c3ea51e6fea4a245db640c9` 的 exact-head CI 已完成；本条目随后的提交只补充该运行证据与术语更正，不改生产或测试代码。
- b1f2fd9 P2C run `40461922846` 3/3、P2B `40461922577` 2/2、PR build `40461922736` 与 push `40461917796` 全 PASS（PR checks 7/7）。Artifacts：Windows `12714602648`、Linux Xvfb `12714528369`、visual `12714648221`、P2B X11 `12715275894`、Weston `12714144687`；digest 见 P2C report。几何 184/184、raster 36/36、nav 10/10、tree 102/102、滚动 2/2 compare-only；P2C2 回归 10/10 improved，20/20 深色页面状态配对有效。整页 1:1 NOT PASS。
- 四组 Windows 新进程实验 4/4、页面语义 20/20、设置窗重开 4/4 PASS。几何 184/184（最大 1.5 px）、导航 10/10、树线 102/102、栅格 36/36 PASS；滚动状态 2/2 compare-only。深色 changed-pixel EN 5.9434%–12.6625%、简中 5.4004%–12.4014%；整页 1:1 NOT PASS。Wayland 输入 NOT RUN，Windows tray/清理对话框 BLOCKED，12 项中文差异 DECISION_REQUIRED。
- 新增文档：`docs/PR_INTEGRATION_READINESS.md`、`docs/ADR/0004-user-preferences-persistence.md`（PROPOSED）、P2C-4 报告归档与 UI 交互矩阵 P3A 状态；long-term 仅更新已接受证据 snapshot，未把 ADR 草案升格为永久架构决策。
- PR #2/#4/#5/#6/#7 全部 OPEN / Draft，没有合并。只读 merge-tree 发现 #4 与 P2A/P2B/P2C 的 `memory/SESSION_LOG.md`、`memory/SHORT_TERM.md` 冲突，集成必须人工合并保留双方记录。PR #4 的 12 项 probe tests 与生产 Xvfb/Weston tests 是不同能力，二者都要保留。
- P3A 只规划语言和应用主题的用户级持久化；跟踪 Issue [#8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)。目标 XDG 配置、schema 1、atomic/no-follow/0600 策略、Reset 事务与临时目录测试均见 ADR 草案。实现尚未开始；清理、Polkit、自动清理、Swap/ZRAM 仍禁用。
- 创建并核验 P3A Issue [#8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)，PR #7 顶层交接评论为 [issuecomment-6435453708](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#issuecomment-6435453708)。没有执行真实清理、特权写入、用户桌面/Hyprland IPC、root GUI 或远端合并。本轮未运行本机项目测试；仅做文档 diff 检查，并使用上述 GitHub-hosted Actions。P3A ADR 仍 PROPOSED，等待 ChatGPT/用户架构复审。
## P2C-4 前序图形证据 snapshot（历史；2026-10-27）

- PR #7 OPEN / Draft；base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；当前截图/比较 evidence head `4f84fb9a9e247a5987a11cd435e5da4b05f78a53`。生产 UI 源码 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b` 未改；当时最新正式 ChatGPT Review 为 P2C-3R1；之后的 P2C-4 review 及 P2C-4R1 状态见本文件开头。
- P2C run [40425954672](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425954672) 3/3 PASS；Windows artifact `12699069397` (`sha256:b9ce68670d7137d24923e64532d5a301d99e743fc423c6935165d5ad8181e1fa`)、Linux X11 `12700585092` (`sha256:e69b53bcf64021e8f0b2f0d70b10fe22ac24e9c00ed46efd32799b1c7264ab53`)、visual/dark/font `12700001976` (`sha256:209f4664b942017097bd17d5734d98b4983a58de3561ddab01739fc692e08bf7`)；APT diagnostics `12698999880` / `12700196476`。
- P2B run [40425954460](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425954460) 2/2 PASS；X11 artifact `12699671782` (`sha256:f1b1dc344fba5de07f30bcde18689694b286dc8d9690c1ffcea8c718229ad305`)、Weston `12700250527` (`sha256:6233aae01f776ac2a03da09c7129fb9b9036bad01e51802ad511a2b593575595`)；APT diagnostics `12700215495` / `12699741667`。PR build `40425954317`、push build `40425944842` PASS；PR checks 7/7。
- Manifest/hash 核验：Windows PNG 41/41、Linux PNG 44/44、comparison 83/83、dark 70/70、P2C source 10/10、P2B X11 PNG 10/10、P2B source/binary 8/8。资源几何 184/184（最大分量 1.5 px）、nav 10/10、tree 102/102、raster 36/36、scroll 2/2 PASS；light changed-pixel 5.0741%–14.8853%，dark 5.9434%–73.2932%，整页 1:1 NOT PASS。
- 文本度量 132 行中，120 项同源文本有 9 项宽高差≤2 px、111 项仍不同，另 12 项来源差异保持 `DECISION_REQUIRED`。Liberation Sans 隔离实验：英文 66 个样本改善 64、退化 1、不变 1；简中 54 项改善 12、退化 12、不变 30，未采用到生产。Windows 专有字体没有打包。
- Ubuntu 24.04.5 / APT 2.8.3 的本次 update/install 均成功：P2C Xvfb 101 MB/13s，P2B Xvfb 101 MB/7s，Weston 79.6 MB/25s，Pillow 503 kB/<1s。历史慢速下载原因未确认。Weston 主窗和设置窗客户端退出码 0；保留 seat 与 layer teardown 诊断。Headless 输入 NOT RUN，P2B 重复五页步骤跳过由 P2C 覆盖。
- Windows tray、没有安全采集路径的清理 popup/确认/结果仍 BLOCKED。没有真实清理、Polkit、Swap/ZRAM、root GUI、Hyprland IPC 或主机桌面输入。完整证据与结果见 `docs/P2C-4_FONT_THEME_MENU_REPORT.md`。

### 前一个完整截图/视觉 artifact snapshot（105d71d，2026-10-27）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；以下 screenshot/visual evidence snapshot 使用 head `105d71d567d7ba096e8b928c072ceb4af15bfe6f`，生产 UI checkpoint 为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。之后本分支只同步报告/矩阵/记忆，未改字体、字号、Canvas 或工作流阈值；当前 PR check 状态以 PR 页面显示为准。
- P2C run [40416882722](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416882722) Windows native、Linux production Xvfb/XTEST、visual compare 3/3 PASS；Windows artifact `12695303623` (`sha256:db33cc90dcd9aa0b8b9678adfe44fed0d6ff6733ee4ae6087603b94bc662e7e7`)，Linux `12694879677` (`sha256:424d9d863a3c6a7d2c2f66224150f58e34aec9b62a4f6196131f0c5b4048a782`)，visual/dark/font `12694609240` (`sha256:5b3e9348af249a02ceb7871d76d4d5cb7f7d90e5647dfc7456e5d2726e3caadc`)；APT diagnostics `12695029426` / `12694434533`。
- P2B run [40416882708](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416882708) production Xvfb/XTEST + Weston Headless/Pixman lifecycle 2/2 PASS；X11 artifact `12696373914` (`sha256:bf70d1f1b7a5fba60ed9ef175071009c12c411f887399e38ab5841ac2c04a9d3`)，Wayland lifecycle artifact `12695762656` (`sha256:dbe4352648a14e4660b2edf518ede8bc1406aec720a4b6a2dd92aeda72d23adb`)。重复五页设置 step 条件跳过，由同 head P2C 完整执行。PR build `40416882739`、push build `40416873128` PASS；PR checks 7/7。
- P2C artifact 核验：Windows PNG 41/41、Linux PNG 44/44、visual 83/83、dark 70/70、P2C source 10/10；P2B X11 PNG 10/10、源码六项及生产/helper 二进制两项 SHA 均匹配。几何 184/184（最大分量偏差 1.5 px）、导航 10/10、tree 102/102、raster 36/36、滚动 2/2 PASS；亮色 changed-pixel 5.0741%–14.8853%，暗色 5.9434%–73.2932%，整页 1:1 NOT PASS。
- APT 诊断确认 Ubuntu 24.04.5 runner image `ubuntu24 20261023.334.3`。P2B Xvfb update 获取 12.6 MB / 1s；安装 101 MB / 36m11s（工作流 step 36m23s），其中 `fonts-noto-cjk` 61.2 MB，来自 `azure.archive.ubuntu.com`，退出码 0。日志没有 DNS、锁或缺包错误；慢速事实已确认，根因仍未确认。
- 字体候选未进生产；Windows 专有字体未打包。12 项中文来源差异仍 `DECISION_REQUIRED`；Windows tray、无安全采集路径的 clean popup/confirmation/result 仍 `BLOCKED`。Weston 只验生命周期，Wayland 输入 `NOT RUN`；无真实清理、Polkit、Swap/ZRAM、root GUI、Hyprland IPC 或本机桌面输入。

### 前一 exact-head 核对（33367f3）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，核验 head `33367f3c15273f3e817bba27e9272a19663ea829`；生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。本轮未改生产字体/字号/UI 源码或比较阈值。
- P2C run [40408926862](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408926862) 的 Windows、Linux production Xvfb/XTEST、比较 jobs 3/3 PASS；artifacts Windows `12691828080` (`sha256:ca0e98ec99ce5332956c037271fb91fbcb7cb4ac11fa086e050eedabaf6b4542`)，Linux `12692965976` (`sha256:9171f0ca79938f360cc36b4937744f65c1f4b42b874ed7ec7244be27c4c88f3f`)，visual/dark/font `12692891146` (`sha256:115c23d9d3d1d90cce265cab20a68176d5fbd104bd76e59728cb59a0d1b186bd`)。
- P2B run [40408926842](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408926842) Xvfb/XTEST 和 Weston Headless/Pixman 2/2 PASS；X11 artifact `12694248993` (`sha256:5f14c003ca627693f928e1d54384afeafa683cb66ceba14f7ab6728327cdf7ac`)，Wayland artifact `12690409508` (`sha256:58cf6b8da7c450ad0374892282d444e92869a1f042f0810d6ae1efcc0ba305d9`)。P2B 五页重复 step 条件 `SKIPPED`，由 P2C 同 head 覆盖。PR build `40408926778`、push build `40408920155` PASS；PR checks 7/7。
- 首次 Xvfb APT update 成功；install 于 `fonts-noto-cjk` 61.2 MB 下载阶段触发 38 分钟硬超时，exit `124` / `command timeout`。未见 DNS、锁或缺包证据，底层网络/镜像原因未确认。只重跑失败 job 后安装 exit `0`。attempt 1/2 diagnostics artifacts `12693989271` / `12693778915`；本次成功不证明长期网络稳定。
- 哈希覆盖核验：Windows PNG 41/41、Linux PNG 44/44、visual 83/83、dark 70/70、P2C source 10/10、P2B source/binary 8/8。主程序/helper SHA-256：`fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404` / `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。几何 184/184（max 1.5 px）、导航 10/10、tree 102/102、raster 36/36、scroll 2/2 PASS；light changed-pixel 5.0741%–14.8853%，dark 5.9434%–73.2932%，full-page pixel identity NOT PASS。
- Windows 官方 v3.5.2 provenance 已验证；Linux XTEST 覆盖五页双语及菜单，清理仍禁用。Liberation Sans 未进入生产；Weston 仅验收隔离生命周期，保留 GTK seat critical，Wayland 输入 NOT RUN。12 项语言差异 `DECISION_REQUIRED`；Windows tray 与安全条件不满足的 clean popup/confirmation/result `BLOCKED`。没有真实清理、Polkit、Swap/ZRAM、root GUI、活动 Hyprland 或本机桌面输入。详见 [P2C-4 report](../docs/P2C-4_FONT_THEME_MENU_REPORT.md)。

### 前一 exact-head 核对（f4c107b）

`f4c107b5ef6bbc34a4a78fd3600dbfe4c4e81161` 的早先 P2C/P2B 运行、artifact 与 metrics 已由 P2C-4 报告保留；它不是当前最新的 Actions evidence snapshot。

### 前一 exact-head 核对（d238649）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，当前核验 head `d23864905f7e841eb428135bb1da244abca5d1df`；生产 UI checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。最新 ChatGPT review 仍是 P2C-3R1 对 `2c745710...` 的审查：CI gate accepted，整体 1:1 NOT PASS。
- P2C run [40393863774](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863774) Windows native、Linux Xvfb/XTEST、比较 3/3 PASS。artifacts：Windows `12687194357` (`sha256:39d347c00ab6cac66a155fcdd3c0c3b76ece305d49a3893872efc3e4239c82e0`)，Linux X11 `12690875520` (`sha256:880a6b11f3cf1555c082d511568ee6d486271b94ffbfdad79db910fc1622612d`)，comparison `12689194953` (`sha256:fa9abb4a9b835a231dfc288c73de6d1d3654e8bd636480977d532b4b93e89312`)。
- P2B run [40393863850](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863850) Xvfb/XTEST 与 Weston Headless/Pixman 2/2 PASS；artifacts X11 `12688395963`、Wayland `12686999481`。PR build [40393863878](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863878) 和 push build `40393857503` PASS，PR checks 7 PASS / 0 FAIL。
- 本轮下载核验 PNG/source hashes：Windows screenshots 41/41、Linux screenshots 44/44、visual 83/83、dark 70/70、P2C source 10/10、source+binary 8/8。几何 184/184（max 1.5 px）、nav 10/10、tree 102/102、raster 36/36、scroll 2/2 PASS。Light changed pixels 5.0741%–14.8853%；dark 5.9434%–73.2932%，均为 compare-only；全页像素 1:1 NOT PASS。
- Windows tag/source/release provenance 与 detached PGP 验证成功；Windows native 菜单仅四项 English。Tray、clean popup 及真实 clean confirmation/result 保持 BLOCKED。字体候选没有达到双语全页收益，因此生产仍 Sans/DejaVu Sans + Noto Sans CJK SC，无 Windows 字体文件进入仓库。
- Xvfb 使用 GitHub-hosted Ubuntu 24.04 / GTK 3.24.41 / Adwaita:light；暗色通过应用 Canvas palette。Weston 13.0.0 Headless/Pixman 客户端退出 0，但保留 `gdk_seat_get_keyboard` critical 和 SIGTERM 后 layer-finalization 诊断；根因未知，不声称有 Wayland 输入验收。当前 P2C APT 成功但下载 69.7 MB 用时 31m33s；旧超时底层原因未确认。
- 没有本机 GUI、Hyprland IPC、用户桌面输入或真实内存/特权操作。证据索引见 [P2C-4 report](../docs/P2C-4_FONT_THEME_MENU_REPORT.md)。

### 更早 exact-head 核对（37cefdb，历史）

- PR #7 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；核验 head `37cefdb351b1cfcdacbc6ca24eb8b06fead93cc1`。生产源码 checkpoint 为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；之后调整 P2B/P2C workflow 并发组并同步报告/记忆，未修改 UI、goldens、依赖或验收阈值。最新 ChatGPT Review 仍是针对 `2c745710...` 的 P2C-3R1 评审：CI gate accepted，整体 1:1 仍未通过。
- P2C run `40390082314` 在该 head 上 Windows 原版来源/截图、Linux 五页双语 Xvfb/XTEST、比较均 PASS（3/3）；artifacts Windows `12682119919` (`sha256:9a0463631da7bf4e049f5fe319afc9496f16b0fd2b940fae85363f6c2f845acc`)，Linux `12684755661` (`sha256:9bfb89a4bb10da11da9b42bc865627e36bc50c1e654ee3212ee14c7086ba0af2`)，comparison/dark/font `12683006399` (`sha256:8b765ec896abad794b91cbe80194c3959dc98a9c4fabb83cf2ffccdc276437b2`)。
- P2B run `40390082510` 的生产主窗 Xvfb/XTEST 和 Weston Headless/Pixman jobs 2/2 PASS；重复的五页 step 按条件 SKIPPED，P2C 已在相同 head 完整覆盖。X11 artifact `12682830560` (`sha256:d5e4967f92c7feb51defd5623b08b0135abed00ace0a2d17d5a995b22e901fc4`)，Weston artifact `12682645763` (`sha256:1c68e2e22f6945514b14efb065da7001b3633d3266795895198c2846b5d86524`)。客户端退出码 0；Weston 收到测试清理 SIGTERM 后记录 `BUG: finalizing a layer with views still on it.`，根因未确认，不能称为无诊断的干净 compositor shutdown；没有把它归为应用崩溃。
- PR build/governance/safety run `40390082353` 与 push build `40390075275` 均 PASS。下载 artifact 后 SHA 复核：visual 83/83、dark-theme 70/70、Linux screenshot 44/44、源码 10/10、二进制 2/2。资源几何 184/184（最大偏差 1.5 px）、导航 10/10、树线 102/102、raster 36/36、中段滚动 2/2 PASS；五页双语暗色对照均有原图/overlay/difference/heatmap；整体像素 1:1 NOT PASS。
- Zamak build 版本 0.2.0；CI 产物主程序 SHA-256 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。Linux 测试环境 Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、96 DPI/scale 1、Adwaita:light；Windows Server 2022 build 20348、96 DPI/100%。没有本机 GUI/Hyprland 操作、输入注入或内存特权动作。

## P2C-4 字体/控件绘制/深色主题实施记录（2026-10-27）

- PR #7 仍 OPEN / Draft；Base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；P2C 生产代码/工作流 head `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。Windows v.3.5.2 tag SHA 固定 `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- P2C run `40373093546` Windows 来源、生产 Xvfb/XTEST 与比较 3/3 PASS；P2B run `40373092535` 的主窗 Xvfb/XTEST、Weston Headless/Pixman 两 job PASS；build/governance/safety PR run `40373092652` 与 push run `40373081011` PASS。P2B 重复设置页 step 按条件 skipped，完整设置页交互在同 head P2C run 已执行。
- Artifacts：Windows `12673463315` (`sha256:22eaadc99b82745dc548af7036bca0bafe7f0c0b38ae5956553d9976dcbfddf1`)，Linux Xvfb `12674390489` (`sha256:f0b49b1577dec13c436cb967c65793ed7388ab0030e1fddc644e7636cc64d2f2`)，视觉/字体/暗色比较 `12673073193` (`sha256:0897e9e15d2a5c8eff9616b6d1eef730fc55e2d454be28238bf39a66810388a3`)。SHA 清单：Windows PNG 41/41、Linux PNG 44/44、dark 70/70、visual 83/83 全匹配。
- 几何/局部 raster 回归：184/184 resource rectangles PASS（最大 1.5 px），nav 10/10、tree guide 102/102、discrete raster 36/36 PASS。暗色 10 对 Windows/Linux screenshot 全部生成比较素材；变化率 5.9434%–73.2932%，仅 `CAPTURE_AND_COMPARE_PASS`，全像素 1:1 仍 `NOT PASS`。
- 字体候选：Win32 `MS Shell Dlg 2` / 8.25 pt / weight 400 / ascent-descent 11/2；Linux Pango Sans 8 pt / DejaVu Sans + Noto Sans CJK SC / ascent-descent 10/3。隔离 Liberation Sans 对 English 64/66 度量改善、简中 12/54 改善/12/54 退化；双语全页变化率没有充分收益，生产字体保持不变，不打包字体。
- GDI/Pango ascent+descent 总和均为 13 px，但 ascent/descent 分配不同 1 px；Linux text origin 不是 GDI baseline 的直接等价测量。GDI 字符串宽度/平均字宽已记；当前证据没有独立且可对齐的 character-spacing/tracking 量，未添加字距补偿。
- XTEST 双语验证 File/View/Settings/Help、语言子菜单、键盘 Enter/方向/Esc、分隔区跳过、暗色 5 页、safe clean dropdown 均通过；清理项禁用。Windows tray、原生清理 popup、安全确认/结果对话框仍 BLOCKED。简中 12 项来源差异维持 DECISION_REQUIRED。
- 不操作本机 GUI/Hyprland；无内存清理、Polkit、root GUI、内核写入或 Swap/ZRAM 变化。日志中的 Weston GTK seat critical 未屏蔽、未推断根因。
- 文档/记忆首轮提交 `6d5f4d4b8212913ea12b59c20f0916b62219aeea`、`dc1e112` checkpoint 的旧状态及早期 APT 超时记录保留为历史快照；当前已核验状态以本文件顶部 `6892110` exact-head 证据和 P2C-4 报告为准。P2C-4 的生产源码 checkpoint 仍是 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`，后续提交只同步证据文档/记忆。

## 先前未完成状态记录（已被上方 2026-10-27 核对覆盖）

以下各段保留当时进度，不能作为当前状态。

## 当前续接：P2C-4 字体/控件绘制/深色主题校准（2026-10-26）

### 最新核对（2026-10-26 17:44 UTC）

- P2C `40363399854` 已结束 FAILURE：Windows job PASS，Xvfb 截图 job 因 APT 38 分钟硬超时退出 `124`，comparison job SKIPPED。APT diagnostic artifact `12672670397` 的日志中 `fonts-noto-cjk` 61.2 MB 包下载未完成，之前出现 `Ign` 后重试；未看到 DNS、锁或缺包错误，底层网络/镜像根因未确认。字体候选及 Linux 截图均未执行。
- P2B `40363344884` 在 17:44 UTC 仍处于 Xvfb 与 Weston 的依赖安装中，未取消、不预判结论。
- 本轮暗色 palette 改动仍为未提交工作树内容；不改变 Light palette、字体字号、几何、文案、Windows goldens 或验收阈值。workflow 正在准备共享同一 PR 并发组、`cancel-in-progress: false`，压低两条 GUI workflow 的同时 APT 争用，但保留各自独立覆盖。
- 重新执行本机非 GUI检查：Zamak 0.2.0 `check/test/build` PASS，30/30 Zamak tests；`verify_project.py` 33 项 PASS；Python/C safety 3/3 PASS；Shell syntax、Arch build 与 `git diff --check` PASS。未在本机启动 GUI、Xvfb、Wayland 或操作 Hyprland。
- PR build run `40363344887` 和 push build run `40363337339`（head `eea82a8...`）PASS；当前远端视觉结果仍没有新 Linux截图。Windows 41/41 PNG SHA 核验仅是上游基准证据，不是暗色修复验收。
- 17:50 UTC 更新：P2B run `40363344884` 的 production Xvfb/XTEST job PASS，APT 安装用时约 35 分钟；它只验证旧推送头，设置页重复场景按条件 SKIPPED。Weston lifecycle job 仍卡在自己的 APT 安装阶段，仍无终态。此次观察支持“Hosted APT 安装极慢”的症状，但不能证明单一根因。
- 17:52 UTC 终态：P2B run `40363344884` FAILURE；Xvfb job PASS，Weston job APT exit `124`。Wayland diagnostic artifact `12672539453` 中同样显示 `fonts-noto-cjk` 61.2 MB 下载未完成。Xvfb apt artifact `12673795414` 显示相同版本依赖最后成功，约 35 分钟。当前工作树已移除仅英文 Weston 生命周期所需作业中的无用 Noto CJK 下载，并增加跨 P2B/P2C 的同 PR 并发控制；尚未推送/验证。

- PR #7 OPEN / Draft，branch `feat/issue-3-p2c-settings-ui`，base `feat/issue-3-p2b-main-window`；本轮起点为 P2C-3R1 已验收 HEAD `2c745710210c8794b52a0e9eb19dd1830c00d8d6`，当前推送 HEAD `eea82a8c2cafb16b25acf3d319e084335548356d`。本地未提交改动已按修复前真实 Windows/Linux 暗色截图校准 Canvas 暗色颜色；不改 Light 调色板、字体、布局、窗口几何、资源文案、goldens 或阈值，等 exact-head CI 验证后提交；未合并 PR。
- Exact-head Windows P2C job [40363399854](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40363399854) PASS，artifact `12670040771`，digest `sha256:74ca2a4a52afcde5fe04334918587a77b1fc6c469ffce03916dbb9920aa616c6`。来源标签 v3.5.2、官方资产 SHA、setup detached PGP signature 与二进制版本核验通过；环境 Windows Server 2022 build 20348、1024×768、96 DPI/100%。设置外框 574×374、客户区 558×335。GDI 132 行：逻辑 face `MS Shell Dlg 2`，8.25 pt/400，ascent/descent 11/2 px；注册表 substitution 指向 Tahoma，尚不足以证明实际字体文件。41/41 PNG hash 对照 `result.txt` 通过。
- Windows 已有主窗与五页 EN/简中 light/dark、四个 English 主菜单、Memory cleaning 滚动两语言截图。托盘菜单未被 UI Automation 暴露；安全清理菜单未能无副作用打开；确认/结果弹窗需真实清理，均 BLOCKED。未执行任何清理。
- Exact-head Linux P2C run `40363399854` 的 Xvfb 作业仍运行于 APT `Install runner-only build and X11 dependencies`，自 16:53:35 UTC 开始；截至上次查询 GitHub 未公开运行中 job 日志。APT 停滞根因未知，Linux截图、字体候选、dark comparison/overlay 尚无结果。P2B exact-head Weston run `40363344884` pending；旧 head run `40355264157` 的 Weston job状态陈旧且仍 reported in progress，不取消/不冒充当前结果。
- 旧字体基线：Windows GDI face alias `MS Shell Dlg 2`、registry substitute Tahoma；Linux 8 pt Sans / DejaVu Sans、简中 Noto Sans CJK SC，Pango ascent/descent 10/3；历史同源 120 行中 25 行宽高差≤2 px、95 行仍有度量差。Liberation Sans 作为独立 CI 进程候选，许可证文件随 artifact 检查；不提交字体文件、不修改生产默认、不通过缩放或缩小字号掩盖误差。Zamak Canvas `Painter.text` 当前给字号/粗体参数，没有 per-call 字体 family 参数；是否需要桥接等候候选实测后决定。
- 本机无图形桌面运行：Zamak 0.2.0；治理 33 项 PASS、Python/C safety 3/3 PASS、Zamak 30/30 PASS、check/build PASS、`scripts/build-arch.sh` PASS、所有 shell `bash -n` PASS、`git diff --check` PASS。未运行本机 Xvfb、Wayland、GUI、桌面输入或 Hyprland IPC。

## 历史续接：P2C-3R1 CI 稳定性与最终证据闭环（2026-10-26）

- PR #7 OPEN / Draft，分支 `feat/issue-3-p2c-settings-ui`，base `feat/issue-3-p2b-main-window`。原始任务 HEAD `52b7977b7582ee197470ba4a0e946a7b1558c2aa`；CI-only commit `406491b70594d202f3d2bb4efa667f2e0886c552` 已推送，PR 远端 head 已核对。生产 UI checkpoint 保持 `60be390eb0ff6c18564f35abcd23d36406c6d3f8`，没有 UI 或比较器变更。
- 修复前 P2C run `40324420754` attempt 1 在 Xvfb bulk APT step 被取消；attempt 2 全部 3 jobs PASS，但包源 `azure.archive.ubuntu.com` 下载 108 MB 用 31 分 18 秒、57.7 kB/s；APT update 本身快速完成，未见 DNS/锁/缺包错误。故障证据指向慢下载，不对更深层网络根因作推断。Pillow 安装 537 kB / 数秒成功。
- 新增 APT 有界诊断 runner，update/install 拆步，内部 8/38 分钟硬时限，workflow step 10/40 分钟，APT 网络超时 30 秒、retry 3、锁等待 180 秒，记录 stdout/stderr/退出码/原因类别/runner 与包版本，失败仍上传 artifact。P2C Xvfb/compare 与 P2B Xvfb/Wayland 都复用；P2B 总时限为 60 分钟。
- P2B 不再对 P2C 功能分支的 push 重复触发；P2C PR 时 P2B 仍运行主窗 X11 与 Weston 生命周期，五页 UI 由 P2C workflow 运行一次。两个 workflow 各自 PR concurrency group 使用 `cancel-in-progress: false`，避免中断正在生成证据的 run。所有 Windows 原生截图、Linux 生产截图和比较作业保留；184/184 几何等阈值/样本未变，1:1 仍 NOT PASS，12 项中文资源差异仍 `DECISION_REQUIRED`。
- P2C run `40336249517`：Windows 官方 3.5.2 provenance/native capture、Linux Xvfb/XTEST 与视觉比较 3/3 PASS；Artifacts Windows `12657162841`、Linux `12656813101`、visual comparison `12656279296`，APT logs `12656977979` / `12656638843`。P2B GUI run `40336249521`：Xvfb main 与 Weston lifecycle 2/2 PASS；artifacts `12655914336` / `12656259412`，APT logs `12655954131` / `12656972640`。该 run 的 P2C 五页重复 XTEST step 被有意跳过，P2C run 同提交已完整执行该测试。PR build `40336249519` 和 push build `40336242138` 均 PASS；PR checks 7 PASS / 0 FAIL，无取消或失败 job。
- P2C APT update/install：Xvfb 16/26 秒、Pillow compare 14/27 秒；P2B X11 6/27 秒、Weston 9/23 秒。Ubuntu 24.04.5 LTS runner image `ubuntu24 20261023.334.3`、APT 2.8.3；安装日志及关键版本和退出码均已在 APT artifacts 核验。
- 最终视觉比较保持 184/184 控件/滚动行 ≤2 px PASS（最大 1.5 px）、导航 10/10、树线 102/102、控件 raster 36/36；10 组页面变化率均优于 P2C-2，本轮范围 5.0741%–14.8853%，全页像素 1:1 仍 NOT PASS。12 项简中资源差异保持 `DECISION_REQUIRED`。source/binary manifest 8 项 SHA 全通过；Windows PNG 41/41、Linux PNG 17/17 SHA 全通过。完整哈希见 `docs/P2C-3_VISUAL_FIDELITY_REPORT.md` 和本轮 artifact manifest。
- build-and-test 系统工具 APT 步骤花 8分43秒但成功；Zamak 0.2.0 check/test/build、治理、Shell、C/Python safety 与 Arch build 全 PASS。本机非 GUI：project verify 33 项、C/Python safety 3/3、shell syntax、Zamak test 29/29、Arch build、diff check 均 PASS。没有启动本机 GUI、Xvfb/Wayland、Hyprland IPC、桌面输入或特权操作。

## 历史续接：P2C-3 原版视觉差异收敛（2026-10-26）

- PR #7 仍 OPEN / Draft；分支 `feat/issue-3-p2c-settings-ui`，Base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。本轮生产源码 SHA `60be390eb0ff6c18564f35abcd23d36406c6d3f8`；随后本记忆/报告更新为文档提交，不改生产源码；未改 main 或其他 PR。
- Exact-source Windows native + Linux Xvfb + comparison run `40319586672` 三个 job 均 PASS；artifacts：Windows `12648157361`、X11 `12648453073`、Overlay/heatmap/metrics/SHA `12648552362`。Windows 的 `IDC_HOTKEY=132` 经 `HKM_GETHOTKEY` 正确采到 `Ctrl+F1`（English/zh-CN），根因是旧采集器把返回值字节顺序反读。
- 对照结果：184/184 控件/滚动行资源矩形 ≤2 px PASS，最大分量 1.5 px；原缺少的 12 个中间滚动状态现有真实双端截图和对应矩形。导航 selection 10/10、树状点线 102/102、raster 定点样本 36/36 PASS。与 P2C-2 相同条件下 10/10 五页双语变化像素率改善，当前范围 5.0420%–14.8853%，仍非 1:1。
- Windows `WM_GETFONT`/GDI 实测 LOGFONT alias `MS Shell Dlg 2`, 8.25pt, weight 400, -11 px, text height 13/ascent 11/descent 2；注册表 substitute `Tahoma`，不能据此证明实际字体文件。Linux 8pt `Sans` / DejaVu Sans，简中 Noto Sans CJK SC fallback，Pango ascent 10/descent 3；英文文本宽度比中位数 1.1469。12 项简中运行时/资源差异矩阵维持 `DECISION_REQUIRED`，不私自选择弱化中文或偏离上游。
- 最终文档 head `7f1f75a031881b576f9e191dc62eb13f52680544` 的 PR Actions 全部 PASS：production Xvfb/XTEST + Weston run `40323243037`；Windows release/settings capture/comparison run `40323243043`；build/governance/safety run `40323243029`。Artifacts Windows `12650430810`、Linux production X11 `12650680581`、comparison `12649498772`。最终比较保持 184/184 geometry ≤2 px、10/10 双语全页变化率改善，变化率与 `60be390` exact-source run `40319586672` 相同。最终 run 的五页 client-area crop 与 Linux 图像 SHA 与此前相同；Windows 整窗 PNG 有独立 SHA，故 hashes 分 run 保管。Zamak check/test/build 29/29、治理 33、Python/C 安全 3/3、Shell 8、Arch build、diff check 本机 PASS。
- 历史诊断：独立 lifecycle run `40319586679` 于 12:23:26 UTC CANCELLED（Weston PASS、Xvfb 依赖安装阶段无结果）。同源码 push run `40319576201` 的 Weston PASS、Xvfb FAIL（exit 1）；artifact 无 client log、截图或 XTEST 记录，发生在生产进程启动前，具体根因无证据；对应 PR run `40319586672` 与最终 head run `40323243037` 的 Xvfb/XTEST 均 PASS。未在本机启动 GUI/Xvfb/Wayland，未访问桌面 IPC 或注入输入。
- Windows 已捕获五页 EN/zh-CN light/dark、主菜单四项及主窗；dark Linux 对照未运行。Tray menu、clean-region menu、cleanup confirmation/result 仍 BLOCKED，不触发真实清理。整体像素 1:1 NOT PASS；没有清理、Polkit、Swap/ZRAM、root GUI 或活动 Hyprland 操作。

## P2C-2 已核验历史证据（2026-10-26）

### 最新 exact-head 验收（production source SHA `20c8fbc8fc80c481965fecbde17aa507eb7247fe`）

- PR #7 仍 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；本轮在同一分支更新导航选中区和对照器，没有修改 main 或其他 PR 分支。
- 当前 Windows 3.5.2 native / Linux Xvfb / comparison run `40294897392` 成功；artifacts：Windows `12636924611`、Linux X11 `12638110426`、overlay/difference/geometry `12637747957`。所有原图和派生图的 SHA-256 manifest 在比较 artifact 中。
- 导航复核结果：资源 `IDC_NAV=104` 的 5 页 × English/简中，共 10 个 selection bound 全部 PASS，`dx/dy/dw/dh` 均为 0；102/102 个树状点线像素为精确 `#6D6D6D`。选中色是原版 `#D9D9D9`，不再绘制整行焦点框。
- 同次比较：172 项资源 ID 控件几何 PASS（≤2 px），0 FAIL；12 项 Linux 中间滚动状态在 Windows 未采集，标记 `NOT_COMPARABLE_NATIVE_STATE`。页面/控件 raster 12/12 PASS。changed pixels 6.8528%–16.8042%，所以 1:1 像素等价仍未通过。
- 同 HEAD 的生产 Xvfb/XTEST + private Weston/Pixman run `40294897349` 成功，artifacts：X11 `12638002197`、Wayland `12637667843`。Xvfb 日志记录 GTK 3.24.41 targeted delete handler、单一已核验 holder、重复显示/关闭、主窗仍可用及退出码 0。Weston 客户端退出码 0，但日志保留 1 条 `gdk_seat_get_keyboard` critical 和 SIGTERM 关机期间的 layer 诊断；未屏蔽，也没有证据确认因果或 compositor crash。这不代表活动 Hyprland 实测或 Wayland 输入验收。
- `build-and-test` PR run `40294897341` 与同 HEAD push run `40294892563` 均已完成并 PASS。
- Windows 环境：Server 2022，1024×768，96 DPI / 100%，light，8 pt `Ms Shell Dlg`；设置外框 574×374，客户区 558×335。Linux：Ubuntu 24.04、GTK 3.24.41、Xvfb 1024×768×24、scale 1 / 96 DPI、Adwaita light、DejaVu Sans / Noto Sans CJK SC。字体基线、抗锯齿和原生控件 raster 仍不同；暗色 Windows 参考缺失。
- 本机当前源码验证：Zamak `0.2.0`；`zam check .` PASS；`zam test .` 29/29 PASS（含双语导航几何测试）；`zam build .` PASS；治理检查 33/33、Python/C 安全测试 3/3、Shell 语法、比较脚本 `py_compile`、`scripts/build-arch.sh`、`git diff --check` 均 PASS。
- 本机没有启动 GUI/Wayland/Xvfb，没有 Hyprland IPC、特权操作、Polkit、清缓存或 Swap/ZRAM 修改。历史 GroupBox、默认只读勾选/颜色预览、无底部固定提示、GTK 针对性 close handler 的已验证状态在下方旧 checkpoint 记录中保留。

### Previous exact-head 验收（production source SHA `06be74b740e8906e64d837d8ac77e0255bea1081`）

- PR #7 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；继续使用 `feat/issue-3-p2c-settings-ui`，未修改 main 或其他 PR 分支。
- 修复 GroupBox 1 px 描边的半像素对齐。精确源截图对照 run `40290615170` 通过；artifacts：Windows native `12636291172`、Linux production X11 `12635727110`、overlay/diff/geometry `12635877774`。Windows 与 Linux 实际截图覆盖五页双语，所有原图 SHA-256、并排图、50% overlay、差异热图、RGB 样本及 CSV 几何表见 `docs/P2C-1_SETTINGS_UI_REPORT.md`。
- 184 个几何记录：172 PASS（偏差≤2 px）、12 `NOT_COMPARABLE_NATIVE_STATE`（Linux 中间滚动状态未在 Windows 捕获）、0 FAIL。Control/page raster 12/12 PASS；Appearance source-color samples 8/8 PASS。五页/两语言 changed pixels 为 7.3958%–17.6612%，故仍不等于 1:1。
- Xvfb/XTEST exact-source PR run `40290615170` PASS，应用正常退出 0；P2B GUI push run `40290609997` 的 Weston Headless/Pixman 也 PASS，artifact `12635732860`。Wayland artifact 保留一条 GDK `gdk_seat_get_keyboard` critical 与 Weston shutdown `BUG: finalizing a layer with views still on it.`，没有证据确认因果；不宣称 Wayland 输入验收。PR 事件 GUI run `40290615331` 的 Xvfb PASS，Weston 依赖安装步骤在报告快照时仍运行。
- GTK 3.24.41 日志记录 `delete-event` signal 66、唯一预存 unblocked handler 70 被单独 block 后连接目标 handler；运行时树确认唯一直接 GtkBox holder 且 border 12→0。XTEST 覆盖 Close/Escape/Alt+F4/WM_DELETE、重复隐藏/重开、主窗口继续可用及正常退出时释放窗口资源。
- 本机：Zamak 0.2.0 check/build PASS、test 28/28 PASS；项目治理 33 项 PASS、C 安全单测 3/3 PASS、Shell 语法、比较脚本 py_compile、Arch build、`git diff --check` PASS。主机没有 Xvfb/Weston，故本机运行时截图 `NOT RUN`；GUI 证据来自隔离 runner。没有启动本地 GUI、访问 Hyprland、清理内存、Polkit 或修改 Swap/ZRAM。
- 环境：Windows Server 2022 1024×768、96 DPI/100%、light、`Ms Shell Dlg` 8 pt；Linux Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、DPI 96/scale 1、Adwaita light、DejaVu Sans/Noto Sans CJK SC。字体/抗锯齿仍可见；暗色 Windows 截图和 Windows 未采集的中间滚动几何仍是缺口。

### Exact-head 验收更新（源码 head `1d1d6562cb27ddb63fb38a2bf71756b5e88d63ee`）

- PR #7 仍 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；PR review 无新行内结论。HEAD 与远端一致，工作树起初干净；未改 main、PR #2/#4/#5/#6，也未启动本机 GUI 或操作桌面。
- 生产 Xvfb/XTEST + Weston Headless/Pixman run `40284909745` 两作业均 PASS。Artifacts：X11 `12633129180`、Wayland `12633864035`。Settings 客户区 558×335；XTEST 覆盖五页双语、Close/Escape/Alt+F4/WM_DELETE_WINDOW、重复 hide/show、相同窗口重开、主窗存活和最终窗口资源释放，应用退出码 0。GUI bridge 日志：GTK `3.24.41`，`delete-event` signal 66，唯一预存 unblocked handler 70 被单独 block 后连接目标 handler；holder 为唯一直接 GtkBox、原 border 12，验证后改为 0。
- Windows native v3.5.2 run `40284909821` 的 Windows job PASS，artifact `12633108874`；源 tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`，binary 版本 3.5.2。PR run `40284909821` 的 Xvfb job 于 25 分钟 job timeout 后 CANCELLED，比较 job SKIPPED；push duplicate `40284904057` 同样在 Xvfb timeout 后 CANCELLED / 比较 SKIPPED。另一个独立生产 Xvfb+Weston run `40284909745` 两项通过。
- 同一 source HEAD 的 Windows artifact `12633108874` 与 Linux artifact `12633129180` 已用隔离 Bundled Python/Pillow `12.3.0` 作本地 exact-head comparison：截图 10 张；控件几何 172 PASS / 0 FAIL；12 个中间滚动项 `NOT_COMPARABLE_NATIVE_STATE`；页面/checkbox raster 10 PASS；上游颜色采样 8 PASS。Overlay/heatmap/CSV 已在本地比较输出生成。Changed-pixel 比例 7.9548%–18.6990%，仍非像素等价。
- 截图与源码 SHA-256、字体/主题/DPI、运行与几何细节已记入 `docs/P2C-1_SETTINGS_UI_REPORT.md`；矩阵补了只读默认态、无外框与证据状态说明。Windows `8pt Ms Shell Dlg` 与 Linux DejaVu Sans/Noto Sans CJK SC 抗锯齿/字体差异仍存在；暗色主题、托盘/菜单/对话框和 Wayland 输入未验收。Weston 留有一次 `gdk_seat_get_keyboard` critical、无 `wl_seat` 和 shutdown `BUG: finalizing a layer with views still on it.`；未观察到 compositor 崩溃，因果未定。
- 本机非 GUI：Zamak 0.2.0 `check`, `build`, `scripts/build-arch.sh` PASS，`test` 28/28；project verify 33 项、C safety 3/3、相关 Shell 语法和 `git diff --check` PASS。Pillow 比较使用 bundled runtime，未安装本机依赖。
- 已调整 P2C Windows baseline workflow：仅由目标 PR 触发，避免同一提交同时启动 push/PR 两套重复截图任务；Xvfb timeout 45 分钟、比较 timeout 20 分钟。此工作流修正尚待当前提交的 Actions 验证。没有在本机启动 GUI、Xvfb、Wayland 或操作 Hyprland。

### P2C-2 GroupBox 校准记录

- 参考与首次未校准样本为 Windows `#DCDCDC`、Linux `#BBBBBB`。仅改颜色后的 CI 样本为 `#E5E5E5`，原因是 1 px Cairo stroke 在整数路径中心覆盖半像素。路径移至半像素并缩小矩形后，exact-head run `40290615170` 在双语均得到 `#DCDCDC`，相关几何与栅格检查通过。暗色值仍是临时值，没有真实 Windows 暗色参考。

- 继续使用 PR #7 分支 `feat/issue-3-p2c-settings-ui`。开工时本地与远端 HEAD 均为 `90f5b31a9dede8896a44bd356c43f38862a005db`；PR #7 OPEN / Draft，base `feat/issue-3-p2b-main-window`，没有修改 main 或其他 PR 分支。
- 已下载并核对该 HEAD 的 CI 证据：Windows 原生/几何 artifact `12632986603`、生产 Xvfb artifact `12632328288`、overlay/diff/几何 artifact `12632129496`；run `40282499292` Windows+Xvfb+comparison、`40282499286` Xvfb+Weston、`40282499313` build/test 均 PASS。该批证据只对应修正前的 `90f5b31`。
- 修正前可比较控件矩形 172 PASS，12 个 Linux 中间滚动项为 `NOT_COMPARABLE_NATIVE_STATE`，0 FAIL；截图像素变化 61.7290%–80.1728%，故不通过 1:1 像素验收。
- 当前未提交修改：浅色设置页背景改为真实 Windows 截图测得 `#F0F0F0`，导航区域保持白色；复选框方框调整到 13 px；内存区域列表 checkbox / label 内缩改成 4 px / 22 px，并由 `ui_geometry.zm` 集中管理；比较脚本增加页面底色样本和 checkbox 像素边界检测。
- 本机非 GUI 验证：Zamak 0.2.0 `check` PASS，`test` 28/28 PASS，`build` PASS；项目治理 33 项 PASS；C 安全测试 3/3 PASS；比较脚本 `py_compile` PASS；`scripts/build-arch.sh` PASS；`git diff --check` PASS。没有本地启动 Xvfb、Weston、生产 GUI 或活动 Hyprland。
- Exact-head `f5c5488b4f4e0871311c52dfe4bde61d89be7b7a` Actions：GUI/XTEST + Weston `40284199595` PASS；build/test `40284199576` PASS；Windows/Xvfb/comparison `40284199572` 的采集/Xvfb PASS，但 comparator FAIL 两个 Memory checkbox raster 行。比较结果是 172 geometry PASS / 0 FAIL（另有 12 个中间滚动项 NOT_COMPARABLE_NATIVE_STATE），六个页面底色样本和两个 General checkbox 样本 PASS；截图 changed pixels 降为 7.9548%–18.6990%，仍非像素等价。
- 检查比较产物 `12634010092` 后确认 Memory checkbox ROI 下边缘包含下一行的一个深色像素，导致两种语言都误报高度偏差；已把裁剪范围缩到当前列表行以内，待新 commit/Actions 验证。Exact head Windows/Xvfb artifacts 为 `12633352398` / `12633925245`。Windows 原生 2 px 几何、深色主题、菜单/托盘截图和像素级 1:1 仍未整体验收。所有设置快照仅为上游只读默认值；未执行设置保存、清理、Polkit、Swap/ZRAM 操作。

## P2C-1 最新续接（2026-10-26）

- PR #7 仍 OPEN / Draft；base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。生产源码测试 checkpoint `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`；文档/记忆同步在 CI-verified head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df` 上完成。本段再更新的只是报告哈希和 Actions 状态，未改生产源码。分支 `feat/issue-3-p2c-settings-ui`。未合并或改写任何其他分支。
- 已修正 libui-ng settings content holder 的 12 px border。最新 X11 artifact `12627741039` 记录 `CI_SETTINGS_CONTENT_BORDER old=12 new=0`，Canvas 与 Windows 原生 client area 均为 `558×335`（固定 Windows Server 2022 / 96 DPI / 100% 条件）。此前 Linux `523×321` / `547×345` 数值不再作为当前尺寸。
- 首次从隔离 Windows Server 2022 interactive runner 获得实际原版 `v.3.5.2` Native 截图。固定 upstream SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`；release archive、setup SHA、setup detached PGP signer、portable executable 3.5.2 及 SHA 均通过核验。环境/几何/PNG SHA-256 与 5 页双语索引在 `docs/P2C-1_SETTINGS_UI_REPORT.md`；Windows artifact `12626872531`，Linux settings X11 artifact `12627741039`。
- Windows 捕获 1024×768、96 DPI、100% scale、light theme、8pt Ms Shell Dlg，时间 `2026-10-26T04:16:04Z`。Settings 外框 574×374、客户区 558×335；IDC_NAV `(12,10) 132×312`，页面 `(156,10) 390×273`，Reset `(153,299) 75×23`，Close `(471,299) 75×23`。Windows 主窗 352×407。采集只在临时 runner 执行，未触发内存清理。
- 当前 code head 本机验证：Zamak 0.2.0 check/build PASS、test 22/22；`verify_project.py` 33 项、C safety 3/3、Arch build、两份变更 shell `bash -n`、`git diff --check` 均 PASS。未启动本机 GUI、Xvfb、Weston 或活动 Hyprland；未执行 Polkit/内核写入、Swap/ZRAM 修改。
- 完整 PR Actions 在文档 head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df` 全 PASS：GUI `40269534094`（Xvfb 与 Weston 两项），build `40269534153`，Windows native baseline `40269534205`。对应 artifacts：Linux X11 `12627279867`、Wayland `12627990285`、Windows native/screenshots `12627309561`。Windows runner 最近采集时间 `2026-10-26T04:31:57Z`；截图 SHA 与环境清单在设置报告中。Push GUI `40269530616` 的 Weston 通过，但 Xvfb job在最后一次检查时仍运行；PR checks 均已完成成功。
- 实测客户区几何相同不等于视觉 1:1。尚未对齐窗口客户区后生成 overlay/diff 或验证 2 px；深色主题、主菜单/托盘/清理对话框截图未取得；设置项持久化、托盘服务、自动/手动真实清理仍未实现/启用。Weston artifact 保留一次 `gdk_seat_get_keyboard` GTK critical、无 `wl_seat` 和 shutdown layer diagnostic，因果未定。

## 最近记录：P2C-1 五页设置 UI 与原版截图 runner（2026-10-26）

- P2C-1 在独立分支 `feat/issue-3-p2c-settings-ui`，Draft PR #7 base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。实现五个设置页与 English/简体中文切换；PR #7 仍 OPEN / Draft，不合并。
- 在 head `988513ad7c9afe5fd79cb0fe6239f7759cbdaf86` 的 Xvfb run `40260614636` 真实 WM_DELETE_WINDOW 测试失败：关闭设置窗导致应用循环退出，缺少设置 close callback marker。已在 `native/gui_window.c` 添加只作用于应用设置窗的 GTK `delete-event` 重新路由，提交 `396c3126fbb8b9af33755ebf8f2eb67942c71f83` 并推送。新 artifact 观察 `signal_id=66 disconnected=1 connected=1`、delete callback、Zamak 将设置窗隐藏并保持主窗、随后 app 正常退出 0。
- 修复 head 的 PR GUI Actions [40261537129](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261537129) 的 Xvfb/XTEST 和 Weston Headless/Pixman 两作业均 PASS；build-and-test [40261541370](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261541370) PASS；push GUI run `40261541380` PASS。Artifacts：X11 `12624986143`，Wayland `12623629586`。中英文五页截图与源 SHA 记录在 `docs/P2C-1_SETTINGS_UI_REPORT.md`。
- X11 环境：Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、scale 1/DPI 96、Adwaita light；主窗 337×369，设置窗 547×345，均是 Linux 测试尺寸而非 Windows 像素基准。语言/页面、Reset 确认、Escape、Close 按钮和窗口管理器关闭均通过；标题栏 close 不再退出主应用。未在本机安装 Xvfb/启动 GUI。
- Wayland 私有 Weston/Pixman 下主窗和设置窗 surface 创建/绘制/退出码 0；不测输入；记录一次 `gdk_seat_get_keyboard` critical、无 `wl_seat`，未归因。没有访问活动 Hyprland 或桌面 IPC。
- 本机：`verify_project.py` 33 项 PASS；Python/C 单测 3/3；shell 语法 8/8；Zamak 0.2.0 check/build PASS，test 21/21；`scripts/build-arch.sh`、`git diff --check` PASS。没有 Polkit、drop_caches、Swap/ZRAM 或 root GUI 操作。
- Windows runner 第一次尝试 run `40261537106` / `40261541252` 在 20 分钟限制取消：资产 SHA PASS 后卡在 Chocolatey GnuPG 安装，artifact `12625127901` 只含部分 `result.txt`。移除安装、使用 Git for Windows 自带 GPG 后，run `40263501261` / `40263504561` 又因 MSYS GPG 将 `D:\...` `GNUPGHOME` 误当 MSYS 相对路径而快速 FAIL；tag 和 archive/setup SHA 均 PASS，签名验证、binary version、桌面检查、截图尚未到达。当前工作副本以 `cygpath.exe` 将 GNUPGHOME/key/setup/signature 路径转成 POSIX 格式，待新 Windows Actions 验证。真实 Windows 截图、DLU 实测、overlay/2px 和 Hyprland 输入仍 BLOCKED / NOT RUN。设置持久化/托盘/特权功能未实现或禁用。

## 历史快照（2026-10-27；P3A-PRE-R1，基线 629fc3f）
> 以下为 P3A-pre 当时状态与证据；OPEN/Draft、审批及“未实施”描述仅对应该日期，当前状态以上方 2026-10-28 快照为准。

- 远端 fetch 后本地/远端 P2C 分支一致 `629fc3f88ee91d0cc3582d6be1789991fc3f72b6`，工作树开始时干净；未切换分支、未合并、未改 base/main。
- 核对 ChatGPT review `5663699910` 和三个新 ADR 线程：`4386187875`（rename 后 fsync 状态）、`4386187883`（损坏配置保存）、`4386187887`（路径/锁/权限）。ADR 保持 PROPOSED，修订详见 `docs/ADR/0004-user-preferences-persistence.md`。
- 当前 exact baseline PR #7 checks 7/7：P2C `40465893251`、P2B `40465893241`、build PR `40465893268`；P2B push build `40465886984` 也通过。证据检查 artifact `12717146938` digest `sha256:d9c99dcaa330aa76c25be8ef60332ad61cb082e5a951fd3ca153369ff9f8bfd5`，client.log SHA-256 `39ef3d24f255df6cd3ccbdd90fd7cf280105933ea130a8351aeccc84c06d08c2`。该 Xvfb 日志含失败注入、立即 redraw 请求和 em-dash 状态标记。
- PR #4 workflow 触发器已从远端文件核实：push `test/issue-3-isolated-gui`，pull_request base filter `fix/issue-1-zamak-build`，权限 `contents: read`。若改 base 到 main，须先更新 PR 过滤条件。独立的 12 项 probe tests 不得被生产 GUI 测试替代。
- 本地验证 PASS：`python3 scripts/verify_project.py` 33 项；`python3 -m unittest discover -s tests -v` 3/3；`bash -n scripts/*.sh scripts/ci/*.sh`；Zamak 0.2.0 `check`、`test` 30/30、`build`；`git diff --check`。本轮仅文档/记忆改动，未运行图形或桌面操作。
