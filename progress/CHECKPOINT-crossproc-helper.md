# CHECKPOINT — Issue #12 M1 跨进程 helper 实施（2026-11-02）

## 状态：仅完成取证与工作树建立，生产代码尚未写入

## 已完成

1. **取证完毕**（全部一手读源码，未见推测）：
   - `p3c/integration`（57dc1fa）是当前最完整集成链：P0-A pagecache 生产路径 + P0-B 只读跨进程预览 + M0 可安装特权链。
   - 现有特权面：`native/memreduct_helper.c`（固定 `pagecache` 一个动作，argv[1] 白名单）、
     `packaging/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy`
     （单 action、`exec.path` 钉死 helper、`allow_any=no/allow_inactive=no/allow_active=auth_admin`）、
     `native/async_privileged.c`（posix_spawn pkexec，126/127 不臆测归因）、
     `scripts/install-arch.sh` / `uninstall-arch.sh`（staging + root:root 0755 复核 + symlink 检测）。
   - 只读 planner：`native/crossproc_plan.c/.h`（扫 `/proc`、预算上限、pidfd 钉身份、skip 原因枚举）。
   - 写半部：`native/crossproc_advise.c/.h`（`MEMREDUCT_ENABLE_PRODUCTION_ADVISE` + `mr_crossproc_arm_approval` 双闸）。
   - GUI 契约：`native/crossproc_gui.h`（强制文案，禁止 "freed"/"system working set"，禁止 UI 传 PID）。
   - 已有真实 EPERM 证据：`p0b/crossproc-impl` 6e36b3d，`tests/m1_real_madvise_test.c` 一次真实
     `process_madvise` 返回 `-1, EPERM=1`。

2. **新工作树已建**（不动任何既有 worktree 的未提交成果）：
   - 路径 `/home/arlune_innersea/.codex/worktrees/p3c-crossproc-helper`
   - 分支 `feat/issue-12-crossproc-helper`，基点 `p3c/integration` @ `57dc1fa`
   - 工作树干净，尚未写入任何代码。

## 下一步（按此顺序，不要重新取证）

1. `native/crossproc_helper.c` + `.h`：独立特权 helper。
   - 与 pagecache helper **代码/路径/Policy action 全分离**；安装至 `/usr/libexec/memreduct-zamak-crossproc-helper`。
   - 不信任 GUI 传入的 PID/UID/iovec/maps/环境变量；只接受**不可信候选 PID**，用 `pidfd_open` 重新打开，
     自己读 `/proc/<pid>/status`、`stat`、`maps`，校验：euid=0、有效 `CAP_SYS_NICE`、请求者 UID 一致
     （`PKEXEC_UID` 仅作线索，须与真实 `/proc/<pid>/status` Uid 交叉验证）、进程存活、`start_time` 一致、
     非 PID 1 / 非内核线程 / 非自身及祖先 / 非 Polkit agent / 非桌面关键进程。
   - 自己重新规划 VMA：只取 private、可回收、非特殊映射；单进程、iovec 与字节上限（首次 1 MiB，随后 ≤64 MiB），
     超时/失败 fail-closed，**一次调用、不重试**。
   - 调用 `process_madvise(pidfd, &iov, ..., MADV_PAGEOUT, 0)`，完整处理
     EPERM/ESRCH/EINVAL/ENOSYS/partial/0/正字节数；输出结构化结果行（machine-readable），
     明确 `bytes_advised != bytes_freed`。

2. **新 Polkit policy 文件**（独立 action id，如 `...zamak.crossproc`），
   `exec.path` 只指向新 helper；defaults 同 M0 形态（三项分开，无 `*_keep`）。

3. `native/async_crossproc.c/.h`：GUI 侧一次性 pkexec 调用，分层状态
   （授权拒绝 / 未执行 / 内核拒绝 / 部分 / 已建议 / 结果未知），**永不自动重试**。

4. 非特权候选枚举（同 UID、普通应用、可操作区域估算）—— 只能是**预览**，不得声称已回收。

5. Zamak GUI 接线：选择进程 → 显示目标/UID/名称/预计区域/上限/风险 → 逐次确认 → Polkit → 结果分层展示。
   遵守 `crossproc_gui.h` 全部 6 条规则。

6. `scripts/install-arch.sh` / `uninstall-arch.sh` / `scripts/build-arch.sh` 加入新 helper（root:root 0755 复核、
   与 pagecache helper 路径分离）；`scripts/ci/build-production-gui.sh` 同步。

7. 测试（新增，不得只交规划）：
   - helper 拒绝路径负例：错误 UID、PID 1、自身、祖先、已退出、start_time 不匹配、超预算、非 pidfd。
   - `MADV_PAGEOUT` 调用形状与 errno 分类（注入 backend，不触真实内核）。
   - policy/install 静态检查（新增独立用例，沿用 `tests/test_polkit_policy.py` 风格）。
   - Python 全量 `unittest discover`、`zam check .`、`zam test .`、`bash -n scripts/*.sh`。

8. `docs/ADR/0007-*.md` 增补 M1.1 段；`memory/SHORT_TERM.md` 记录本轮真实证据。

## 硬边界（本轮未获授权，逐条不可越）

- 未获批准：安装 root-owned crossproc helper / 新 Polkit policy 到本机。
- 未获批准：再次执行任何真实 `process_madvise`（上一次授权已用掉）。
- 未获批准：push / merge / release / tag 推送。
- 永不：给 GUI 或通用 helper `setcap`；把 pagecache helper 扩成任意 PID 执行器；
  让 root 扫全 `/proc`；常驻 root daemon；修改 Swap/ZRAM；重启系统服务。
- 任何失败都必须如实呈现为拒绝/未执行/未知，不得伪造成功。

## 阻塞

- 无技术阻塞。等待本会话上下文压缩后继续写代码。
