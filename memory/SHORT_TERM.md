# 短期记忆 / Latest Handoff

日期：2026-10-25；任务：项目文档治理、GitHub 初始化准备和 Codex 协作交接。

## 执行范围
在保留 v0.2.0 源码的前提下，建立 `AGENTS.md`、PRD/架构/安全/测试/路线图、ADR、协作方式、三层记忆、Issue/PR 模板和工具脚本。

## 事实和证据
- 源码基线来自此前本地 `memreduct-linux-zamak-v0.2.0-source.zip`，未从 GitHub 拉取。
- `python3 -m unittest discover -s tests -v`：3 passed（本轮）。
- 未执行 `zam check`, `zam test`, `zam build`：缺少 `zam`；npm 官方包查询 EAI_AGAIN（DNS）。
- Github 新建仓库：**NOT DONE**；连接器无创建仓库接口，浏览器额外服务返回 `insufficient_credits`。待本机 `gh` 初始化。
- 特权代码未经真实机器写入内核测试；CI 不得执行内核级清理。

## 代码审查中发现的待修点
1. `src/main.zm`: 自动清理原型开启后可能频繁弹 Polkit 授权；不能在安全复审前作为默认/推荐功能。
2. `src/memory.zm`: 丢失必要键时与真实 0 混淆；缓存估计不等于可直接回收页数。
3. `native/async_privileged.c`: 子进程等待错误/重入/退出处理需要覆盖更多测试；应避免假成功。
4. `native/memreduct_helper.c`: 必须确保特权路径所有权、授权策略和执行语义有端到端验证。
5. CI：需先核实 Zamak 0.2.0 构建可用，不能拿 C 测试合格代替完整构建。

## 下轮任务入口
`prompts/CODEX-P1.md`；Codex 实测后覆盖本文件：提交 SHA/工作树、命令输出、尚未解决项、下一任务。不要擦除历史事实，可在 `memory/SESSION_LOG.md` 留一条简要审计记录。
