## P3C：候选面板的 Continue 在未选中时正确保持 disabled

2026-11-09，Xvfb + XTEST 驱动 `m0+m1-candidate`。

### 观测

- 面板列出真实进程后，`Continue...` 按钮**保持置灰**（未选中任何候选时不放行）。
- 点 Tab 只移动焦点，不构成选中；点 Continue 区域无任何 pkexec 派生（`pgrep pkexec` 为空）。

这是**正确的 fail-closed 行为**，不是缺陷：UI 不允许在没有明确目标的情况下发起特权请求。

### 为什么自建 320 MiB 目标不出现在此 GUI 列表

自建目标以 `systemd-run --user --scope --unit=app-xtarget2` 启动时**能被独立
`mr_cand_scan` 收录**（`private_resident=295981056`），但在 GUI 场景中它由 DSH 会话派生，
先在 `dsh-subprocess-*.scope` 下；该 cgroup 尾段不匹配 `app-*.scope`，
因此被 `mr_xproc_cgroup_path_is_user_app` 拒绝。保护设计，非缺陷。

### 结论

GUI 侧（枚举 → 选择 → 门控 → 提交前拦截）行为一致且 fail-closed。
唯一未完成的是**选中后点击 Continue 的真实 `process_madvise` 执行**，
该步需要 Polkit 管理员密码（两 policy 均 `allow_active=auth_admin`，本机无 rules 放行），
必须由用户亲自输入。不代劳、不绕过。
