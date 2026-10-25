# Codex Desktop — P0 GitHub bootstrap（完整可复制任务）

你是 Mem Reduct Linux (Zamak Native) 的实现 Agent。阅读 `AGENTS.md` 与 `memory/{LONG_TERM,MEDIUM_TERM,SHORT_TERM}.md`、`docs/PRD.md`、`docs/ARCHITECTURE.md`、`docs/SECURITY.md`、`docs/GITHUB_BOOTSTRAP.md`，不要从旧聊天猜规则。

**目标**：在用户已登录 GitHub CLI 的本机把当前工程建成 PUBLIC GitHub 仓库 `MilchstraBe-Endless10/memreduct-linux-zamak`，推送 main 并核验远端 SHA。不得操作别的现有仓库。

**约束**：先打印 `pwd`、`git status`、`gh api user --jq .login`。目标账号不匹配，立刻停止并说明；公开行为已经由用户在聊天中要求，但 CLI 的确认步骤不可绕过。执行 `scripts/publish-github.sh` 或按文档等价执行。不允许强推，不要上传任何 secrets/个人数据。

**检查**：`python3 scripts/verify_project.py`; C 单测；语法检查；`git rev-parse HEAD` 和 `git ls-remote origin refs/heads/main` SHA 一致；`gh repo view ... --json visibility,url,defaultBranchRef` 确认 PUBLIC。若失败，不能说建仓成功。

**交付**：直接提供仓库 URL、分支、SHA、完整测试/命令结果、GitHub CI 状态、必要的用户批准点；更新短期记忆与 Session Log。P0 完成后创建 P1 编译验收 Issue；不要擅自增加功能。
