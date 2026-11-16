# P0 集成报告 — Issue #10 审查意见落实（2026-11-01）

基线 `0eea3dcf55f8915e8e4128d793205ec1dcc8775f`，两条 P0 线独立完成后合入，无冲突。

| 用途 | 分支 | SHA |
|---|---|---|
| P0-A 页缓存清理产品路径 | `p0a/pagecache-production` | `0cbdf605c4ddec4698a400ad79ed34576aa9b035` |
| P0-B 跨进程只读预览 | `p0b/crossproc-impl` | `b73d0ca2ace236d14f3d590e74f666b7907c5f9a` |
| 集成 | `p3c/integration` | 见 git log（本次 merge commit） |

## P0-A：页缓存清理成为真实、受门控的产品面

审查指出的三个实缺口全部补上，且没有放松任何门：

1. **Polkit policy 落地**（此前 install-arch.sh 引用但文件不存在）。
   单一 action、exec.path 钉死 root-owned helper、defaults 全部 `no`：
   发行版自带的 policy 不授予任何东西，管理员需通过 rules 文件显式放行。
   双语 description/message 写明内核文档的代价：重读变慢、不保证释放量、
   一次性诊断而非优化。无 retain_authorization/require_active：每次重新授权。
   现有 test_polkit_policy 规格（17 例）验证通过——我的第一稿三种方式违反
   它（auth_admin_keep、缺 zh_CN、argv1 注解），已按规格修正。

2. **能力门从 -D 宏改为翻译单元选择**。zam 的 [native] 不接受 defines，
   所以 MEMREDUCT_ENABLE_PRODUCTION_CLEAN 从未真正可达。现在
   mr_production_clean_enabled() 只存在于一个 gate TU：
   default（返回 0，zam.toml 所列）或 enabled（返回 1，仅由
   scripts/ci/build-production-clean-gui.sh 在私有树副本中选择）。
   二进制要么包含可达的桥、要么不包含，nm 可直接判别。

3. **production-clean 构建可脚本化、可校验**。新脚本跑全套门禁、公开换
   gate TU、构建 GUI+helper、nm 验证后才发布哈希。
   scripts/ci/p0a-production-clean-mock-xvfb.sh 用真实指针事件驱动全链：
   点击 target 19 → 生产确认框（fits=true；旧文案第 3 行实测 236px 超出
   229px 面板会截字，已改为 "Re-reads slower; no amount guaranteed." /
   "重读变慢；不保证释放量。"）→ 键盘焦点两按钮 → 确认 target 710 →
   mr_start_reclaim → **真 /usr/bin/pkexec**。headless Xvfb 无 polkit
   agent，诚实结果是 result=uncertain result_state=10
   automatic_retry=false——脚本断言它，同时禁止 result=accepted、"freed"、
   任何 backend=mock 标记出现。accepted 分类由 TESTING 构建单测覆盖。

另修：target 19 失败文案原先在任何未启用构建里都谎称 "pending safety
review"；现在如实说"当前构建未编译此功能"，CI 标记带
production_bridge_connected=false。

## P0-B：跨进程只读预览进入普通 GUI

ADR-0007 只读半区接入主窗口：self-trim（target 500）后状态栏追加
"N processes / M regions / ~X MiB candidate (read-only)"。planner 钉 pidfd、
按固定策略分类区域，但本构建中可写半区是拒绝 stub——nothing advised,
nothing written。CI 标记 CI_CROSSPROC_PREVIEW 带 readonly=true 与
no_advice_issued=true，新 E2E（scripts/ci/p0b-self-preview-xvfb.sh）缺失即失败。

状态语义：0 OK、5 NO_CANDIDATES、3 BUDGET_EXHAUSTED 都是完成的扫描
（5 是"每个进程都属于其他用户"这台机器上的诚实答案——实测 1x SKIP_PID1
+ 255x SKIP_OTHER_UID、0 targets）；只有真正失败的扫描才显示"扫描不可用"。
接线前先用直接 C harness 在真机 /proc 上验证过。

Xvfb 真点击 E2E：hover 扫描找 Clean 按钮、点箭头区（target 20）开菜单、
点第一行（target 500），断言 CI_CLEAN_SELF_MENU / CI_CLEAN_SELF /
readonly 预览标记，并断言全程无 pkexec/polkit/swapoff/zramctl 引用。

## 集成验证（合并树）

```
python3 -m unittest discover -s tests -> 107 OK (1 skip)
bash -n scripts/*.sh scripts/ci/*.sh  -> clean
zam check .                           -> CHECK OK
zam test .                            -> 76 passed
zam build .                           -> memreduct_zamak-95480779...
p0b E2E on merged default build       -> PASS (readonly=true, no_advice=true)
```

## 明确 NOT RUN / 待用户单独授权

- 真实特权回收（pkexec 授权通过 + helper 写 drop_caches）：需要用户批准
  目标设备与动作；本机只验证到"真 pkexec 被调用且被 polkit 拒绝"为止。
- 跨进程 process_madvise 对其他真实进程：OWNER_APPROVAL_REQUIRED，
  本轮 GUI 只暴露只读预览。
- 三按钮真实点击（#11 项）不变。
