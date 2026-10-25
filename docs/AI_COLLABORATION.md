# 你 + Codex Desktop + ChatGPT：三方协作协议

## 原则
工具之间没有自然的实时共享上下文。**GitHub Issue/PR、仓库内 `AGENTS.md`、`memory/` 和测试日志** 构成唯一可以审计的协作通道；聊天记录只辅助，不单独作为事实来源。一个文件/分支在同一时刻只能有一个写入负责人。

## 角色与读写边界
| 主体 | 责任 | 不得做的事 |
|---|---|---|
| 用户/仓库 Owner | 需求优先级、是否公开、风险签字、主分支合并与发布 | 不必手动逐条管理程序内部逻辑 |
| ChatGPT（架构审查） | 架构/PRD/ADR、安全审核、Codex 任务书、审 diff/日志/证据 | 不凭 Codex 口头描述声称测试已通过；不并行覆盖其工作文件 |
| Codex Desktop（实施） | 本机编辑代码、执行构建与测试、复现 BUG、提交功能分支及 PR、维护短期记忆 | 不擅自变更长期规则、生产自动执行或虚构测试数据 |

## 一轮闭环
1. 用户提出目标。ChatGPT 将它拆成一份 `GitHub Issue`，包括完成标准、风险、Owner 和优先级；技术结构变更先起 ADR。
2. Codex 从 GitHub 读取 `AGENTS.md`、三层记忆、Issue；开 `feat/...` 分支并报 baseline SHA。
3. Codex 实施、跑测试、更新短期记忆/阶段状态、记录**完整命令与输出**，形成 PR；如工具链缺失，诚实标记 NOT_RUN。
4. ChatGPT 获取 PR diff 和 CI/命令输出，独立审接口、安全权限、性能指标可信度、范围漂移；在 PR Review 提出问题。
5. Codex 在同一个 PR 内修复，回传新 SHA、证据；ChatGPT 再审。
6. 用户批准后合并；正常 Git 操作允许在授权范围自动完成，但**公开新仓库、强推、正式发布和 root 风险改动必须显式确认**。
7. 合并后 Codex 同步 `memory/MEDIUM_TERM.md` / `SHORT_TERM.md` 和 `memory/SESSION_LOG.md`。下次新聊天只需仓库链接 + Issue + HEAD SHA。

## 典型信息交接格式
```
repo: owner/memreduct-linux-zamak
issue: #N
branch: feat/issue-N-...
base_SHA: <sha>
head_SHA: <sha>
changed_files: [..]
checks:
  - command: ...
    status: PASS | FAIL | NOT_RUN
    evidence: ...
blockers: [...]
review_focus: [privileged-helper / policies / ui / ...]
next_step: ...
```

## 冲突处理
- 先 `git fetch` 与 `git status`; 不允许无说明的 `git reset --hard` 或强推覆盖另一端改动。
- 文档冲突以用户明确修改和已合并 ADR 为准；先由 ChatGPT 列争议点、用户裁定，再由 Codex 单点改动。
- CI 绿但缺真实主机测试，只能标 `CI-green / HOST-not-verified`；不得标 `release-ready`。

## PR 复审重点
对内核写操作的范围/权限、缓存/Swap/ZRAM 指标、程序异常退出、默认自动化行为、Hyprland 兼容性、包完整性、许可证与敏感数据泄漏进行反向审查。
