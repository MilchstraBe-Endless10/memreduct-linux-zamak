# PR 集成准备度审计

审计时间：2026-10-27；PR 元数据、Review、Actions 和只读合并模拟对应本轮开始时的远端快照。仅使用 GitHub PR 元数据、现有 review/Actions 结果、本地远端引用和 `git merge-tree` 的只读合并模拟；没有合并、改写或推送分支。

## 当前远端快照

本地与 `origin` 的工作分支 HEAD 均为 `556a727c100aad4af1a25c48c10513c4dd42c5c7`，工作树干净。GitHub 当前可见的五个 PR 均为 OPEN / Draft，GitHub 的 mergeability 字段均为 `MERGEABLE`；这只表示 PR 的当前 base/head 能自动合并，不代表已获审查批准，也不代表 stacked PR 的最终集成无冲突。

| PR | Head 分支 / SHA | Base 分支 / SHA | 当前 CI 证据 | 最新复审结论及待办 |
|---|---|---|---|---|
| [#2 P1 构建修复](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/2) | `fix/issue-1-zamak-build` / `11b0b9444cd94f0026b855abe3849a06106426f2` | `main` / `080417e153d91bf2adac756e5154311fef49333f` | [40170875463](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170875463)：Linux build-and-test 成功 | ChatGPT 复审称编译/构建改动可供用户考虑批准；安装、卸载、Polkit 与真实内核清理路径没有验收。包装器文件名校验是非阻塞建议。不能据此开启清理功能。 |
| [#4 隔离 GUI 测试](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/4) | `test/issue-3-isolated-gui` / `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1` | P1 分支 / `11b0b9444cd94f0026b855abe3849a06106426f2` | [40192531809](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40192531809)：Xvfb/XTEST 与 Weston 各 1 job 成功；[40192531948](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40192531948)：build 成功 | P2A-R6 独立复审接受测试里程碑。它仍是探针测试，不代表生产 UI 验收。合入主线前要修正 PR base 对应的 workflow 触发分支，并完成下面列出的记忆冲突解决。 |
| [#5 P2A UI 规格](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/5) | `docs/issue-3-p2a-ui-spec` / `d89d74fde433da1863b4de0223f974c824fb590e` | P1 分支 / `11b0b9444cd94f0026b855abe3849a06106426f2` | [40198617901](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40198617901)：build-and-test 成功 | 最后一次 P2A 规格复审为条件接受；复审提交时的 head 早于当前 `d89d74f`。当前 head 有 CI 结果，但尚无针对该 SHA 的新 ChatGPT 复审。 |
| [#6 P2B 生产主窗口](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6) | `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888` | #5 分支 / `d89d74fde433da1863b4de0223f974c824fb590e` | [40208944364](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208944364)：生产 Xvfb/XTEST、Weston 生命周期 2/2 成功；[40208944120](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208944120)：build 成功 | P2B-2 技术范围接受；Windows 像素级验收不接受。Review thread `discussion_r4371210356` 仍未关闭且非 outdated；复审说明采样失败重绘已修复，集成前应核对修复证据并关闭/回复该线程。其他两个主窗问题线程为 outdated。 |
| [#7 P2C 设置界面](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) | `feat/issue-3-p2c-settings-ui` / `556a727c100aad4af1a25c48c10513c4dd42c5c7` | #6 分支 / `911a3d0b99b05f15bd0109dca475b0b5174e7888` | [40456081269](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40456081269)：Windows 原版采集、生产 Xvfb、比较 3/3 成功；[40456081264](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40456081264)：P2B Xvfb/Weston 2/2 成功；[40456081238](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40456081238) 与 push run [40456075786](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40456075786)：build 成功 | 最新 P2C-4R1 复审 [5662810728](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#pullrequestreview-5662810728) 接受受限的主题/语言语义修复与 CI；不是 1:1、产品功能或合并批准。P2B 作业内重复五页步骤被条件跳过；同 SHA 的 P2C 作业实际执行了五页覆盖。#7 当前 review threads 均 outdated。 |

上述每个 PR 均未合并。Actions 的成功结论只用于表中列出的具体 SHA/Run，不推导出 PR 审批。

## 文件范围与重叠

| PR | 变更范围 | 集成审查重点 |
|---|---|---|
| #2 | 8 个文件：`zam.toml`、Zamak 内存解析、Arch 安装/构建入口、治理检查、测试计划和记忆 | 构建修复范围窄；安装/卸载和特权 helper 仍未在真实系统执行。 |
| #4 | 12 个文件：独立 workflow、Canvas probe、Xvfb/Weston 脚本、来源记录、测试说明与记忆 | 测试专用。原有 9 个事件路由测试函数保留，新增 3 项几何/语言测试，总计 12 项。 |
| #5 | 14 个文件：上游 UI 规格、交互矩阵、兼容性、捕获清单、AGENTS/PRD/架构及记忆 | 规格与项目规则；保留 Windows v3.5.2 唯一基准及 Linux API 语义边界。 |
| #6 | 16 个文件：生产 `src/main.zm`、`memory.zm`、`policy.zm`、几何、C UI 测试桥接、生产 Xvfb/Weston workflow 与报告 | 真实生产主窗口测试，不是 probe；内存清理仍关闭。 |
| #7 | 28 个文件：生产设置 UI 与几何、C 测试桥接、Windows 采集/比较脚本、P2B/P2C workflow、视觉报告/交互矩阵/记忆 | 生产设置页和 Windows/Linux 证据比较；没有新增生产清理路径。Windows 原生二进制仅在隔离 Runner 验证，未列入仓库文件；Windows 专有字体未打包。 |

### PR #4 能力是否重复

没有把 PR #4 的 `tests/gui/canvas_probe/`、`scripts/ci/p2a-*` 或 `.github/workflows/p2a-r4-isolated-gui.yml` 复制进 #6/#7。生产分支确实有各自的 Xvfb/XTEST 和 Weston 作业，但它们启动正式 Mem Reduct UI，验证的是生产菜单、统计、设置页和生命周期；#4 验证的是隔离 Canvas probe 的事件路由与测试机制。两者有输入/生命周期主题上的部分重合，代码、对象和断言不同，不能互相代替。最终主线必须保留 #4 的 12 项探针测试，也必须保留 #6/#7 的生产 GUI 检查。

### 只读合并模拟

本地 `origin` 引用已刷新至上表 SHA。使用 `git merge-tree --write-tree` 对 sibling PR 进行三方合并模拟，没有切换分支或写工作树：

- #4 + #5：`memory/SESSION_LOG.md` 与 `memory/SHORT_TERM.md` 均出现内容冲突。
- #4 + #6：相同两个记忆文件冲突；这是因为 #6 的祖先含 #5 的记忆变更，而 #4 从 P1 独立分支出发。
- #4 + #7：相同两个记忆文件冲突。
- #5 → #6、#6 → #7 是父子依赖，不属于 sibling 合并；不要将 #7 直接基于 P1 来绕过父级代码。

模拟未写入索引或工作树；冲突必须人工保留各阶段日志，不得整文件选择任一侧覆盖另一侧。

## 建议的集成顺序

此顺序是待用户明确批准的候选方案，不是合并指令。为保留 PR 祖先与可审查的增量，建议使用仓库允许的 **merge commit**；不要 squash 后假设子 PR 的旧 base commit 仍在主线祖先图中，也不要强推重写堆叠分支。

1. **#2 → `main`。** 用户批准后先合入 P1。等待 exact-main build/governance/C safety CI；单独审查安装脚本与 `--dry-run` 结果。不得执行真实清理或安装特权服务。
2. **#4 → `main`。** 将测试 PR 的目标改为 `main` 前，先让 workflow 的 `pull_request.branches` 与新 base 一致；push 事件也须实际运行。确认 12 个 probe tests、Xvfb/XTEST、Weston、两个证据 artifact 与 build job 均成功。保留九个原测试函数。
3. **#5 → `main`。** 将 P1 的已合入祖先和 #4 更新纳入规格分支，人工合并 `SESSION_LOG`/`SHORT_TERM` 冲突。Review 完整 UI 文档、上游来源/许可证、长期规则；在 main 上通过 build/governance/C safety。
4. **#6 → `main`。** #5 合入后将 #6 base 改为 main（在相同祖先仍可见时无需重写提交）。运行生产 Zamak check/test/build、主窗口 Xvfb/XTEST、Weston 生命周期和项目 C/Python 安全检查。复核并处理非 outdated 的 memory invalidation review thread。不得将 Weston 生命周期写成 Wayland 输入验收。
5. **#7 → `main`。** #6 合入后再改 base；运行 Windows 原版来源/状态采集、Linux 生产五页 Xvfb/XTEST、逐页比较、几何/栅格阈值以及 P2B 回归。条件跳过步骤必须由同 SHA 另一项实际执行覆盖，并逐项核查 job；整页 1:1 仍 NOT PASS，不能变为通过声明。
6. **最终整合验收。** 从新 `main` 运行 Arch-native 构建与包内容检查，确认所有五 PR 的关键能力均仍存在。合并后的 main 再跑完整 Zamak/治理/C/Python/shell/Arch 验收，不把单个 PR 的旧 CI 代替最终 SHA 验收。

如果 #4 不能在原 PR 中安全改 base/触发 workflow，就暂停它的合入，先修正触发条件并重新审查；不可以因 #6/#7 有生产 GUI 测试而删除 #4。

## 安全、许可证与最终构建检查

- CI 使用 GitHub-hosted runner；不得启用 `pull_request_target`、仓库写权限、项目密钥、root GUI 或工作站桌面控制。#4 只需 `contents: read`。
- P1 helper 与真实系统写操作的权限边界仍须独立检查；本次审计没有执行 Polkit、drop_caches、Swap/ZRAM 写入、自动清理或后台服务安装。
- 核对 Mem Reduct 源码/截图归属与许可证声明；上游 Windows 可执行文件只在 Runner 验证，不提交仓库；不复制 Windows 专有字体；检查所有截图/artifact 不含个人路径、令牌或敏感数据。
- 以干净 Arch Linux/Omarchy 构建环境执行 `zam --version`（固定 0.2.0）、`zam check .`、`zam test .`、`zam build .`、`scripts/build-arch.sh`、`python3 scripts/verify_project.py`、`python3 -m unittest discover -s tests -v`、`bash -n scripts/*.sh scripts/ci/*.sh`，并核对生成包文件清单。此环境只做构建和无特权 UI 测试，不安装 root 服务、不运行清理、不修改 Swap/ZRAM。
- 真实 Omarchy/Hyprland 输入验收另在专用可丢弃 VM 或明确批准的隔离目标中进行；本机活动 Hyprland IPC、桌面输入注入与主题配置不属于验收手段。

**当前结论：** #2/#4/#5/#6/#7 的工程范围都有可核验 CI；#2 可供用户考虑批准，#4 测试里程碑已接受，#5 仍需按当前 head 重新复审，#6 有未关闭的非 outdated 线程需整理，#7 的 P2C 证据范围已接受。所有 PR 仍 OPEN/Draft，不能合并，且任何 GitHub 的 `MERGEABLE` 状态都不构成用户授权。

### 本轮后续跟踪

P3A-pre 的独立跟踪 Issue 已创建为 [#8](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/8)。这份报告中的 PR 状态、SHA、CI 和 Review 是上述审计快照；后续提交导致的 head/check 变化需在执行集成前重新核对。

本报告和 ADR 随后提交至 PR #7。提交 `b1f2fd95333a26af1c3ea51e6fea4a245db640c9` 的 P2C/P2B/build-and-test PR/Push 检查均通过；该 head 仅有文档/记忆增量。最新 PR 状态与当前 head 以 GitHub PR 页面为准，不能把表格中的 `556a727` 当作当前 head。
