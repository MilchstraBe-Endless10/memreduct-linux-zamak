# S5 进度落盘 — task-mwfx8g0k（GUI 范围提示+深度清理默认关闭+单文件部署+对照验收）

落盘时刻：2026-11-14 12:50（context 熔断，等待手动压缩后接续）

## 做到哪（调研已完成，代码未动）

1. S4 实现位置：`/home/arlune_innersea/.codex/worktrees/q12-dof/memreduct-linux-zamak`（HEAD a55c08e，
   "feat: S4 pressure-reclaim stage (3GiB x2 rounds, PSI-guarded)"）。
   - `src/main.zm` 3146 行；CI_DEEP 探针仅 3 处：
     - L2146-2150：`MEMREDUCT_CI_DEEP=1` + `s.test_mode` 在 2s tick 调 `activate(s, 104.0)`（emit CI_DEEP_HARNESS）。
     - L2317-2368：target 104.0 = 深度清理管线 S1(pkexec 页缓存)→S2/S3(session file pages)→S4(pressure_run)，emit `CI_DEEP_CLEAN ok= ran= delta_mib=`。
     - 入口：菜单 File 内 row2（L1425-1427，"Deep clean (multi-stage)" / F10 快捷键 104.0）。
   - 均衡清理入口：菜单 row1（103.0）+ clean popup row9 + Canvas bal picker（bal_view=1）。
2. 已安装二进制：`/usr/bin/memreduct-zamak` sha256=6f06697b6d12dfdf14086fe9a597ea90572c665a6af7c1382dc649069aee9282
   （2026-11-13 23:18 装的 R5 RSS GUI 版）；备份 `/usr/bin/memreduct-zamak.before-p0a`
   sha256=83392128fc957ce949ce517c94007fe3fcba7bdac0ddebbd2a0095eee4d72b75。
   ⚠️ 教训：`/usr/bin/memreduct-zamak --version` 不支持该参数，会直接拉起 GUI 主窗口（已 kill）。
3. 候选构建：`/home/arlune_innersea/q12-evidence/q12-candidate/memreduct-zamak`
   sha256=0e6aa385f077147db534d444de8df756dbc5ba8a570f2f26e9500e093a192971（S4 深度清理版）。
   q12-dof worktree 干净（仅 ?? deploy-backup/，内有 pre-deploy 备份）。
4. 构建入口：`bash scripts/ci/build-production-clean-gui.sh`（换 gate/gui_runtime/async/window 四 TU，
   自带断言拒 CI 串混入）。

## 下一步（按序）

1. **计划闸口（硬闸）**：动笔前先 `board_report(kind=progress, question="计划改动：…")` 报文件清单
   （预期全部落在 q12-dof worktree：`src/main.zm`、`src/ui_geometry.zm`、
   可能 `progress/S5_*.md`；不碰别的 worktree、不碰 /usr/bin 之外的系统路径）。
2. 实现：深度清理入口改为「高级/深度清理」面板——默认关闭（设置开关或菜单默认 disabled），
   面板内明确显示影响范围文案（会话级 memory.reclaim、不带 swappiness、匿名页可能进 ZRAM/swap）；
   均衡清理保持现状（定向文件页回收）；范围提示 + 用户明确选择 + 压力变坏不再继续（PSI 守卫已有）。
3. 构建：`bash scripts/ci/build-production-clean-gui.sh` → 核对候选哈希。
4. 部署：核对候选与安装版完整哈希 → 备份当前 /usr/bin/memreduct-zamak 到明确路径（如
   /usr/bin/memreduct-zamak.before-s5）→ 单文件替换（需 root，走 sudo，注意沙箱 NoNewPrivs 禁令——
   实际本会话 danger-full-access 且 approval 禁用，sudo 若失败要上报而不是绕）。
5. 对照验收：不带 MEMREDUCT_CI_DEEP 的正式 GUI 在真 Hyprland：即时/30s/数分钟
   MemAvailable+PSI+磁盘换页（vmstat pgmajflt/pswpin/out）+前台响应；
   放行标准=用户可明确选择范围+界面不阻塞+压力变坏不再继续+结果不夸大。
6. 完成契约：board_report(kind=complete) 或escalate；先 send_message 给父会话。

## 卡在哪

- 上下文熔断（连续 6 轮无正文产出），需手动压缩后由接续会话继续；
- 尚未向看板报计划闸口（这是动笔前的硬闸，接手者第一件事就做它）。
- git 红线照旧：禁 checkout/restore/reset --hard/clean、禁 commit/push/add。

---

# 续跑2更新（2026-11-14 12:57，再次熔断）

## 已完成（本段新进展）

1. 工作树盘点：q12-dof HEAD a55c08e。未提交改动：
   - `M native/balance_bridge.c`（-92行）：并行任务 task-mwfx84el 在途编辑（12:52 还在写）。内容=S4 重写为 512MiB 小批次+PSI avg10/swap速率守卫，且删除了 `mr_ci_deep_requested`（native 侧环境变量探针）。方向与 reviewer 意见一致，保留不动。
   - `?? progress/task-mwfx84el-S4-metrology-fix.md`（对方进度）、`?? scripts/acceptance-prod-gui.sh`（12:50 新增）、`?? deploy-backup/`、`?? progress/S5_GUI_SCOPE_HINT_PROGRESS.md`（本文件）。
