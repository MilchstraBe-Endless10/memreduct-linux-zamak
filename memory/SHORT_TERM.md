# 短期记忆 / Latest Handoff

## 最近记录：P2B-1 生产主窗口候选与 CI 复测（2026-10-25）

- PR #5 的 R7 规格修正 head `d89d74fde433da1863b4de0223f974c824fb590e` 已核对，仍 OPEN / Draft，CI 2/2 PASS；正式 `IDD_MAIN=224×214 DLU` 为主窗口模板，每个 `IDD_SETTINGS_* =260×168 DLU` 是子页模板。R6 Actions `40192531809` / `40192531948` 与对应 push runs `40192528601` / `40192528611` 已记录。没有合并 PR #2/#4/#5。
- P2B 独立分支 `feat/issue-3-p2b-main-window` 从 PR #5 修正规格分支派生；PR #6 OPEN / Draft，base 为 `docs/issue-3-p2a-ui-spec`。生产代码 head `a6df90389a69fc9cad12272c2daa2f273c834385` 通过 PR GUI run `40199653180`、build-and-test run `40199653440`；push GUI/build runs `40199649320` / `40199649284` 也通过。远端分支 SHA 已核对。
- 生产 `src/main.zm` 以真实 `/proc/meminfo` 显示 RAM/Swap，System working set 显示不可用；英文/简中独立切换；清理按钮和八项 Windows 清理选择全部禁用，不触发 Polkit 或内核写操作。DLU 转换参数集中在 `src/ui_geometry.zm`，仍为未校准估算。
- Xvfb/XTEST 实测正式窗口：top-level 337×369、Canvas 313×345，scale 1；点击语言菜单、split button、禁用区域、Tab/方向键/Enter/Escape 与 Alt+F4 close 均通过，退出码 0。实际截图及 SHA-256 在 `docs/P2B-1_PRODUCTION_UI_REPORT.md` 和 artifact `12601156413`。
- Weston 13.0.0 Headless/Pixman 私有 Wayland socket 下正式客户端连接、绘制、读 meminfo、退出码 0；不测试 Wayland 输入。诊断保留：一次 `gdk_seat_get_keyboard` critical、`wl_seat` 未公告；Weston SIGTERM 清理日志含 `BUG: finalizing a layer with views still on it.`，尚未确认为应用故障，不隐藏日志。
- 本机 `verify_project.py` 29 项、C 安全单测 3 项、shell syntax、Zamak 0.2.0 check/test/build（7/7）、`scripts/build-arch.sh`、`git diff --check` 均 PASS。没有在主机启动 GUI、访问活动 Hyprland IPC、操作用户桌面或执行特权清理。
- Windows 原生 v3.5.2 截图、真实字体/DPI DLU 校准、配对 overlay/2 px 验收仍 BLOCKED / NOT RUN；Hyprland 鼠标/键盘、HiDPI、设置页与托盘未实现。PR #4 保留独立 12/12 探针测试，未并入 PR #6。

## 历史记录：P2A-R6 规格入库与隔离 GUI CI（2026-10-25）

## 本轮基线与结果
- 文档分支 `docs/issue-3-p2a-ui-spec` 独立基于 P1 `11b0b9444cd94f0026b855abe3849a06106426f2`，不包含 PR #4 测试代码；已创建 [Draft PR #5](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/5)，base `fix/issue-1-zamak-build`，引用 Issue #3。PR 创建时 head `b94731b9a9bc52a4834358f9f71fcf86a2033925`；本次最后的记忆同步提交会推进 head，最终 SHA 以 PR 远端核验为准。
- PR #2 仍 OPEN / Draft，head `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #4 仍 OPEN / Draft，base `fix/issue-1-zamak-build`，最终本轮 head `e2d3c5e4481e9494472913f41636580369038cd8`，远端 SHA 已核对一致。
- R6 GUI Actions push [40191454446](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454446) 与 PR [40191456330](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456330) 均 PASS；build-and-test push [40191454361](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191454361) 与 PR [40191456339](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191456339) 均 PASS。Artifacts 已下载核验；含 X11/Wayland 日志、PNG、协议/几何状态和 SHA 清单。
- 9 项既有 Zamak 事件路由测试函数原样保留，新增 3 项紧凑几何/双语用例；Zamak 0.2.0 共 12/12 PASS。探针源码 SHA-256：`d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。
- Xvfb + Openbox 下实际 XTEST 验证 hover/pressed/release、按钮及箭头命中、菜单、Tab/Enter/Escape、双语和 resize；同按钮 ROI hover→pressed、pressed→release 差异均为 8004 像素。WM close 通过 Alt+F4 传递 WM_DELETE，记录 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`、退出码 0 且窗口消失。`xdotool windowclose` 实际用 `XDestroyWindow` 销毁 X 窗口，不作为 WM close 证据。
- Weston Headless/Pixman Wayland socket、312×324 Canvas 绘制、受控退出及进程退出码 0 均 PASS。每个运行记录一次 `gdk_seat_get_keyboard` critical，未公告 `wl_seat`；两者相符但因果未证实，未隐藏 GTK 诊断。

