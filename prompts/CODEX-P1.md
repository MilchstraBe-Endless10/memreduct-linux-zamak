# Codex Desktop — P1 Zamak 构建和首个 Hyprland 验收（完整可复制任务）

你是实现 owner。先读取 `AGENTS.md`、全部三层记忆、`docs/PRD.md`、`docs/ARCHITECTURE.md`、`docs/SECURITY.md`、`docs/TEST_PLAN.md` 和 ADR；检查 `git status --short --branch`, `git rev-parse HEAD`, `git remote -v`，报告 baseline。

## 目标
**只做 P1**：在 Arch/Omarchy 实际安装固定 Zamak 工具链并跑通 `zam check .`, `zam test .`, `zam build .` 和 Qt 替换为 Zamak GTK3 GUI 窗口启动。修复真实编译/API 问题，不新增 ZRAM、自动后台服务或非相关功能。

## 硬约束
- 不能运行真实 `drop_caches` 写操作，不能修改 root 服务/Swap；若要安装 root helper 需获得用户批准。
- 不能通过直接替换 `src/main.zm` 为静态 UI 或注释关键接口“骗过”验收；保留当前原型行为。
- 编译器缺失必须修复安装或给出完整 blocker，不得跳过却标 PASS。
- 不能编造 GUI 截图、测试结果。图形验收真实使用 Wayland/Hyprland，并记录 `XDG_SESSION_TYPE`、GTK 后端、Zamak/编译器版本。

## 测试
- `python3 scripts/verify_project.py`
- `python3 -m unittest discover -s tests -v`
- `bash -n` 检查所有脚本
- `zam --version`, `zam check .`, `zam test .`, `zam build .`
- GUI：以普通用户打开、刷新数值、按钮操作使用 mock/dry-run（不触及真实内核）。验证窗口未卡死、取消/失败可见。

## Git 与报告
开 `feat/p1-zamak-build` 分支，最小提交，CI 若能运行则附运行链接。不修改 main。记录每条命令真实结论；更新短期记忆，必要时补充 ADR；推送到远端并核验 SHA，创建 Draft PR 待 ChatGPT 复审（若公共仓库尚未创建，暂停推送并先完成 P0）。最后给出完整报告：baseline SHA、HEAD SHA、改动文件、测试证据、阻塞、风险和下一任务建议。
