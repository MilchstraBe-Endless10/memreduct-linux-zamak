# PR 集成准备度审计

审计时间：2026-10-27。下方表格保留开始审计时的快照（P2C head `556a727...`）；文末补充了随后精确核验的 `663a9d3...` 快照。PR 元数据、Review、Actions 和只读合并模拟来自 GitHub 与本地远端引用；集成模拟没有写入工作树、修改 PR base 或执行合并。本报告作为既有 PR #7 的文档增量交付。

## 初始审计快照（P2C head `556a727...`）

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
| #7 | 30 个文件（当前 GitHub changed-file 列表）：生产设置 UI 与几何、C 测试桥接、Windows 采集/比较脚本、P2B/P2C workflow、视觉报告/交互矩阵/记忆 | 生产设置页和 Windows/Linux 证据比较；没有新增生产清理路径。Windows 原生二进制仅在隔离 Runner 验证，未列入仓库文件；Windows 专有字体未打包。 |

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

## PR #7 当前快照补充（2026-10-27；核验 head `663a9d3e627ccb2d9bcb884cd708f140006f58a5`）

PR #7 仍 OPEN / Draft，base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；当前 GitHub 文件列表为 30 项。`663a9d3` 只更正了 P2C 报告中 `b1f2fd9` 的文件清单，没有生产 UI、测试脚本或 workflow 变化。

- P2C Windows 原版采集、生产 Xvfb 和视觉比较 [run `40463959137`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463959137) 的 3 个 job 全部通过。Artifacts：Windows `12715378285` (`sha256:74bb6d72079937afb32f6ff57ad384aef673d9a861e8b31f25b1280ce72032f2`)、Linux X11 `12716186528` (`sha256:11f27c8c728d6e8922b26aa36ef22a28d2fcc6a0751bade382bc34984739978d`)、比较 `12715319837` (`sha256:bfed79350d706676c6dc3ca972a73ff04d2b9e7736137d931002679fcc7811b7`)。
- P2B 主窗口 [run `40463959086`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463959086) 的 Xvfb 和 Weston 最终 attempt 2 均通过。首次 Weston attempt 因 15 秒内没有出现有界退出标记而超时；该失败日志与 artifact `12716375114` 保留。只重跑失败的 Weston job 后通过，最终 artifact `12716440984` (`sha256:8629bb9567ae529d431b251cd5777f845e2044662f4855ff2199060d08091704`)；X11 artifact `12716220331` (`sha256:8a7a626d1a8ffa15fc0cddb6237f5c724f52bc0fc8a3826f07b9576b75bf67d3`)。日志没有 compositor 崩溃证据；首次超时根因未确认。
- PR build [run `40463959092`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463959092) 与 push build [run `40463951002`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40463951002) 均通过；必要检查最终 7/7 通过。P2B 中重复的设置页 step 条件跳过，由同一 head 的 P2C Xvfb 作业实际覆盖。
- 该 head 的几何 184/184（最大分量偏差 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36、中间滚动 2/2 compare-only。十组 light 对照为 5.0741%–14.8853%，均优于 P2C-2；整页像素 1:1 仍 NOT PASS。Windows 深色/语言独立实验 4/4、语义页面状态 20/20、设置窗重开 4/4；Wayland 输入仍 NOT RUN。

此快照更新不更改建议的集成顺序或用户批准门槛；进入集成前仍须重新读取所有 PR 的远端 SHA、Review 和 checks。


## P3A-PRE-R1 当前复核快照（2026-10-27；审计对象 head `629fc3f88ee91d0cc3582d6be1789991fc3f72b6`）

本节更新此前过期的 #7 快照，不删除历史状态。核验时工作树与 `origin/feat/issue-3-p2c-settings-ui` 均为 `629fc3f88ee91d0cc3582d6be1789991fc3f72b6`，干净；PR #7 仍 OPEN / Draft，base #6 `911a3d0b99b05f15bd0109dca475b0b5174e7888`。五个 PR 都是 OPEN / Draft，GitHub 对各自**当前 base/head** 返回 MERGEABLE；这不是审批或合并授权。