2. 计划闸口已报：board_report 子代理不可用，已用 send_message 向父会话(session-dad7b6ae)报了完整文件清单与不碰清单。父会话随后发来「断点续跑」指令 = 默许继续。
3. 设计勘察完成（全部带行号，见下）。

## 完整实现设计（下次直接照做，不再勘察）

**改动文件：src/main.zm + src/ui_geometry.zm（+本文件收尾）。不碰 native/*、scripts/*。**

ui_geometry.zm：
- `root_menu_row_count`（~L156）：menu==2 从 5 改 6（View 菜单加一行「Enable deep clean (advanced)」check 行）。
- `root_menu_separator`：不变（menu2 row3 仍是分隔线）。
- `menu_root_rect`：高度跟随 row_count 自动变，无需改；menu==2 宽 256 装得下。
- 新增 `deep_panel(s) -> ui.Rect`：复用 bal_panel 尺寸 `ui.rect((w-320)/2,(h-300)/2,320,300)`；`deep_run_button_rect`/`deep_cancel_button_rect` 参照 bal_run_button_rect（panel.bottom()-30，22 高）。

main.zm：
- `MainWindow` struct（L225+，bal_* 字段附近）加：`deep_enabled: bool`、`deep_view: i64`、`deep_confirm_focus: i64`；new() 初始化 `deep_enabled: false, deep_view: 0, deep_confirm_focus: 0`。
- `root_menu_target`（L389）：menu==2 加 `if row == 4: return 205.0`，row4 原 204.0 改 row5。
- `draw_root_menu` menu==2 分支（~L1437）：row4 画「Enable deep clean (advanced)」+draw_menu_check(s.deep_enabled)，row5 画原 Language 行（含 ›）。文案双语 label()。
- `hit()`（L1361）最前面加：`if s.deep_view == 1: return deep_hit(s, x, y)`；新 fn deep_hit：run 按钮→930.0，cancel→931.0。
- `activate()`：
  - 加 `205.0` 分支：toggle deep_enabled，emit `CI_DEEP_TOGGLE enabled={0/1}`，redraw。
  - 加 `930.0`：deep_view=0 → 调新 fn deep_run(s)；`931.0`：deep_view=0 redraw。
  - 改造 `104.0`（L2317）：开头加守卫——`if not s.deep_enabled: s.menu=0; s.popup=0; status=label(zh,"Deep clean is off by default. Enable it in the View menu (advanced).","深度清理默认关闭；请在 View 菜单（高级）中开启。"); emit CI_DEEP_BLOCKED reason=disabled; return`；否则 `s.deep_view = 1` 并 emit `CI_DEEP_SCOPE_PANEL shown=true`，不再直接执行。
  - 新 fn deep_run(s)：把 L2317-2368 原管线体（S1→S4+delta 汇总+CI_DEEP_CLEAN emit）整体移入，930 与 CI_DEEP 触发共用。
- 深度说明面板绘制 fn draw_deep_panel(s,p,颜色)：仿 draw_balanced_picker。文案（双语）必须含：
  - 作用范围：本会话 cgroup 的 memory.reclaim（会话级），不是全系统。
  - 不带 swappiness 调整；内核可能把冷匿名页压入 ZRAM/交换区（去向不保证只进 ZRAM）。
  - 短暂延迟/CPU/I/O 影响；PSI 与换页速率守卫在压力变坏时停止后续批次。
  - 结果只报实测观测值，不夸大。
  - 标题「深度清理 — 高级」/“Deep clean — advanced”。
- 键盘（L2690-2730）：deep_view==1 时 Enter 触发 930/931（focus 切换），Esc 关面板；F10 保持走 activate(104)（自然被守卫/面板拦截）。
- CI 触发 L2146-2150 原样保留（MEMREDUCT_CI_DEEP → activate(104)）——注意 mwfx84el 删了 native 的 `mr_ci_deep_requested` 定义；若构建链接报 undefined，这就是卡点：escalate，不代改对方文件。

**构建**：`bash scripts/ci/build-production-clean-gui.sh`（worktree 内）→ 产物 build/ 下，sha256 记录。
**部署**：备份 `sudo cp -a /usr/bin/memreduct-zamak /usr/bin/memreduct-zamak.before-s5` → 哈希核对 → 单文件替换 → 记录前后 sha256。
**验收**：不带 MEMREDUCT_CI_DEEP 跑正式 GUI（真 Hyprland；GUI 点击 Canvas 需用户或探针路径）：验证 ①默认关闭（菜单行 disabled/提示）②开启后点深度清理先出说明面板不直接执行 ③执行时 UI 不阻塞 ④结果文案只报实测 ⑤即时/30s/数分钟 MemAvailable+PSI+vmstat pswpin/out+前台响应。
**完成契约**：send_message 父会话 + 按「## 开发描述/## 改动清单/## 自测情况/## diff 概要」格式收尾（board_report 不可用）。

## 卡点

上下文再次熔断（本会话已两次）。接手者：读本文件「完整实现设计」节直接动笔，第一步先跑 `git -C <worktree> status --short` 确认 mwfx84el 改动现状。
