# 短期记忆 / Latest Handoff

## 最近记录：P2A-R6 隔离 GUI CI 与规格提交（2026-10-25）

- 规格分支 `docs/issue-3-p2a-ui-spec` 基于 P1 `11b0b9444cd94f0026b855abe3849a06106426f2`；已提交源码级规格首版 `367efce8dcdecb487595507ff7e9f275bedd96bc`，仍待更新最终 CI 记录和创建独立 Draft PR。
- 测试分支 `test/issue-3-isolated-gui` 的 R6 验证源码 SHA `1cb7b77246275962f092fa0c0ccc27fbf982ce8b`。PR #4 为 OPEN / Draft，base P1 分支；PR #2 为 OPEN / Draft。没有改 main、P1 功能或生产 UI。
- 保留原 9 个 Zamak 测试函数，新增 3 项紧凑几何/双语测试；Zamak 0.2.0 `check/test/build` PASS，12/12 probe tests PASS。探针 SHA `d3487e4c9a51c2a0cea6ac25f1e58e19f2d435028cef872e3827d145b96f05a3`。
- X11 修正：`xdotool windowclose` 的上游实现是 `XDestroyWindow`，不发 WM_DELETE，不能用于正常 GTK 关闭验收；调用它导致异常销毁和 GTK critical。隔离 Xvfb 下启动 Openbox 后，实际通过 XTEST Alt+F4 验证 WM close：`WM_PROTOCOLS` 含 `WM_DELETE_WINDOW`，日志顺序包含 `CI_WINDOW_CLOSE`、`CI_APP_RETURNED`，退出码 0 且 X 窗口消失。Openbox 运行后 `xdotool windowquit` 尝试仍未触发回调，未将其标为通过。hover-only 与 held-press ROI、pressed 与 release ROI 像素差各为 8004。
- R6 Push/PR CI：GUI runs [40191087455](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191087455) / [40191091618](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191091618) PASS；build runs [40191087528](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191087528) / [40191091733](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40191091733) PASS。Artifacts 附有截图、ROI 差异、源码/二进制 hash、协议状态和日志。
- Weston 13 Headless/Pixman：Wayland socket、Canvas 312×324 绘制、事件循环返回、退出码 0 PASS；没有广告 `wl_seat`，每次记录到一次 `gdk_seat_get_keyboard` critical。缺失输入 seat 是相符解释但非已证因果；没有屏蔽 GTK 诊断。
- 文档尺寸硬约束已明示：Mem Reduct 3.5.2 主窗 224×214 DLU；五个设置页模板各 260×168 DLU。生产 GUI 必须维持原版官方紧凑尺寸，禁止放大；CI 336×348 仅为探针估值。真实 Windows 截图、DLU→物理像素实测、Hyprland 输入、HiDPI 和 2px overlay 均 BLOCKED / NOT RUN。
- 本机非 GUI 检查：`verify_project.py` 29 项、C 测试 3 项、4 个 shell 脚本语法、Zamak 0.2.0 check、5 项项目 tests/build、Arch helper build、`git diff --check` 均 PASS。未操作本机 GUI/Hyprland IPC，未执行 Polkit、drop_caches、Swap/ZRAM 操作。
- 下一步：更新规格分支记忆/文档，推送并创建独立 Issue #3 Draft PR；保留 PR #4 Draft，等待 ChatGPT 复审；不合并 PR #2/#4，不进入 P2B。
- 纠正文档/诊断后的 PR #4 head `6ad0f401d2b831ee71a1387a858e7456bbe444e5` 再次通过全部 CI：GUI push/PR `40192348460` / `40192351634`，build push/PR `40192348489` / `40192351591`。Artifacts 重新上传；probe SHA 未变。本轮没有生产 UI 或特权代码改动。

## 最近记录：P2A-R5 尺寸修正与隔离 GUI CI（2026-10-25）

- 工作分支 `test/issue-3-isolated-gui`，P1 基线 `11b0b9444cd94f0026b855abe3849a06106426f2`；PR #2 仍 OPEN / Draft、未合并。PR #4 仍 OPEN / Draft，base 为 `fix/issue-1-zamak-build`。
- 用户要求 GUI 按官方紧凑尺寸，不要放大。核对 Mem Reduct v.3.5.2 tag 源码 `2f5c55d15f69a2c88859e036b16c6f4c971e1968`：`IDD_MAIN` 为 224×214 Dialog Units，字体 `8 pt Ms Shell Dlg`。以 96 DPI 常见 6×13 px base-unit 估算，测试窗口请求 336×348；GTK Canvas 实测 312×324。该换算尚未在真实 Windows 上测量，也不是像素级基准。
- 仅收紧无特权 Canvas 探针窗口、修正 GTK Tab/Enter 字符事件名称并缩短占位文案；没有改生产 UI、P1 构建链路或特权功能。9 项已有路由测试函数体保持不变。
- 本机：`python3 scripts/verify_project.py` PASS（29 项）；C unittest PASS（3 项）；7 个 shell 脚本 `bash -n` PASS；Zamak 0.2.0 probe check/test/build PASS；9/9 probe 测试 PASS；源哈希验证和 `git diff --check` PASS。没有在本机启动 GUI、触碰活动桌面、安装依赖或执行内存/Swap/ZRAM操作。
- GitHub：测试源 SHA `6e0a4a79d56633603a298e5e0ef6520028c71b80` 的 push run [40187178574](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40187178574) 与 PR run [40187181722](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40187181722) 均 PASS；Wayland Headless/Pixman 与 Xvfb/XTEST 两个 job 均通过，截图、版本、日志和测试记录已作为 artifact 上传。
- 仍未完成：真实 Windows 3.5.2 截图与像素对照、Hyprland 实机输入验收、正式生产 UI 复刻。P2A 尚不可宣称正式验收；等待 ChatGPT 复审 PR #4，不进入 P2B、不合并任何 PR。

---

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
