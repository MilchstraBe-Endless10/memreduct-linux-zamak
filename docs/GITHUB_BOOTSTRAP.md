# 公共 GitHub 仓库创建与首次推送

## 当前事实
- 目标账号：`MilchstraBe-Endless10`；仓库建议：`memreduct-linux-zamak`，PUBLIC。
- 2026-10-25 已创建 PUBLIC 仓库：<https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak>；默认分支 `main`。初次 bootstrap 提交 `02ca561b41d55611a131338bdca50243e78c4aa7` 已与远端 `refs/heads/main` 核验一致。
- 首轮 GitHub Actions `build-and-test` 在 `zam check .` 失败：`project.name must be an identifier`；Zamak 0.2.0 编译验收见 [P1 Issue #1](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/1)。

## 在 Omarchy 的一次性操作
```bash
sudo pacman -S --needed git github-cli
# 若未登录：gh auth login
# 下载并解压当前治理版源码 zip，cd 到仓库根目录
bash scripts/publish-github.sh
```

这个脚本会检查当前登录账号、要求输入 PUBLIC、审查 git 操作环境、创建同名公开仓库并推送；若远端已存在或已关联仓库，先退出并人工检查，不能覆盖。

## 采用网页创建仓库的备用方案
1. GitHub → New repository → `MilchstraBe-Endless10/memreduct-linux-zamak` → **Public**。
2. 选择**不初始化 README / .gitignore / license**（本包已包含）。
3. 在源码根目录执行 `git init -b main; git add -A; git commit -m 'chore: bootstrap Zamak architecture and governance'`，完成后 `git remote add origin git@github.com:MilchstraBe-Endless10/memreduct-linux-zamak.git` 与 `git push -u origin main`。
4. `git rev-parse HEAD` 和 `git ls-remote origin refs/heads/main` 的 SHA 应一致；`gh repo view MilchstraBe-Endless10/memreduct-linux-zamak --json visibility,url,defaultBranchRef` 应为 PUBLIC。

## 首次推送后的配置
- Settings → Branch protection / Rulesets：`main` 强制 PR（不少于一名审核者），禁止 force push / deletion，要求 `build-and-test` CI。不要在 CI 名称未确认前随意配置不存在的检查。
- 开启 GitHub Issues、Discussions（可选）、Dependabot/security alerts；按 `docs/ROADMAP.md` 建 P1–P6 Issues，给 P1 优先级。
- 拒绝提交 access token、private logs、SSH keys、真实个人路径、调试抓包内容等。公开前通读 `git diff --cached --stat` 和敏感扫描输出。
- 不创建已运行 GitHub Release tag：只有源码发布与 CI 绿、实机 GUI 验证后才做发行版。