## 文档与尺寸约束
- 已整理 P2A 源码规格、互动矩阵、Linux 能力映射、版本差异、可行性及真实 Windows 截图采集清单；官方基准固定 `v.3.5.2` / `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 新增明确尺寸规则：主对话框 224×214 DLU、五个设置页模板各 260×168 DLU；生产 GUI 必须保持官方紧凑比例，不得放大。R5/R6 336×348 顶层、312×324 Canvas 仅为 96-DPI 估算的 CI 探针尺寸，不是 Windows 测量或产品目标。
- 官方 Windows 原生截图及真实 DLU→物理像素换算仍 BLOCKED；overlay/diff/2px 结果 NOT RUN。336×348 top-level / 312×324 Canvas 仅为 CI 探针尺寸。没有新增或公开私人截图/路径。

## 本机只读/非 GUI 验证
- `python3 scripts/verify_project.py` PASS（29 项）；`python3 -m unittest discover -s tests -v` PASS（3 项 C 安全测试）。
- 所有 `scripts/*.sh` 语法检查 PASS；Zamak `0.2.0`、`zam check .`、`zam test .`（5 passed）、`zam build .`、`bash scripts/build-arch.sh`、`git diff --check` PASS。
- 未启动 GUI/Compositor，未使用 Hyprland IPC、鼠标键盘注入或特权清理接口；未触碰 Swap/ZRAM/Polkit。
- 规格分支更新官方 3.5.2 源码规格、资源/交互矩阵、Linux 语义、安全边界、真实 Windows 截图缺口、Zamak 技术结论及紧凑尺寸约束；长期规则写入 AGENTS/PRD/ARCHITECTURE/LONG_TERM。PR #5 创建时的源码规格 head `90d62100e840aa408e4ea99cbbd866ef2025e99e` 上，build-and-test push `40191896559` 与 PR `40191901022` 均 PASS。记忆同步提交会使 head 前进，但不改程序源码；最终 branch SHA 以远端校验为准。Windows release 截图和真实像素比较仍 BLOCKED。
- 未启动本机 GUI 或 compositor，未访问 Hyprland IPC，未执行 Polkit、内存清理、Swap/ZRAM 操作。PR #2/#4/#5 不合并；P2B 不开始。下一步等待 PR #5 Actions 完成并交付两个 Draft PR 供复审。

日期：2026-10-25；任务：P1 Zamak 0.2.0 编译与原生构建验收。

## 执行范围
按 Issue #1 修复 Zamak 0.2.0 编译/构建问题，保留现有 GUI 与特权边界；运行本机 Arch/Omarchy 验收，准备关联 Issue 的 Draft PR。工作分支 `fix/issue-1-zamak-build`，基于 `origin/main` / `080417e153d91bf2adac756e5154311fef49333f`。未修改 main，未扩展功能。

## 实际改动
- `zam.toml`：Zamak `project.name` 从非法连字符形式改为合法标识符 `memreduct_zamak`；产品显示名、仓库名、安装命令和 helper 仍保留 `memreduct-zamak`。
- `src/memory.zm`：将不存在的无参 `split()` 调整为 Zamak 0.2.0 支持的 `words()`，并正确传播解析错误。
- `scripts/install-arch.sh`：按 `memreduct_zamak-<hex-hash>` 匹配 GUI 二进制，避免把 `-test-` 测试产物当作应用。
- `scripts/verify_project.py`：将 manifest 标识符检查同步为 `memreduct_zamak`。

## 本机验证证据
环境：Omarchy 4.0.6、Hyprland 0.56.2、Wayland、GTK 3.24.52、Zamak 0.2.0。

| 命令 / 验收 | 状态与实际结果 |
|---|---|
| `python3 scripts/verify_project.py` | PASS，29 项检查。 |
| `python3 -m unittest discover -s tests -v` | PASS，3 项 C 安全测试。 |
| `bash -n`，逐个检查 `scripts/*.sh` | PASS，4 个脚本。 |
| `zam --version` | PASS，`0.2.0`。 |
| `zam check .` | PASS，`CHECK OK memreduct_zamak`。 |
| `zam test .` | PASS，5 passed，0 failed。 |
| `zam build .` | PASS，输出 `build/memreduct_zamak-00ee9d44b77125b6721133332f35be3f1f9d3d518e7ab20b40d8265940928d57`。 |
| `bash scripts/build-arch.sh` | PASS；Zamak 检查/测试/构建及 C Helper 均成功。 |
| Helper `--dry-run` | PASS；未修改内核状态。 |
| Hyprland GUI smoke test | PASS；普通用户 Wayland 窗口，中文/内存数据/控件/刷新可见，`xwayland=false`，正常关闭，退出码 0。未点击清理、未启用自动清理。 |

安装器候选扫描只匹配 GUI 产物；之前的动态库检查未发现缺失库。未执行涉及 `/usr` 的安装/卸载；Polkit E2E、真实 `drop_caches`、Swap/ZRAM 操作均未运行。无任何真实内核清理写入。

## GitHub / 下一步
- 当前已验证 `gh` 账号为目标 `MilchstraBe-Endless10`；目标仓库仅为 `MilchstraBe-Endless10/memreduct-linux-zamak`。
- 代码提交 `1d4654867f59154981010007288cff1fda42f457` 使用 GitHub noreply 邮箱；文档验收记录后续提交 `9b5177e992eb1c86b72643a0ae2c0cda9a570555`。记录该状态时，`git ls-remote` 与本地 HEAD 相同，工作树干净。
- Draft PR #2：<https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/2>，base `main`，该次核验的 head 为 `9b5177e992eb1c86b72643a0ae2c0cda9a570555`，正文 `Refs #1`。PR 仍为 OPEN / Draft。
- Actions `build-and-test` 对代码提交成功：push run [40170617141](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170617141)，pull_request run [40170634633](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170634633)。只含文档记录的后续提交也成功：push run [40170749833](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170749833)，pull_request run [40170752581](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170752581)。
- 等待 ChatGPT 复审及用户决定是否合并；不得自动合并或进入 P2。
