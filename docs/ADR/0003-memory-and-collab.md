# ADR-0003: GitHub + 三层记忆 + 双 AI review

Status: Accepted as requested workflow (2026-10-25)

**Decision**: `AGENTS.md` 常驻入口；`memory/LONG_TERM.md` 长期稳定、不自动漂移；`MEDIUM_TERM.md` 里程碑；`SHORT_TERM.md` 每轮证据；`SESSION_LOG.md` 追加历史。GitHub PR/Issues 做跨聊天事实锚点。

**Roles**: Codex 唯一实现 owner，ChatGPT 评审 owner，用户批准重要变更与发布；禁止在同一文件同时写入。

**Gate**: Coding → actual tests → Commit → Push → remote SHA verify → memory update → PR review → user merge/release approval。
