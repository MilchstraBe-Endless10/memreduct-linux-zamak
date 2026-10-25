# ChatGPT PR Review — 复制并替换 Issue/PR 链接

请独立复审 GitHub PR `<PR_URL>`，Issue `<ISSUE_URL>`，按仓库 `AGENTS.md`、长期记忆、PRD、ADR、SECURITY/TEST_PLAN 与 Codex 实际 diff/CI/日志判断。不要仅重述 Codex 自评。

依次评审：需求范围 / 许可证 / API 版本 / 编译及测试证据 / 内核特权边界 / 自动清理权限与频率 / MemAvailable 与实际释放数字的真实性 / Hyprland 平台证据 / 失败恢复 / Git 变更。

输出：Blocking / Major / Minor 三种问题，标注具体文件与行号，提供能直接交给 Codex 的单次修复任务块；若证据不足写 NOT VERIFIED；决定 PR 是否可以交由用户批准合并，不自行声称用户已同意。
