# AGENTS.md — Mem Reduct Linux / Zamak Native

> 每个 Codex 会话的第一入口。规则优先级：用户当前明确要求 > 本文件 > `memory/LONG_TERM.md` > `docs/` 内正式批准的决策 > 中/短期记忆。冲突时先报告，不得私自覆写产品边界。不要把过去的对话摘要当成代码事实。

## 1. 项目身份
- Linux 原生开源内存管理工具，暂定名 **Mem Reduct Linux — Zamak Native**；非 Henry++ 官方移植，不可暗示获得原作者认证。
- 目标：Arch Linux / Omarchy / Hyprland (Wayland)，不用 Docker，不用 WebUI，优先中文桌面 GUI；免费，GPL-3.0-or-later。
- 主技术：**Zamak 0.2.x** (先锁 0.2.0)、`std/ui` (Linux GTK3/libui-ng)，小体积 C FFI 和独立受权限约束的 helper。
- 产品第一优先：用户点击后**真实执行**缓存回收/后续安全压缩策略，不是监控器或磁盘清理器。
- 严格区别“可用 RAM 变化”“回收页缓存”“ZRAM 压缩节省”与“进程 RSS 降低”，不得用统一的“释放 GB 数量”混淆。

## 2. 禁止事项（硬约束）
- 不得终止/暂停用户进程来伪装释放内存；不得静默 swapoff、重建 Swap、自动清除热缓存；不得承诺零 CPU/I/O/延迟影响。
- 不得让整个 UI 使用 root；不得把动态任意命令、文件路径或 shell 参数传给特权 helper；不得 `curl | sh` 安装未知代码。
- 不得启用高频全局 `drop_caches` 默认定时器；自动模式在安全策略验收前必须关闭、不能偷偷激活。
- 不得对真实 `/proc/sys/vm/drop_caches` 进行 CI、单元测试或未经授权的现场压力测试。
- 不得制造无证据的性能结论、Stars、用户规模、测试截图、CLI 结果。不可将 `zam` 缺失标注为“全部测试通过”。
- 不得强推、删除远端分支、合并未通过审查的 PR；不得擅自公开用户数据、密钥或本机路径。仓库为 PUBLIC，提交前必须审查敏感数据。
- 未获用户进一步明确批准，不得对用户日常机器运行破坏性内存测试、修改 Swap/ZRAM 或安装 root 级后台服务。

## 3. 开工必读（按顺序）
1. `memory/LONG_TERM.md`（长期约束）
2. `memory/MEDIUM_TERM.md`（里程碑、阻塞、阶段）
3. `memory/SHORT_TERM.md`（最近一次执行的证据）
4. `docs/PRD.md`、`docs/ARCHITECTURE.md`、`docs/SECURITY.md`
5. `docs/ADR/`（正式技术决策）及 `docs/TEST_PLAN.md`
6. `git status --short --branch; git log -1 --oneline; git remote -v`，再检测 `zam --version` 和依赖；如无 Git 远端，不要推测已发布。

## 4. 每轮执行协议
- **Scope**：只领取一个有明确验收标准的 Issue 或阶段任务，列出要改的文件及其所有权；先记录 baseline。
- **Implement**：小提交、可回退，避免混入无关改动；新特权能力先 ADR + 威胁模型，后代码。
- **Verify**：至少 `python3 -m unittest discover -s tests -v`、`bash -n scripts/*.sh`、`zam check .`、`zam test .`、`zam build .`（如环境缺依赖，标注 NOT RUN + 原因；不可跳过不说明）。
- **Document**：同步 ADR（有设计变更才写）、`memory/SHORT_TERM.md`（每轮）、`memory/MEDIUM_TERM.md`（阶段变化）、测试结果。
- **Git**：功能分支 `feat/issue-N-description` 或 `fix/issue-N-description`，正常开发 PR→CI→Review→Merge。每次可审查变更建议 Commit + Push；公网发布、合并、强推、tag/release 需要用户批准。
- **Hand-off**：输出分支/HEAD SHA、改动摘要、测试命令和真实结果、已知风险、下一步、需要 ChatGPT 重点复审的文件。绝不写“已经远端推送”除非能核验远端 SHA。

## 5. 明确的角色
- **Codex Desktop**：单一工作树的实现 Owner；负责编译、测试、截图、故障排查、Git PR。
- **ChatGPT**：产品/架构审查和任务规划；独立看 diff 与报告，对安全边界/数据和指标真实性提意见；不在 Codex 正写同一分支时并发改相同文件。
- **用户**：产品优先级、是否公开、特权机制风险、重要架构变更、最终 merge/release 的批准人。
- 不能假定两个 AI 能直接实时互聊。**GitHub Issues/PR + docs + memory/** 是可审计的异步交接媒介。

## 6. 完成的定义
验收条款可追踪、diff 可读、回归测试通过、Wayland 目标设备上有明确实测（要求平台验收时）、文档与记忆同步、风险公开、PR 可审查，且功能没有超越安全边界。