| PR | 当前 head → base | 最新可核验检查 | 当前审查状态 |
|---|---|---|---|
| [#2](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/2) | `11b0b9444cd94f0026b855abe3849a06106426f2` → `main` `080417e153d91bf2adac756e5154311fef49333f` | build-and-test PR/Push `40170875463` / `40170872595`：2/2 成功 | 仍为 Draft；P1 编译门槛接受，安装/Polkit/内核清理未验收。 |
| [#4](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/4) | `1bca068d4efc27b04aa74a9111cd65cb5cd9bfd1` → P1 `11b0b9444cd94f0026b855abe3849a06106426f2` | GUI PR/Push `40192531809` / `40192528601`：Xvfb 与 Weston 通过；build `40192531948` / `40192528611`：通过；当前 rollup 6/6 | P2A-R6 测试里程碑接受，PR 仍 Draft。保留独立探针，不以 #6/#7 生产 GUI 测试替代。 |
| [#5](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/5) | `d89d74fde433da1863b4de0223f974c824fb590e` → P1 `11b0b9444cd94f0026b855abe3849a06106426f2` | build PR/Push `40198617901` / `40198615044`：2/2 成功 | 文档规格 PR 仍 Draft；在真正集成前须以当前源文件重做完整复审。 |
| [#6](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6) | `911a3d0b99b05f15bd0109dca475b0b5174e7888` → #5 `d89d74fde433da1863b4de0223f974c824fb590e` | 生产 Xvfb/Weston `40208944364` 2/2；build `40208944120`；当前 rollup 6/6 | P2B-2 技术范围接受；像素验收未通过。采样失败重绘线程仍未解决且非 outdated，见下文证据与拟回复。 |
| [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) | `629fc3f88ee91d0cc3582d6be1789991fc3f72b6` → #6 `911a3d0b99b05f15bd0109dca475b0b5174e7888` | P2C `40465893251` 3/3；P2B Xvfb/Weston `40465893241` 2/2；build PR `40465893268` 与 push `40465886984` 通过；PR 当前 rollup 7/7 | P2C-4R1 工程证据范围接受；整页像素 1:1 未通过，Wayland 输入未运行，托盘/清理窗口受阻，12 项简中来源差异仍 `DECISION_REQUIRED`。最新 P3A-PRE review `5663699910` 接受 P2C-EXIT/集成审计，但 ADR-0004 仍条件接受且**未批准实现**。 |

### P2C-4R1 归档基线

- 已复审历史 HEAD：`556a727c100aad4af1a25c48c10513c4dd42c5c7`；生产 UI checkpoint：`6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。
- Windows 四组隔离语言/主题顺序实验 4/4、页面语义状态 20/20、设置窗口关闭重开 4/4；控件几何 184/184（最大分量偏差 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36。
- 有效深色配对 changed-pixel 为 5.4004%–12.6625%；整页 1:1 `NOT PASS`。Wayland 真实输入 `NOT RUN`；托盘与清理弹窗 `BLOCKED`；12 项上游简中来源差异 `DECISION_REQUIRED`。
- 旧混合主题错配截图继续保留为历史无效配对；新复验没有证明其根因，也没有复现该现象，不得删除旧源图或反向声称已确认缺陷原因。

### PR #4 独立探针保留与触发器

PR #4 的 12 项独立 Canvas probe 测试（其中原有 9 项事件路由函数保持）以及它自己的 Xvfb/XTEST、Weston 脚本和来源说明在 #6/#7 中没有被复制。#6/#7 测试的是生产应用，能力对象不同；集成后两个测试族都必须保留并运行。

已从 PR #4 head 分支读取 `.github/workflows/p2a-r4-isolated-gui.yml`：

```yaml
on:
  push:
    branches:
      - test/issue-3-isolated-gui
  pull_request:
    branches:
      - fix/issue-1-zamak-build
permissions:
  contents: read
```

若 #4 的 base 未来改到 `main`，在改 base 之前须把 `pull_request.branches` 改为 `main` 并复核 `push` 的意图；不可以假设 PR 事件会因分支名称相近而触发。保持 `contents: read`、12 项测试、真实 XTEST/Weston 步骤和 artifacts，不因生产测试重叠而删除探针。

### 记忆文件冲突处理规则

只读 sibling 合并模拟确认 #4 与 #5/#6/#7 在 `memory/SHORT_TERM.md` 和 `memory/SESSION_LOG.md` 存在冲突。解决时：

1. 两侧按日期追加各自阶段记录，保留原始 SHA、测试事实和来源；不得整文件选择一侧或删除旧记录。
2. 将共同事件去重时只删除可证明完全相同的重复段，不能合并掉不同分支独有证据；每段注明来源 PR/分支和被测 SHA。
3. 最新状态另加短摘要，历史段仍保留。若无法确认事实冲突，保留两条并标为待核，不自行折中。
4. 冲突处理提交后重新运行治理检查，并在每一次实际合并后重新检查祖先关系、PR head/base 以及新的 exact-main CI。

### PR #6 采样失败重绘线程：代码与证据

线程 [`discussion_r4371210356`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6#discussion_r4371210356) 仍为未解决、非 outdated。本轮只核验证据并给出拟回复，不关闭或直接回复该线程。

- 在当前 PR #6 源码中，`src/main.zm` 的 `refresh()` 失败分支（约 327–339 行）将 `sample_ok=false`，清零旧样本字段，更新状态，随后明确调用 `s.canvas.redraw()` 再返回；test mode 记录 `CI_MEMORY_INVALIDATED redraw_requested=true`。
- `scripts/ci/p2b-production-xvfb.sh` 第 277–312 行先截取成功样本，再通过 CI-only C bridge 注入非特权采样失败，等待 `CI_MEMORY_INVALIDATED` 与 `CI_DRAW_SAMPLE state=unavailable values=em-dash`，重新截屏并用 `assert_changed` 验证 ROI 像素变化，同时要求错误必须是指定注入错误，并核对清理保持 `action=none`。
- exact-head Actions [run `40465893241`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40465893241)，Xvfb job `122061088384` 成功。关联 X11 artifact `12717146938`，GitHub digest `sha256:d9c99dcaa330aa76c25be8ef60332ad61cb082e5a951fd3ca153369ff9f8bfd5`；下载后 `client.log` SHA-256 `39ef3d24f255df6cd3ccbdd90fd7cf280105933ea130a8351aeccc84c06d08c2`，其中实际出现上述三个标记。该 artifact 对应 PR #7 head `629fc3f...` 的 CI merge ref；生产代码来自 #6 的 `911a3d0...`。源码 Git blob `src/main.zm=ef5d77d0d7a24b6ed40d5aee174b90ddb0010d05`，Xvfb 脚本 blob `bdaaf32175b4e39983633464293b388836b22b33`。
- 此证据证明隔离 Xvfb 中注入失败后 UI 发出重绘请求、进入 em-dash 状态且图像变化；它不证明 Wayland、Hyprland 或系统级写入审计。

**拟回复文本（保留给审查线程，不在本轮代替审查者关闭）：**

> Fixed in the P2B-2 source at `src/main.zm`: the failed-sample branch clears the previous values, marks the sample unavailable, calls `s.canvas.redraw()`, and only then returns. The production Xvfb test injects a later non-privileged sampling failure after live samples, requires `CI_MEMORY_INVALIDATED redraw_requested=true` and `CI_DRAW_SAMPLE state=unavailable values=em-dash`, and checks a changed readout crop. This passed on the PR #7 exact-head workflow run 40465893241 (Xvfb job 122061088384); artifact 12717146938 contains the client log and before/after screenshots. This verifies the isolated Xvfb failure-to-redraw path; it does not claim Wayland input or syscall-level kernel-write auditing. Please re-review this evidence before resolving the thread.

### P3A-PRE-R1 ChatGPT Review 对应表

| Review 要点 | 文档处理 | 仍需复审/边界 |
|---|---|---|
| [review `5663699910`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#pullrequestreview-5663699910)：P2C-EXIT 与集成审计接受；ADR 条件接受、不可实现 | 本节补入 P2C 固定验收基线与五 PR 当前快照；ADR 状态继续为 `PROPOSED`，明确不授权实现。 | P2C 的 1:1、Wayland 输入、tray/清理弹窗依旧未验收。 |
| [thread `4386187875`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#discussion_r4386187875)：rename 成功后目录 fsync 失败必须表达不确定 | ADR §2.1–2.4 增加 `VISIBLE_DURABILITY_UNCERTAIN`、安全回读、运行时协调、状态转换和注入测试。 | 不确定时不假装回滚；重试不得重放 rename。 |
| [thread `4386187883`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#discussion_r4386187883)：锁内重读异常不能被默认值静默覆盖 | ADR §2.2、§3、§6 区分损坏/不可读/未来 schema/权限与安全对象状态；普通保存失败关闭，Reset/恢复独立确认。 | future schema 需额外降级警告并先制作安全备份。 |
| [thread `4386187887`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7#discussion_r4386187887)：祖先路径、合法根 symlink、锁文件和权限 | ADR §1.1–1.2 规定根目录 fd 锚定、允许用户配置的根目录 symlink、拒绝 app/叶节点 symlink、openat2/openat 回退、固定稳定锁文件、UID/mode/nlink 检查及不 unlink。 | 仅普通用户权限；无 root、Polkit 或路径兜底写入。 |

PR #7 的五个旧 inline 线程在该快照均为 outdated；上述三个新 ADR 线程当时未解决且非 outdated。本次只记录逐项处理，不批量 resolve。PR #6 的采样线程也保持未关闭，等待审查者核对上述拟回复。

### 后续集成顺序与门槛

候选顺序仍为 #2 → #4 → #5 → #6 → #7，需项目所有者对每一次实际合并单独批准。#4 改 base 前更新并验证 workflow 分支过滤；#4 与 #5 合入时按上文 append-only 规则人工解决记忆冲突；依赖 PR 在父项合并后重读祖先关系并审查新的 diff；每次实际合并后先运行新的 exact-main CI，再开始下一项。最后从最终 main 完成 Arch 原生构建、Zamak check/test/build、项目治理、C/Python 安全与 shell 检查。此节仅为只读准备方案，不改变任何 PR base、分支或 main。
