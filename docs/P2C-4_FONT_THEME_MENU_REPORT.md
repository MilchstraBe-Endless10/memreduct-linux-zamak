# P2C-4 — 字体、控件绘制与深色主题校准

状态：P2C-4 字体候选、控件栅格、深色主题和安全菜单证据已完成；PR #7 仍 OPEN / Draft，不合并。最新完整截图与对照由 P2C run `40425954672` 在 evidence head `4f84fb9a9e247a5987a11cd435e5da4b05f78a53` 生成；生产 UI 源码 checkpoint 为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。当前分支工作树已同步该证据；整体像素 1:1 仍 NOT PASS，12 项简中来源差异仍待产品决策。

## 截图与比较 evidence snapshot：4f84fb9（2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 状态为 OPEN / Draft；base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`，evidence head `4f84fb9a9e247a5987a11cd435e5da4b05f78a53`。最新已审正式 review 仍为 P2C-3R1；几何/CI 阶段已接受，但整体 UI 1:1 明确 NOT PASS。
- [Windows v3.5.2 原生采集、Linux 生产 Xvfb/XTEST 和视觉比较 run 40425954672](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425954672)：3/3 jobs PASS。Windows artifact `12699069397`（`sha256:b9ce68670d7137d24923e64532d5a301d99e743fc423c6935165d5ad8181e1fa`），Linux X11 artifact `12700585092`（`sha256:e69b53bcf64021e8f0b2f0d70b10fe22ac24e9c00ed46efd32799b1c7264ab53`），比较/深色/字体 artifact `12700001976`（`sha256:209f4664b942017097bd17d5734d98b4983a58de3561ddab01739fc692e08bf7`）。APT diagnostics：Linux `12698999880`（`sha256:31729ff3376693ea048994e0b0288a727e114fd5b0b6a8ad0fc3eec26ccc36b5`），比较器 `12700196476`（`sha256:22fb723d7494a5561309ccb8e063fde310304b2af6b83a5d6676c4496fc7c304`）。
- [生产 Xvfb/XTEST 与 Weston Headless/Pixman 生命周期 run 40425954460](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425954460)：2/2 jobs PASS。X11 artifact `12699671782`（`sha256:f1b1dc344fba5de07f30bcde18689694b286dc8d9690c1ffcea8c718229ad305`），Weston artifact `12700250527`（`sha256:6233aae01f776ac2a03da09c7129fb9b9036bad01e51802ad511a2b593575595`）；APT diagnostics：X11 `12700215495`（`sha256:ddba8c63cab2ee3146365a88e8f5f0d11ad6ae947923dd6499f9a9c02276a578`），Wayland `12699741667`（`sha256:41a807461287e487b6b411fb96bd4767c72910ee1d295c3e0822fb507150ebac`）。此 workflow 的重复五页设置步骤按条件跳过；五页双语覆盖实际由上述 P2C Xvfb job 执行并通过。
- [PR build/governance/safety run 40425954317](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425954317) 与 [push build 40425944842](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40425944842) PASS；PR checks 7/7 PASS。Zamak 0.2.0 check/test/build、治理、Shell 及非特权 C/Python 安全作业均通过。
- 当前 artifact SHA 清单核验：Windows PNG 41/41、Linux PNG 44/44、comparison manifest 83/83、dark-theme manifest 70/70、P2C source 10/10、P2B X11 PNG 10/10、P2B source/binary 8/8。具体原图、对照图、源码/二进制散列均保存在链接 artifacts 中；该验证不宣称图片像素相同。
- 184/184 控件/资源几何 PASS，最大单分量偏差 1.5 px；导航 10/10、树线样本 102/102、离散控件栅格 36/36、中间滚动状态 2/2 PASS。10 张亮色页面的 changed-pixel 范围为 5.0741%–14.8853%；十组深色 English/简中配对为 5.9434%–73.2932%，均为 compare-only，整体像素验收 NOT PASS。
- 最新 132 行文本度量表含 120 个同源项目：9 项宽高均在 2 px 内，111 项仍有文字度量差异；其余 12 项为上游简中资源/实际显示差异，继续 `DECISION_REQUIRED`。Windows GDI 记录 `MS Shell Dlg 2`、8.25 pt、weight 400、ascent/descent 11/2 px；Linux Pango 为 Sans 8 pt，英文 DejaVu Sans、简中 Noto Sans CJK SC fallback，ascent/descent 10/3 px。隔离 Liberation Sans 66 个英文样本中 64 改善、1 退化、1 不变；54 个简中样本中 12 改善、12 退化、30 不变，因此没有采用到生产。
- Ubuntu 24.04.5 / runner image `ubuntu24 20261023.334.3` / APT 2.8.3 上，本轮 P2C Xvfb 101 MB 安装 13 秒、P2B Xvfb 101 MB 7 秒、Weston 79.6 MB 25 秒、Pillow 503 kB 小于 1 秒，update/install 均退出码 0。历史慢速下载的根因仍未确认；当前成功不证明长期网络稳定。
- P2C Windows 基准仍是官方 v.3.5.2 tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`；Windows runner 为 Server 2022 / 96 DPI / 100%。Linux Xvfb 为 Ubuntu 24.04 / GTK 3.24.41 / 96 DPI，`GTK_THEME=Adwaita:light`；深色模式由应用自己的 Canvas 调色板切换，不代表 GTK 原生暗色主题。XTEST 覆盖 Linux File/View/Settings/Help 菜单、语言子菜单和安全 clean dropdown；Windows tray 与无安全触发路径的清理菜单/确认/结果弹窗仍 BLOCKED。
- Weston 私有 headless socket 上 production 主窗及设置窗客户端均退出码 0；没有鼠标/键盘输入验收。保留 `gdk_seat_get_keyboard` critical、未公告 `wl_seat` 和 SIGTERM teardown layer 诊断，根因未证实。没有真实清理、Polkit、Swap/ZRAM、root GUI、活动 Hyprland IPC 或本机桌面输入。

## 前一截图对照 snapshot：748a61e（历史记录；2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 在此 evidence snapshot 时为 OPEN / Draft；base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；tested evidence head `748a61e62c597793e0acedb3f83bccb12f950285`。最新已审 review 仍是 P2C-3R1（接受其 CI gate；整体 UI 1:1 NOT PASS），无更新 review 或行内评论。
- [P2C Windows 原版 + Xvfb/XTEST + 比较 run 40422156194](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40422156194)：3/3 PASS。Artifacts：Windows `12699255059` (`sha256:70819f2d8a87574e2084b1f6ebe5fedfa8f3e816d01c2301869ab8b520d0a9b1`)、Linux X11 `12699350353` (`sha256:471fe629cca97d3c498fe2cda062b917e090fd1e63f2109b339c16e295087e28`)、视觉/暗色/字体 `12698126389` (`sha256:19bca18936bf9f9f0e3b28fcf09b71281e44208674d1a39570e1c2c370105724`)；APT 诊断 artifacts `12698306089` (`sha256:746b85ccfa0875b1e4b4172c907a469572d651dd94b2f496eae7c65b4754b911`) / `12698046531` (`sha256:46ffeaa745d8e9192225cecf8db485b178fbb993c134f982a1292fef64787eb9`)。
- [P2B 生产 Xvfb/XTEST + Weston Headless/Pixman run 40422156208](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40422156208)：2/2 jobs PASS。Artifacts：X11 `12697309676` (`sha256:b926d635c0ff9819a5f69f88a8520cc13e11dda7d83595d9e0c77761b9d8ecb4`)、Weston `12699213346` (`sha256:8af9038aae378f82228553661eeb49135e29eff62ca17c7442dc7424c48cd2fa4`)；APT 诊断 artifacts `12699225113` (`sha256:eeeee912d3ee077e45bd8db5d42339e7b612dd42a461b2c48d3b81c259daeee8`) / `12698294052` (`sha256:4d388912769447b83c1094c53595b8bb66e79f4d4c6961bac107edc04bb96ca4`)。
- [PR build/governance/safety run 40422156138](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40422156138) 与 push build [40422151693](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40422151693) PASS；PR checks 7/7 PASS。Zamak 0.2.0 check/test/build、治理、Shell 与 C/Python 安全步骤均 PASS。P2B 的重复设置页 step 按条件 SKIPPED，由相同 head 的 P2C Xvfb 作业完整覆盖；没有失败或取消的必需 job。
- 运行环境是 GitHub-hosted Ubuntu 24.04.5，image `ubuntu24 20261023.334.3`，APT 2.8.3。P2B Weston job 的 APT update 在 5 秒内成功（索引 12.6 MB，报告 1 秒下载）；依赖安装在 21 分 45 秒成功，APT 日志记录从 `azure.archive.ubuntu.com` 获取 79.6 MB 用时 21 分 32 秒、平均 61.6 kB/s，退出码 0 / 分类 PASS。日志未显示 DNS、包锁、缺包或仓库错误；已确认的是下载慢，具体根因未确认。P2C Xvfb 安装 18 秒，P2B Xvfb 安装 2 分 16 秒，比较 job 的 Pillow 安装 4 秒，均成功。不能据一次成功称 APT 网络稳定问题已根治。
- P2B Wayland artifact 证明私有 Wayland socket 建立、production 主窗与设置窗连通/绘制，两个客户端退出码均为 0。此 lane 不含鼠标/键盘验收。保留诊断：`gdk_seat_get_keyboard` critical 1 次、Weston 未公告 `wl_seat`；Weston 收到 SIGTERM 后日志出现 `BUG: finalizing a layer with views still on it.`。CI job 成功，但 compositor teardown 返回状态没有单独记录；现有日志不能证明此诊断由应用造成，也不能当作干净的 compositor shutdown 证据。
- 精确 artifact 清单：Windows PNG 41/41、Linux PNG 44/44、比较素材 83/83、暗色素材 70/70、P2C 源文件 SHA 10/10、P2B X11 PNG 10/10 与源码/二进制 SHA 8/8 均匹配。P2B 生产和 helper 二进制 SHA-256 分别为 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`、`30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`；P2C 关键源码 SHA-256 在 P2C X11 artifact 清单中对应 `src/main.zm`、`src/settings_ui.zm`、`src/ui_geometry.zm` 与测试/比较脚本。
- 字体结论：Windows GDI 逻辑 face `MS Shell Dlg 2` / 8.25 pt / weight 400，ascent/descent 11/2 px；Linux Pango Sans 8 pt 解析至 DejaVu Sans / Noto Sans CJK SC，ascent/descent 10/3 px。120 项同源文本度量中 25 项差在 2 px 内、95 项仍有差异。隔离 Liberation Sans 测试改善多数 English 宽度，但简中样本改善与退化并存、全页收益小；生产字体和字号不改，未打包 Windows 专有字体。深色 10 组双语页面均有并排/overlay/difference/heatmap；全页变化率 5.9434%–73.2932%，仍为 compare-only。亮色为 5.0741%–14.8853%。几何 184/184（最大分量偏差 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36、中间滚动 2/2 PASS；这些局部结果不代表整页像素相同。
- Windows v3.5.2 参考未缩放；Linux 深色截图在 GTK `Adwaita:light` runner 中通过应用自己的 Canvas 暗色调色板采集。四个主菜单与安全清理 dropdown 有 XTEST 证据；Windows tray、不能无副作用触发的清理弹窗/确认/结果继续 BLOCKED。12 项简中资源来源差异保持 `DECISION_REQUIRED`，没有改文案。未运行真实清理、Polkit、Swap/ZRAM、root GUI、活动 Hyprland IPC 或主机桌面输入。
- 最终全页 UI 1:1 / 像素验收：NOT PASS；Windows 与 GTK/Pango 的平台绘制差异、文字度量、简中来源差异和禁用状态差异仍需分开处理。

## 前一完整截图/对照 artifact snapshot：105d71d（2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) OPEN / Draft，Base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；HEAD `105d71d567d7ba096e8b928c072ceb4af15bfe6f`。生产源码 checkpoint 仍为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；HEAD 中的本轮改动仅同步报告、交互矩阵和记忆。
- P2C [Windows native + production Xvfb/XTEST + visual comparison run 40416882722](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416882722)：3/3 PASS。Windows artifact `12695303623` (`sha256:db33cc90dcd9aa0b8b9678adfe44fed0d6ff6733ee4ae6087603b94bc662e7e7`)，Linux X11 artifact `12694879677` (`sha256:424d9d863a3c6a7d2c2f66224150f58e34aec9b62a4f6196131f0c5b4048a782`)，视觉/暗色/字体 artifact `12694609240` (`sha256:5b3e9348af249a02ceb7871d76d4d5cb7f7d90e5647dfc7456e5d2726e3caadc`)；APT diagnostics `12695029426` (`sha256:081e99dc7ceb1d7bdfefef89297c128cf1d9fc1d9f3bd147a6dd0c95135f46fa`) and `12694434533` (`sha256:e4d5a8344547de4527c36124be05f35d7065072162551143acb088af19c78168`)。本次 P2C Linux APT install 获取 101 MB 用时 14 秒，comparison job 的 Pillow install 依赖已可用且两个 APT 诊断均 PASS。
- P2B [production Xvfb/XTEST + Weston Headless/Pixman lifecycle run 40416882708](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416882708)：2/2 PASS。X11 artifact `12696373914` (`sha256:bf70d1f1b7a5fba60ed9ef175071009c12c411f887399e38ab5841ac2c04a9d3`)，Weston artifact `12695762656` (`sha256:dbe4352648a14e4660b2edf518ede8bc1406aec720a4b6a2dd92aeda72d23adb`)；APT diagnostics `12697343427` (`sha256:eaa89f01b169727fae75896a2a061c9e9f199ad6c84dca496d7e9404a6e78859`) and `12695913269` (`sha256:a8d4e35a793fb144c2f828a1c56d2fe89bd3ba323ecd8aa9fc665d80c209cdea`). P2B 的重复五页设置步骤按条件跳过；同一 HEAD 的 P2C job 完整执行并通过五页双语覆盖。
- PR build/governance/safety [run 40416882739](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416882739) 与 push build [run 40416873128](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40416873128) PASS；PR checks 7/7 PASS。P2C artifact 的 PNG 清单核对为 Windows 41/41、Linux 44/44、视觉 83/83、暗色 70/70，源码 10/10；P2B 的截图哈希全部匹配，六个源码 SHA 与 HEAD 匹配，两个二进制 SHA 匹配 artifact。
- 视觉/几何结果未回退：资源几何 184/184（最大分量偏差 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36、中段滚动 2/2 PASS。亮色 changed-pixel `5.0741%–14.8853%`，暗色 `5.9434%–73.2932%`；所有全页结果仍为比较证据，1:1 `NOT PASS`。
- P2B Ubuntu 24.04.5 / GitHub-hosted image `ubuntu24 20261023.334.3` 的 Xvfb APT update 成功（获取 12.6 MB，1 秒）；依赖安装也成功，但耗时 36 分 23 秒。原始 APT 日志显示获取 101 MB 用时 36 分 11 秒，来自 `azure.archive.ubuntu.com`，其中 `fonts-noto-cjk` 为 61.2 MB；安装退出码 `0`，诊断分类 PASS。日志未显示 DNS、锁、缺包或仓库错误。慢速现象可确认，具体根因仍未确认；不能据此声称网络或镜像故障已解决/已定位。
- P2B Xvfb artifact 的 10/10 截图 SHA 均通过；生产进程 WM-close 退出码 `0`，结果记录了真实 `/proc` 采样、语言菜单/XTEST 与清理操作禁用。Wayland artifact 仅通过独立 Weston 客户端生命周期；无 Wayland 输入或 Hyprland 验收。字体生产默认未改、Windows 专有字体未打包；12 项中文来源差异仍 `DECISION_REQUIRED`。无真实清理、Polkit、Swap/ZRAM、root GUI 或活动桌面输入。

## 前一 exact-head CI：516e824（历史验证）

### 516e824 artifact 与检查结果

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) OPEN / Draft；base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；head `516e824cf4bbb840650bccdbbfdedb2d352d9090`，与远端分支一致。提交仅更新报告和记忆；生产源码 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`、工作流、基准图与阈值未变。
- [P2C Windows native + Linux production Xvfb/XTEST + visual comparison run 40415153443](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40415153443)：3/3 jobs PASS，Windows 原版来源/截图、Linux 五页生产 XTEST、视觉比较均执行。Windows artifact `12695431166` (`sha256:dd5841ac4ca46a51abf0941bff97710c848791924b16f0b18a6ce2ef4f9cb56b`)，Linux artifact `12695636229` (`sha256:07bf246bb3a21ed110acf0ec90f75215fab30eed7aac9c6d8bea296d633e13cf`)，视觉/暗色/字体 artifact `12696136119` (`sha256:67505e04aa2626fa96eabee6d3cca3b5c820de23cc50859a5dc3953fd97b6ba0`)；APT diagnostics `12695656194` (`sha256:71d2badc563342768486d7f4f3af7fd0b8b71eb5fcc9781eefc0d979669dfa93`) 与 `12695991376` (`sha256:b1a5253dfa38320dba78619a3c326731030069b564019a00233911bc630c2a67`)。
- [P2B production Xvfb/XTEST + Weston Headless/Pixman lifecycle run 40415153555](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40415153555)：Xvfb 与 Weston jobs 2/2 PASS。X11 artifact `12695985667` (`sha256:75e628506c4c1766e332fcad4d8b345c046c5d287f369e5fca2428fa581beeee`)，Weston artifact `12695376138` (`sha256:756a830bc7bcf73b9835f1eef6fe77c477b1ead89376bee07941322e395f443f`)；APT diagnostics `12695845445`、`12695745513`。该 workflow 的重复五页设置测试按条件跳过；同 head P2C job 已完整执行并通过五页覆盖。Weston 仅验收隔离客户端生命周期，不代表 Wayland 输入或 Hyprland 验收。
- [PR build/governance/safety run 40415153495](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40415153495) 与 [push build run 40415146990](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40415146990) 均 PASS；当前 PR checks 7/7 PASS。Zamak 0.2.0 版本/check/test/native build、治理、Shell、C/Python safety 步骤均成功。精确 Actions 列出的 P2C 和 P2B 必要 jobs 均成功；未见失败或取消 job。
- 当前 artifacts 的 PNG/source 清单核验：Windows PNG 41/41、Linux PNG 44/44、visual 83/83、dark 70/70、P2C source 10/10、P2B source/binary 8/8 均匹配。几何 184/184 PASS（最大分量偏差 1.5 px）、导航 10/10、树线 102/102、控件栅格 36/36、中段滚动 2/2 PASS。亮色 changed-pixel 为 5.0741%–14.8853%，暗色为 5.9434%–73.2932%；差异图/overlay/heatmap 已生成，均为比较数据而非全页像素通过，1:1 仍 NOT PASS。
- 当前 head 的 P2C/P2B 有界 APT 更新和安装步骤成功。历史 `40408926842` 的一次 P2B 下载超时及其失败重跑通过仍保留作诊断记录；日志仅能确认 `fonts-noto-cjk` 下载阶段超时（exit 124），底层网络/镜像原因未确认。当前成功不证明网络问题永久消失。

## 前一 exact-head 验证：33367f3（2026-10-27，历史 snapshot）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 仍 OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，head `33367f3c15273f3e817bba27e9272a19663ea829`。生产 UI 源码保持 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；后续改动在工作流/Windows 采集脚本与报告/记忆，未改生产字体、字号、UI 源码或视觉阈值。
- [P2C Windows native + Linux production Xvfb/XTEST + visual comparison run 40408926862](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408926862)：3/3 jobs PASS，Windows 来源与截图、Linux 五页生产 XTEST、比较器均运行。Artifacts：Windows `12691828080` (`sha256:ca0e98ec99ce5332956c037271fb91fbcb7cb4ac11fa086e050eedabaf6b4542`)，Linux `12692965976` (`sha256:9171f0ca79938f360cc36b4937744f65c1f4b42b874ed7ec7244be27c4c88f3f`)，visual/dark/font `12692891146` (`sha256:115c23d9d3d1d90cce265cab20a68176d5fbd104bd76e59728cb59a0d1b186bd`)；APT diagnostics `12693270736`、`12693050979`。
- [P2B production Xvfb/XTEST + Weston Headless/Pixman run 40408926842](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408926842)：两个 job 均 PASS。X11 artifact `12694248993` (`sha256:5f14c003ca627693f928e1d54384afeafa683cb66ceba14f7ab6728327cdf7ac`)，Weston artifact `12690409508` (`sha256:58cf6b8da7c450ad0374892282d444e92869a1f042f0810d6ae1efcc0ba305d9`)，APT attempt 1/2 diagnostics `12693989271` (`sha256:0ee1ca0d97bf6df0f3205fa4f975c52fc6de15d1e872b4a5ab85034d091d0ed2`) / `12693778915` (`sha256:c1f239e16c467881cb8e70ed312c4f2d3c9cbf7589d5db72c2732539227f17f8`)。
- P2B 的首次 Xvfb 安装中，`apt-get update` 成功；bulk install 在 `fonts-noto-cjk` 61.2 MB 下载期间达到 38 分钟硬时限，退出码 `124`，诊断分类 `command timeout`。日志有慢速/`Ign` 下载迹象；未观察到 DNS 失败、包锁或包不存在，底层网络/镜像原因未确认。只重跑失败 job 后，依赖安装以退出码 `0` 完成。此重试证明本次恢复，不证明网络问题已根治。P2B 重复的五页设置 step 条件 `SKIPPED`；同 head 的 P2C Xvfb job 完整运行五页双语覆盖。
- 同 head 的 PR build [40408926778](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408926778)、push build [40408920155](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40408920155) 均 PASS；PR checks 7/7 PASS。P2C Windows PNG 41/41、Linux PNG 44/44、visual 清单 83/83、dark 清单 70/70、P2C source 清单 10/10、P2B source/binary 清单 8/8 均核对匹配；P2B 主程序 SHA-256 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper SHA-256 `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。
- 几何与局部图形回归保持：Win32 resource-ID 控件 184/184 PASS（最大分量偏差 1.5 px）、导航 10/10、树状连接线像素 102/102、控件栅格 36/36、中段滚动 2/2 PASS。该 run 的亮色五页 English/简中 changed-pixel 为 5.0741%–14.8853%；深色十组为 5.9434%–73.2932%。差异图/overlay/heatmap 已生成；以上是比较结果，不是整页像素验收，1:1 仍 NOT PASS。
- 环境：Windows Server 2022 build 20348，交互桌面 1024×768、96 DPI/100%；Linux GitHub-hosted Ubuntu 24.04.5，Xvfb 1024×768×24、GTK 3.24.41、Adwaita:light、96 DPI、scale 1，应用暗色由生产 Canvas 调色板切换。测试未访问用户活动桌面。Weston 只验收 Headless/Pixman 生命周期，不代表 Wayland 输入通过。
- 字体决策不变：Liberation Sans 仅隔离进程试验，英文多数宽度差收敛，但简中有改善也有退化，未用于生产、未提交 Windows 专有字体。12 项来源语言差异仍 `DECISION_REQUIRED`；Windows tray、无法安全触发的 Windows clean popup/confirmation/result 仍 `BLOCKED`。禁用清理、无 Polkit/Swap/ZRAM/内核写入。

## 前一 exact-head 证据：f4c107b（历史快照，2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，head `f4c107b5ef6bbc34a4a78fd3600dbfe4c4e81161`。相对生产源码 checkpoint `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`，产品 UI 实现未变；该 head 只同步报告、交互矩阵和记忆。没有合并、强推或活动桌面操作。
- [P2C Windows 原版 + Linux Xvfb/XTEST + 视觉比较 run 40406877107](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877107)：3/3 jobs PASS，Windows 官方版本/原生采集、Linux 生产设置窗口 XTEST、字体候选及比较器均执行完成。Windows artifact [12691453162](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877107/artifacts/12691453162)，digest `sha256:e0186164c67325c2607585a554282c3bf6b8c4a56ebccba76d2260bcc774ebbf`；Linux X11 artifact [12691688156](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877107/artifacts/12691688156)，digest `sha256:0f95f45130bf191764706b6732063adf21032034b807c7f9639a0b2ee6780c7d`；视觉/深色/字体比较 artifact [12690699316](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877107/artifacts/12690699316)，digest `sha256:533900b7f323afd2e1738581946a0b799c35f311835333142ea6ea26b98b2361`。APT diagnostic artifacts：Linux `12690913807`，比较器 `12691488689`。
- [P2B production Xvfb/XTEST + Weston Headless/Pixman run 40406877396](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877396)：2/2 jobs PASS。X11 artifact [12690432596](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877396/artifacts/12690432596)，digest `sha256:8a1dcea9b4357c37ab2f2804bd1d722b9c7db49610c7ad5b43edd78673d773e6`；Wayland artifact [12691687733](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877396/artifacts/12691687733)，digest `sha256:5022645c92f6e669f0d6852e3a945a5bedbeb391adf8e46257d0323da49d834d`。P2B 重复的设置页 step 按条件跳过；同 head 的 P2C Xvfb job 已完整覆盖五页。PR build [40406877378](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406877378) 与 push build [40406867272](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40406867272) PASS；PR checks 7/7。
- 下载后按文件内容核对：Windows PNG 41/41、Linux PNG 44/44、visual 清单 83/83、dark 清单 70/70、源码文件 10/10、源码/二进制 8/8 均匹配。该 Linux 构建主程序 SHA-256 为 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper 为 `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。资源 ID 几何 184/184（最大分量偏差 1.5 px）、导航 10/10、树线像素 102/102、栅格 36/36、中间滚动状态 2/2 PASS。十组暗色 English/简中截图均有配对原图、并排图、50% overlay、difference 和 heatmap；changed-pixel 为 5.9434%–73.2932%，仍是 `COMPARE_ONLY; NOT PIXEL PASS`。亮色五页双语 changed-pixel 为 5.0741%–14.8853%。
- Windows Server 2022 build 20348、96 DPI / 100%，官方 `v.3.5.2` tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`；Windows runner 采集主窗口、五页双语及暗色状态。release archive SHA-256 `ec41ca15ff623f53850d9ee01986d3f8ff457ec93457a6ceeb993b7164023d15`、setup SHA-256 `025e5c06f7beccf5c85cbe931bc058f53a4642ca83a00855a475026a5e6334c4`，detached PGP signer `D98523611524AB29BE7330AC288120A75635B5FD` 验证成功；portable exe SHA-256 `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`，未 Authenticode 签名。Linux 为 GitHub-hosted Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、96 DPI、`GTK_THEME=Adwaita:light`；暗色由应用 View 菜单切换 Canvas 调色板，不更改 runner/桌面主题。
- 原生 GDI 逻辑字体记录 `MS Shell Dlg 2` 及 `Tahoma` registry substitution；没有证据能证明实际物理字体文件。隔离 Liberation Sans 候选对英文 64/66 个同源度量样本改善；简中 12 项改善、12 项退化，且不适合以单语收益牺牲 CJK，因此生产字体保持默认 Sans/DejaVu Sans + Noto Sans CJK SC。没有打包 Windows 专有字体或整体缩放 UI。
- XTEST 实际打开 File/View/Settings/Help、双语 Language 子菜单和安全 clean dropdown，验证菜单键盘导航、语言切换及关闭。清理功能保持禁用。Windows tray 和无法在不执行真实清理时安全触发的 clean popup / confirmation / result 仍 BLOCKED。Weston 私有 socket 上主窗/设置窗连通、绘制并正常退出 0；`gdk_seat_get_keyboard` critical 1 次且 `wl_seat` 未公布，因果未证实，Wayland 鼠标/键盘输入 NOT RUN。

## 前一 exact-head 验证：6892110（历史快照，2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，head `689211049a2028a8d740763d958d9b001c00c255`。这是报告、矩阵和记忆同步提交；生产 UI、验收阈值及 Windows goldens 与前一 source checkpoint 相同。Review 中没有比 ChatGPT P2C-3R1（针对 `2c745710210c8794b52a0e9eb19dd1830c00d8d6`）更新的评论。
- [P2C Windows 原版 + Linux Xvfb/XTEST + 视觉比较 run 40405594197](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40405594197)：3/3 jobs PASS，Windows 来源验证、生产 Xvfb/XTEST、几何/暗色/字体比较及 artifact 上传都成功。Windows artifact [12689674313](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40405594197/artifacts/12689674313)，digest `sha256:b757c0792ba9bc4bfa5ec4ce705c9d764e5aa4c9730bc5044d810dcdda4855fe`；Linux X11 artifact [12691666963](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40405594197/artifacts/12691666963)，digest `sha256:206da7efe42cd4738abba584b8e05d6dfb9d00ce6605e1de86f8fa2e546cc452`；visual/dark comparison artifact [12690223544](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40405594197/artifacts/12690223544)，digest `sha256:50007b55a99511dbd677954260697a3e172c5d391ae9dbc0f0c090561dba3766`。APT diagnostic artifacts：Linux `12691676864`，comparison `12689884428`。
- [P2B production Xvfb/XTEST + Weston Headless/Pixman run 40405594177](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40405594177)：两个 job PASS。X11 artifact `12689774323`，digest `sha256:f0bd55f9f4b6cb062a25f86a08a43674210371cbeed86ce3fa3001efd4a65901`；Wayland artifact `12691681387`，digest `sha256:87e036fb034f7857b2e10b1b8227afd1eb20c5b0ab6ae14b68c4c65f6b3ba4da`。P2B 重复的五页设置用例按条件跳过；P2C 同 head 已完整执行该用例。X11/Wayland APT diagnostic artifacts 分别为 `12689914182`、`12691356842`。
- [PR build/governance/safety run 40405594170](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40405594170) 与 [push build 40405588384](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40405588384) 均 PASS；7 个 PR checks 全通过，包含治理、Shell syntax、非特权 C safety、Zamak 0.2.0 check/test/native build。
- 最新 run artifact 哈希复核：Windows PNG 41/41、Linux screenshot PNG 44/44、visual 83/83、dark-theme 70/70、P2C source 10/10、source+binary 清单 8/8 全匹配。控件几何 184/184 PASS（最大分量偏差 1.5 px），nav 10/10、tree guide 102/102、raster 36/36、中间滚动 2/2 PASS。精确 changed-pixel 比率与紧接本段后的 d238649 历史 run 表格完全一致；当前 artifact 内含本 head 独立生成的 PNG、叠图、差异图、热图及逐图 SHA。
- 该 exact-head P2C runner 的依赖安装成功：APT 获取 101 MB 用时 7 秒；Pillow comparator install 成功。前一 head 的同一依赖步骤曾出现长时间低速下载但最终成功；此前超时的底层网络/镜像根因仍未确认，不把当前成功说成问题已根治。
- P2B Weston 13.0.0 Headless/Pixman 使用独立 socket；生产客户端连接、绘制、读取采样后以退出码 0 退出。日志中的 `gdk_seat_get_keyboard` critical 和 SIGTERM 关闭期 layer-finalization 诊断均保留，根因未知；不宣称 Wayland 鼠标/键盘验收。P2B Xvfb/XTEST 通过生产主窗口操作，清理动作保持禁用；本机没有 GUI、Hyprland IPC 或用户桌面输入。
- Windows screenshot 来源仍为 v3.5.2 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。Windows tray、没有安全操作路径的原生 clean popup、需执行真实清理才会出现的确认/结果窗仍 BLOCKED；12 项简中来源差异仍 DECISION_REQUIRED。整体像素 1:1 仍 NOT PASS。

## 前一 exact-head 验证：d238649（历史快照，2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) OPEN / Draft，base `911a3d0b99b05f15bd0109dca475b0b5174e7888`，head `d23864905f7e841eb428135bb1da244abca5d1df`。Review 列表没有比 ChatGPT P2C-3R1（针对 `2c745710210c8794b52a0e9eb19dd1830c00d8d6`）更新的评论；该 review 接受 CI gate，但明确整体 1:1 NOT PASS。
- [P2C Windows 原版 + Linux Xvfb/XTEST + 比较 run 40393863774](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863774)：3/3 jobs PASS，必要比较步骤均运行。Windows artifact [12687194357](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863774/artifacts/12687194357)，digest `sha256:39d347c00ab6cac66a155fcdd3c0c3b76ece305d49a3893872efc3e4239c82e0`；Linux production X11 artifact [12690875520](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863774/artifacts/12690875520)，digest `sha256:880a6b11f3cf1555c082d511568ee6d486271b94ffbfdad79db910fc1622612d`；comparison artifact [12689194953](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863774/artifacts/12689194953)，digest `sha256:fa9abb4a9b835a231dfc288c73de6d1d3654e8bd636480977d532b4b93e89312`。APT diagnostic artifacts：Linux `12690375257`，comparison `12690605189`。
- [P2B production Xvfb/XTEST + Weston Headless/Pixman run 40393863850](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863850)：2/2 jobs PASS。X11 artifact `12688395963`，digest `sha256:4169e92f40281f9d3d02b1a5c62a51e3fb6d3f44a6c2516b630e053626aff985`；Wayland artifact `12686999481`，digest `sha256:c22749335194dcad9379ce9bd339fa5a90678fbb36cc48564838c738872fd085`。P2B 中重复的五页设置场景按 workflow 条件跳过；同 head 的 P2C Xvfb job 完整执行了五页双语测试。
- [PR build/governance/safety run 40393863878](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393863878) 与 [push build 40393857503](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40393857503) 均 PASS；PR checks 为 7 PASS / 0 FAIL。build 作业包含治理文件、Shell syntax、C safety、Zamak 版本/check/test/native build 全部成功。
- Windows runner 验证固定 v3.5.2 tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`、官方 release archive SHA-256 `ec41ca15ff623f53850d9ee01986d3f8ff457ec93457a6ceeb993b7164023d15`、setup SHA-256 `025e5c06f7beccf5c85cbe931bc058f53a4642ca83a00855a475026a5e6334c4` 和 detached PGP signer `D98523611524AB29BE7330AC288120A75635B5FD`；portable 3.5.2 executable SHA-256 `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`。portable executable 未 Authenticode 签名；不把 setup PGP 签名说成 executable 签名。Windows PNG SHA 41/41、Linux production PNG 44/44、visual PNG 83/83、dark-theme PNG 70/70、生产源码 10/10、源码/二进制清单 8/8 均与 artifact/工作树相符。
- 测试环境：Windows Server 2022 build 20348、1024×768、96 DPI / 100% scale；Linux Ubuntu 24.04.5 runner、Xvfb 1024×768×24、GTK 3.24.41、96 DPI、scale 1、GTK_THEME=Adwaita:light。Linux dark 是应用 Canvas palette；不修改 GTK/桌面主题。设置客户区与 Windows crop 都是 558×335，没有拉伸、缩放或移位。

### exact-head 视觉数据

下表为当前 run 的原始 changed-pixel 比例，类别是 compare-only，不构成整页像素通过。

| 页面 | Light English | Light 简中 | Dark English | Dark 简中 |
|---|---:|---:|---:|---:|
| General | 12.0184% | 10.6617% | 10.2327% | 51.6086% |
| Memory cleaning | 14.8853% | 14.7392% | 12.6625% | 73.2932% |
| Appearance | 11.5123% | 10.8966% | 11.6311% | 54.3027% |
| Tray icon | 14.7900% | 13.9287% | 11.2534% | 54.6584% |
| Advanced | 5.5871% | 5.0741% | 5.9434% | 46.0889% |

十组暗色 EN/简中对照均有 Windows/Linux 原图、并排、50% overlay、difference、heatmap 与逐图 SHA；dark-theme manifest 70/70 匹配，结论均为 `CAPTURE_AND_COMPARE_PASS; PIXEL_IDENTITY_NOT_PASS`。暗色中文变化率包含原版仍显示英文的上游文案差异，不能归因于字体。

冻结的几何/控件验收无回退：资源控件矩形 184/184 PASS（最大分量偏差 1.5 px），导航选中框 10/10、树线像素 102/102、离散控件 raster 36/36、中段滚动 2/2 PASS。全页像素 1:1 继续 NOT PASS。

### 字体、交互与运行限制

- Windows GDI：逻辑 face alias MS Shell Dlg 2、8.25 pt、weight 400、ascent/descent 11/2 px；FontSubstitutes 指向 Tahoma，但物理字体文件未证实。Linux Pango Sans 8 pt、weight 400，英文匹配 DejaVu Sans、简中匹配 Noto Sans CJK SC、ascent/descent 10/3 px。两者 ascent+descent 均 13 px，但垂直分配不同，且 GTK text origin 不是 GDI baseline 等价值。
- 120 项同源度量中 25 项宽高差在 2 px 内、95 项仍有差异。隔离 Liberation Sans 候选：English 66 项中 64 改善、1 退化、1 不变；简中 54 项中 12 改善、12 退化、30 不变。英文全页收益仅 0.0578–0.4093 个百分点，简中几乎不变或略退；生产字体、字号、几何均未改变，也未提交字体文件。Canvas Painter.text 没有 per-call 字体 family 参数，证据不支持增加字体桥接。
- Linux XTEST 实际操作 File/View/Settings/Help 双语菜单、View 语言子菜单、方向键/Enter/Escape、菜单分隔槽与安全 clean dropdown；禁用清理动作无法执行。窗口 Close、Alt+F4、WM_DELETE_WINDOW、主窗保留与释放均通过。Windows runner 采集四个英文主菜单；tray 未被 UI Automation 暴露，clean split menu 无安全 ExpandCollapse 接口，确认/结果框需要真的清理才能打开，全部保持 BLOCKED。
- Weston 13.0.0 Headless/Pixman 客户端连入私有 socket，窗口绘制并退出码 0；不声称有 Wayland 鼠标/键盘输入。日志保留 `gdk_seat_get_keyboard critical count=1`（日志称 wl_seat 未广播）以及 SIGTERM 后 `BUG: finalizing a layer with views still on it.`；没有根因证据，未归因给应用或隐藏诊断。
- 当前 P2C apt update/install 均退出 0；Ubuntu Azure archive 下载 69.7 MB 用时 31m33s（36.8 kB/s），期间出现 Azure 源获取与 archive.ubuntu.com 重试路径。比较 job 的 Pillow install 通过。该次无超时，但旧运行中断仍没有可证实的 DNS、包锁或仓库根因；不能把本次成功解释为网络问题已根治。
- 没有执行真实清理、Polkit、drop_caches、Swap/ZRAM、root GUI 或用户桌面输入；所有未验收清理操作继续禁用。12 项简中来源差异维持 DECISION_REQUIRED。

## 上一 exact-head 验证：37cefdb（历史快照，2026-10-27）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 状态 OPEN / Draft；base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`；head `37cefdb351b1cfcdacbc6ca24eb8b06fead93cc1`。生产源码 checkpoint 为 `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`；之后仅调整 P2B/P2C workflow 并发组并同步报告/记忆，未修改 UI、goldens、依赖或验收阈值。检查时没有比 P2C-3R1（head `2c745710210c8794b52a0e9eb19dd1830c00d8d6`）更新的 ChatGPT review；该 review 接受 R1 CI 门槛，但仍明确整体 1:1 UI 为 NOT PASS。
- P2C [Windows 原版 + Linux 生产 Xvfb/XTEST + comparator run `40390082314`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314)：3/3 jobs SUCCESS，Windows 来源/原生截图、Linux 五页中英文及菜单截图、视觉比较步骤均执行。P2B [生产 Xvfb/XTEST + Weston lifecycle run `40390082510`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082510)：2/2 jobs SUCCESS；P2B 中重复的五页设置场景按条件 SKIPPED，同 head 的五页完整验证由 P2C Xvfb job 实际覆盖。PR build/governance/safety [`40390082353`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082353) 与 push build [`40390075275`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390075275) 均 SUCCESS。
- Artifact digest：Windows [`12682119919`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314/artifacts/12682119919) `sha256:9a0463631da7bf4e049f5fe319afc9496f16b0fd2b940fae85363f6c2f845acc`；Linux production X11 [`12684755661`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314/artifacts/12684755661) `sha256:9bfb89a4bb10da11da9b42bc865627e36bc50c1e654ee3212ee14c7086ba0af2`；Windows/Linux visual comparison [`12683006399`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082314/artifacts/12683006399) `sha256:8b765ec896abad794b91cbe80194c3959dc98a9c4fabb83cf2ffccdc276437b2`；P2C APT diagnostics `12684970524` / `12683476105`。P2B artifacts：X11 [`12682830560`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082510/artifacts/12682830560) `sha256:d5e4967f92c7feb51defd5623b08b0135abed00ace0a2d17d5a995b22e901fc4`；Wayland [`12682645763`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40390082510/artifacts/12682645763) `sha256:1c68e2e22f6945514b14efb065da7001b3633d3266795895198c2846b5d86524`；其 APT diagnostics 为 `12682755594` / `12683000436`。
- 对实际下载的 exact-head artifacts 校验：visual `visual-sha256.csv` **83/83**、dark-theme SHA 清单 **70/70**、Linux production screenshot 清单 **44/44** 全部与 PNG 实际字节匹配；Linux source 清单 **10/10** 与源码工作树匹配，主程序/helper 二进制 **2/2** 匹配。主程序 SHA-256 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。完整截图 SHA 与截图本体保存在上述 artifact，不混用其他 run 的文件。
- 最新轻色五页双语 changed-pixel 百分比（Windows 原生客户区与 Linux production Canvas 均 558×335，未缩放）：

| 设置页 | English | 简体中文 |
|---|---:|---:|
| General | 12.0184% | 10.6617% |
| Memory cleaning | 14.8853% | 14.7392% |
| Appearance | 11.5123% | 10.8966% |
| Tray icon | 14.7900% | 13.9287% |
| Advanced | 5.5871% | 5.0741% |

  五页 × 双语的暗色配对截图、并排图、overlay、difference、heatmap 和逐图哈希也都在 visual artifact 的 `dark-theme/` 目录；暗色全页差异率沿用报告下一节列出的数值。几何 **184/184** PASS（最大单项偏差 `1.5 px`）、导航 `10/10`、树线采样 `102/102`、离散控件栅格 `36/36`、中间滚动状态 `2/2` PASS；这些局部测试不等价于像素相等。
- Weston 在独立 Headless/Pixman 下连接、绘制；测试客户端退出码 `0`，主窗与设置窗均释放。输入设备验收 NOT RUN。关闭客户端后由脚本对 Weston 发送 SIGTERM，`weston.log` 出现 `caught signal 15` 和 `BUG: finalizing a layer with views still on it.`；尚无证据确定根因，因此本报告将其作为 compositor 关闭期诊断保留，不能解释为 clean compositor shutdown，也没有据此归咎应用。
- Linux 环境：GitHub-hosted Ubuntu 24.04、Xvfb `1024×768×24`、GTK `3.24.41`、96 DPI/scale 1、`Adwaita:light`；Linux 暗色由应用 Canvas palette 切换。Windows：Server 2022 build 20348、1024×768、96 DPI/100% scale。原版仍固定 v.3.5.2 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 整体 Windows/GTK 像素同一性仍 **NOT PASS**；Windows 专有字体没有打包；12 项上游简中显示差异仍 `DECISION_REQUIRED`。Windows tray、原生清理弹出菜单及真实清理确认/结果弹窗仍 BLOCKED；未执行清理、Polkit、Swap/ZRAM、root GUI 或活动 Hyprland 操作。

## 前一 CI checkpoint `dc1e112`（2026-10-27，历史记录）

- PR [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) 仍 OPEN / Draft，Base `feat/issue-3-p2b-main-window` / `911a3d0b99b05f15bd0109dca475b0b5174e7888`。以下 exact-head CI 对应分支提交 `dc1e112ba27bb1b89bcdd4b3c55ddf1465695e10`；该提交只更新两个 GUI workflow 的 concurrency group，生产源码、Windows 基准、比较阈值及测试数量均未改变。后续报告/记忆同步不改产品代码。
- P2C run [40382203136](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40382203136) 在此 head 上 **3/3 jobs PASS**：Windows 3.5.2 来源与截图、Linux 生产设置 Xvfb/XTEST、几何/视觉/暗色比较均执行成功。Artifacts：Windows `12679133143` (`sha256:1417e252b1d837c509670b73524ea9b9058eec0055104cfe5da478beb242e66a`)、Linux Xvfb `12677879049` (`sha256:26fbc176837d7bce5fde49256f8d0f6744ddd235d1adfe13ea3a3e0972019805`)、比较 `12679860675` (`sha256:72d78940a992fae4c60efd0157bc4f7d9d9c4fcc702ed097b577e5e4ff5e6d8d`)。比较 job 报告 Windows crop 与 Linux Canvas 均为 `558×335`、截图 10/10；控件矩形 184/184、离散栅格 36/36、导航选中框 10/10、树点线像素 102/102、中间滚动 2/2 均 PASS。暗色/亮色几何一致；10 组暗色截图生成配对证据，整体像素同一性明确 `NOT PASS`。比较 artifact 包含逐图 SHA-256 清单与 Overlay/Difference/Heatmap。
- P2B run [40382203135](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40382203135) 首次 attempt 的 Xvfb APT 安装失败；仅重跑失败 job 后，attempt 2 **P2B Xvfb 与 Weston 两个 job 均 PASS**。Xvfb artifact `12681997882` (`sha256:eb2ec95f941c81e179787a72dc6c1f9368ddd2fb7b6a75574b0c314cf285d5fd`)，Wayland artifact `12681398424` (`sha256:e9a57f49ccf67ba376b703d6e0e4d1db548aaea2d1b872c251aa572e1966c801`)，APT attempt 1/2 artifacts `12681383785` / `12680778845`。PR #7 上重复的设置页步骤按条件 `SKIPPED`；同 head 的 P2C run 已实际执行完整五页双语测试。
- attempt 1 APT 日志：Ubuntu 24.04.5 runner image `20261012.334.1`；`apt-get update` 成功、抓取 12.6 MB；安装 `fonts-noto-cjk` 61.2 MB 包时出现 `Ign`/重试，38 分钟时限到达并返回 `124`。未见 DNS、APT 锁或包不存在证据。可确认故障为包下载超时，底层网络/镜像/runner 原因未证实。attempt 2 同一字体包随 101 MB 总依赖下载在 4 秒内完成，APT 退出 `0`，XTEST 包括中文切换与 WM close（退出码 `0`）通过。该重试只证明本次恢复成功，不证明长期网络稳定。
- Build/governance/safety run [40382203131](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40382203131) PASS。本机非 GUI 回归：Zamak `0.2.0`、check、test（30/30）、build、Arch build、治理 33 项、Python/C 安全测试 3/3、Shell 语法、`git diff --check` 均 PASS。没有启动本机 GUI、操作 Hyprland/桌面输入、清理内存、调用 Polkit、修改 Swap/ZRAM 或写内核参数。

## P2C-4 生产源码 checkpoint 证据（2026-10-27 核对）

- PR：[#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7)，分支 `feat/issue-3-p2c-settings-ui` → `feat/issue-3-p2b-main-window`；Base `911a3d0b99b05f15bd0109dca475b0b5174e7888`；本轮生产源码与工作流 HEAD `6502342489e1e0a6dfc1877f826f89cab1ed0a9b`。固定 Windows 基准 v.3.5.2 / `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- P2C [Windows native + Linux Xvfb/XTEST + comparison run 40373093546](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546)：三个 job 全部 SUCCESS，且必需步骤均执行。Artifacts：Windows [`12673463315`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546/artifacts/12673463315)，digest `sha256:22eaadc99b82745dc548af7036bca0bafe7f0c0b38ae5956553d9976dcbfddf1`；Linux 生产 X11 [`12674390489`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546/artifacts/12674390489)，digest `sha256:f0b49b1577dec13c436cb967c65793ed7388ab0030e1fddc644e7636cc64d2f2`；几何/视觉/暗色/字体比较 [`12673073193`](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373093546/artifacts/12673073193)，digest `sha256:0897e9e15d2a5c8eff9616b6d1eef730fc55e2d454be28238bf39a66810388a3`。P2C Xvfb APT diagnostics artifact `12674160933`，Pillow comparison diagnostics `12672174873`。
- P2B [生产窗口 Xvfb/XTEST + Weston Headless/Pixman run 40373092535](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373092535) 两个 job 均 PASS；artifacts：X11 `12674265667`、Wayland `12673517780`。该 run 中因 P2C workflow 已覆盖设置页，重复的设置页 X11 step 按条件 SKIPPED；完整五页双语测试实际在同 HEAD 的 P2C run 执行并 PASS。PR build/governance/safety [run 40373092652](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373092652) 与 push build [run 40373081011](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40373081011) 均 SUCCESS。
- 当前 P2C Linux 证据核验：Windows 截图清单 **41/41**、Linux PNG 清单 **44/44**、暗色比较 SHA 清单 **70/70**、视觉比较 SHA 清单 **83/83** 均匹配。Linux 设置客户区保持 `558×335`；截图未经缩放或拉伸。Windows 环境为 Windows Server 2022 build 20348，1024×768，96 DPI / 100% scale；Linux 为 Ubuntu 24.04、Xvfb 1024×768×24、GTK 3.24.41、96 DPI、scale 1、GTK theme `Adwaita:light`。Linux 暗色状态由应用 View 菜单切换 Canvas palette，不修改 GTK 或桌面主题。
- 当前 build artifact 记录源码 SHA：`src/main.zm` `6d8408d46360757616f85609c85acf02534b0090a679d74bd4ed5c6175c34d8f`；`src/settings_ui.zm` `f821f36117ab5ba0ec757c7596d991dc980d8d805aa1b804952c3f9d2a17c920`；`src/ui_geometry.zm` `a2fd4af1de7b414d97dbe7c871d4048cc7dfab1095499f3998a0916eaba6e087`；`native/gui_window.c` `63c313bed8205e14266148b262064900dacf838e03c1aa0134dc2facff6f6326`。同 SHA 构建的生产二进制 SHA-256 `fa13bc97526e97debd189ef09dacffc31ebf085d168ffe520856d00cdbd30404`，helper `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08`。

## 字体度量与隔离候选结论

- Windows 原生控件 `WM_GETFONT`/GDI 报告逻辑 face alias `MS Shell Dlg 2`、8.25 pt、weight 400，典型 ascent/descent `11/2 px`。注册表 substitution 显示 Tahoma，但没有证据证明 GDI 实际加载的物理字体文件。Linux 默认 Pango `Sans` 8 pt / weight 400，英文字形匹配 DejaVu Sans、简中匹配 Noto Sans CJK SC，ascent/descent `10/3 px`。两边 ascent+descent 都是 13 px，但基线相对分配差 1 px；导出的 GTK text origin 不是 Win32 GDI baseline 的直接等价测量，因此不宣称基线零差异。GDI 字符串宽度与平均字宽已记录，Pango/GDI 没有独立的 tracking/character-spacing 调节量或等价报告列；当前差值是整串字形 advance 的度量结果，没有用任意字距补偿。Canvas 当前 API 没有每次 `Painter.text` 调用独立指定字体 family 的参数。
- Liberation Sans 只在隔离 Xvfb 进程通过临时 Fontconfig alias 测量并渲染候选截图，没有复制/提交字体文件，也没有改变生产默认字体。120 个同源控件度量样本（English 66、简中 54）中，English **64 改善 / 1 退化 / 1 不变**；简中 **12 改善 / 12 退化 / 30 不变**。宽度在 ±2 px 内的样本由默认 English 5/66 增至候选 56/66；简中由 4/54 增至 6/54。该候选不能同时稳定改善两种语言。
- 候选全页差异改善很小：English 对 Windows 的变化率只下降约 `0.0578–0.4093` 个百分点；简中变化率约从 `-0.0032` 到 `+0.0022` 个百分点（候选有略微回退或几乎不变的页）。因此没有足够的双语全页证据采用 Liberation Sans，也没有引入字体专用 C/GTK bridge；维持 DejaVu Sans + Noto CJK fallback，不缩放窗口或缩小字号。该实验只证明候选进程的度量变化，不证明字体具备 Windows 字体的完整字形/光栅等价。

## 深色主题对照

依据已采集 Windows v.3.5.2 原生暗色截图，production Canvas 暗色 palette 已对齐背景、导航、选中态、GroupBox 边线、字段、按钮、树状点线、滚动条及暗色 checkbox 面；浅色 palette、几何与生产字体均未改。Linux `GTK_THEME=Adwaita:light` 是隔离 runner 的固定主题，深色由应用自身 Canvas 状态实现，并非 GTK 原生暗色主题。

十组 558×335 的 Windows 原生 / Linux 生产配对截图已生成，并各有并排图、50% overlay、difference、heatmap 和 SHA。每组结论为 `CAPTURE_AND_COMPARE_PASS; PIXEL_IDENTITY_NOT_PASS`：

| 设置页 | English changed pixels | 简体中文 changed pixels |
|---|---:|---:|
| General | 10.2327% | 51.6086% |
| Memory cleaning | 12.6625% | 73.2932% |
| Appearance | 11.6311% | 54.3027% |
| Tray icon | 11.2534% | 54.6584% |
| Advanced | 5.9434% | 46.0889% |

这些变化率是整页原始差异，叠加字体/抗锯齿、Win32 与 GTK 控件栅格、平台状态，以及 12 项 Windows 简中运行时来源差异；没有把全部像素差异归因到字体。源语言差异仍 `DECISION_REQUIRED`。全像素 1:1 不通过，也没有因客户区尺寸一致而推定视觉等价。

## 主菜单与安全截图边界

- Linux 生产 XTEST 在 English 与简体中文下实际展开 File / View / Settings / Help，检查菜单文字/顺序、复选和快捷键显示、分隔槽跳过、语言子菜单、Enter、方向键、Escape 及焦点/窗口生命周期；安全的清理区域下拉只用于展示，清理项仍禁用，未调用任何清理动作。五页双语 light/dark 截图以及暗色 Reset/Close 焦点截图均已生成。
- Windows 原生 runner 取得四个主菜单英文截图。Runner UI Automation 未暴露托盘按钮；Windows 清理分割按钮没有安全 ExpandCollapse 路径；确认/结果弹窗需先触发真实清理。因此 Windows tray、Windows 清理区域 popup、确认/结果弹窗仍 `BLOCKED`，没有点击 Invoke 或发送清理命令。Linux safe clean-dropdown 截图不等于 Windows 原生 popup 验收。
- Weston 仅通过私有 Headless/Pixman socket 验证生产客户端连接与退出生命周期；没有 Wayland 鼠标/键盘输入，也没有活动 Hyprland 验收。日志中的 GTK seat critical / compositor shutdown diagnostic 原样保留，未归因或屏蔽。

历史 `40363399854` APT 超时及当时 P2B Weston 失败记录保留在后文作为事件历史；它们不代表本轮 exact-head 状态。其根因未确认。当前 exact-head 有界 APT update/install 均成功，不足以证明未来 runner 网络永远稳定。

## 固定范围

- 上游基准：Mem Reduct v.3.5.2，tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- PR #7：`feat/issue-3-p2c-settings-ui` → `feat/issue-3-p2b-main-window`，Draft，禁止合并。
- P2C-3R1 已验收基线：HEAD `2c745710210c8794b52a0e9eb19dd1830c00d8d6`。
- 本轮首个验证提交：`a58745f9b5ed029a00f1ce61d21e5348ea2dcaec`。它保留原设置客户区 558×335、184 项资源几何阈值、导航/树线/栅格样本与中文来源决策。
- 生产字体默认值未改变。Liberation Sans 仅在独立 Xvfb 测试进程中通过临时 Fontconfig 配置作为候选；只复制字体包许可说明到 CI artifact，不提交字体文件。

## 已知字体证据与待核差异

P2C-3 同源文本比较有 120 项：25 项宽高差在 2 px 内，95 项仍存在度量差异；其中英文文本宽度 Linux/Windows 比值中位数为 1.1469（66 个样本）。

Windows `WM_GETFONT` 报告逻辑别名 `MS Shell Dlg 2`、8.25 pt、weight 400；GDI ascent/descent 为 11/2 px。注册表 `FontSubstitutes` 指向 Tahoma，但现有证据不能证明 GDI 实际加载的物理字体文件。Linux Canvas 为 8 pt `Sans` / weight 400；英文常见匹配 DejaVu Sans，简中为 Noto Sans CJK SC，Pango ascent/descent 为 10/3 px。GDI 与 Pango 的字符串度量、字高和字形栅格分开记录；上下文文字锚点不当作 Win32 基线的直接等价值。

`std/ui` 的 `Painter.text` 暴露字号和粗细，但没有逐次字体家族参数。候选测试通过进程级 Fontconfig 映射评估可分发的 Liberation Sans；此方法会影响该测试进程内所有 Canvas 文本，不代表生产设置已生效。是否应增加受控字体桥接取决于候选字体的 GDI 度量和实际全页比较结果。

候选字体精确结果与限制见上方“字体度量与隔离候选结论”。`eea82a8` 对测试进程的 Fontconfig alias / Pango family assertion 修正已在 exact-head 候选作业执行；生产字体未改。

## 深色主题与菜单

Linux CI 将 GTK 主题固定为 `Adwaita:light` 并单独记录；通过生产程序 View 菜单切换应用自己的 Canvas 调色板。它不修改 GitHub runner 全局 GTK 主题，也不代表 GTK 原生暗色主题。Windows 配对截图由隔离 Windows 2022 runner 通过原版 `IDM_USEDARKTHEME=159` 产生。

依据旧生产截图/Windows 3.5.2 原生暗色截图（run `40355264229`，只作为修复前证据）逐像素取样，修正背景 `#242424→#171717`、导航 `#303030→#171717`、选中底色 `#3C5066→#333333`、GroupBox 边线 `#707070→#404040`、字段面 `#303030→#404040`、按钮面 `#3A3A3A→#272727`、树状点线 `#9A9A9A→#6D6D6D`、滚动条轨道/滑块 `#F0F0F0/#D7D7D7→#676767/#4D4D4D`。窗口背景目标是实测 Windows 的 `#171717`。首个 General 页面未勾选复选框内点为 Windows `#FFFFFF`、Linux 修复前 `#303030`；本次仅在应用暗色状态把复选框面改为白色，Light 模式仍使用既有 `#F4F4F4`，保留通过的 light 栅格基线。所有未实现设置仍按安全边界显示为禁用态，故其文字/勾选灰度差异归类为安全状态差异，不伪装成已启用。

当前 palette 修正确由 exact-head hosted 生产截图确认；dark/light 共用已验证的生产几何，暗色 PNG、GDI/Pango 度量与差异率均关联本报告列明的同一批 Actions artifacts 与 SHA manifest。

生产 XTEST 覆盖 File / View / Settings / Help 在 English 和简体中文下的展开截图、语言子菜单两种选择状态、清理区域下拉外观、分隔行不参与鼠标/键盘命中、Escape 关闭，以及暗色五页双语截图。所有清理项仍禁用；截图路径不触发清理。暗色 Reset / Close 焦点截图已随本轮同 head artifact 采集。

深色截图、几何复核、Overlay / Difference / Heatmap 与 SHA 已在同一精确 head 生成；十组全页差异率仍是 compare-only，不代表像素验收通过。

## 冻结视觉验收与剩余差异

沿用 P2C-3R1 精确证据：控件几何 184/184 PASS、最大分量偏差 1.5 px；导航选中框 10/10 PASS；树线像素样本 102/102 PASS；控件栅格 36/36 PASS；中间滚动状态 2/2 已采集。P2C-4 不改变这些阈值、Windows 原图或简体中文来源矩阵中的 12 个 `DECISION_REQUIRED` 项。

P2C-3R1 十组全页变化率为 5.0420%–14.8853%，只可作为历史趋势背景。完整像素 1:1 仍 `NOT PASS`。P2C-4 新截图和 candidate 指标必须按各自 artifact/PNG SHA 记录，不可把不同 run 的截图混成一组。

| 差异类别 | 当前处理 |
|---|---|
| 几何 | 冻结已通过的 184 项；深色模式比较同源矩形。 |
| 字体/字形 | GDI/Pango 度量和候选截图分别比较；不缩放界面、不缩小生产字号。 |
| 抗锯齿/控件栅格 | 与坐标和文字内容分开报告；离散栅格样本不代表整控件相等。 |
| 安全禁用状态 | 尚未实现或需特权的动作保持禁用。 |
| 中文来源差异 | 12 项保持 `DECISION_REQUIRED`，不混入字体差异率解释。 |
| 平台不可等价 | GTK/Pango 与 Win32/GDI 控件绘制不能承诺像素完全相同。 |

## 安全边界

本机仅执行治理、静态、Zamak/C 测试和构建，没有启动本机 GUI/Xvfb/Wayland、访问 Hyprland IPC 或注入本机输入。GitHub 测试仅使用临时 runner、Xvfb/XTEST、Wayland headless 生命周期；没有 root GUI、Polkit、drop_caches、Swap/ZRAM 或内核写入。

## 历史状态快照（P2C-4 初次验证阶段；由上方 exact-head 结果覆盖）

- PR #7 仍 OPEN / Draft；branch `feat/issue-3-p2c-settings-ui`，base `feat/issue-3-p2b-main-window`，HEAD `eea82a8c2cafb16b25acf3d319e084335548356d`，与 `origin` 一致。
- P2C run [40363399854](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40363399854) 的 Windows job 已 PASS；artifact `12670040771`，GitHub artifact digest `sha256:74ca2a4a52afcde5fe04334918587a77b1fc6c469ffce03916dbb9920aa616c6`。release tag/source、官方 release 资产、setup detached PGP signature 和 3.5.2 版本均由 runner 核验。Windows Server 2022 Datacenter build 20348，1024×768，96 DPI / 100% scaling；配置使用 Light Apps theme。实测设置窗口外框 574×374、客户区 558×335。GDI 记录 `MS Shell Dlg 2` 逻辑字体，8.25 pt / weight 400 / ascent 11 / descent 2 px；注册表替代为 Tahoma，但尚不能证明进程实际加载的物理字体文件。
- 该 Windows artifact 的 `result.txt` 列出 41 个截图哈希，本地逐项 SHA-256 核对 41/41 匹配。包含主窗与五页 EN/简中 light/dark 截图、主菜单四项 EN 截图及两种语言的 Memory cleaning 中段滚动截图。tray menu 未被 runner UI Automation 暴露；clean-region menu 的安全展开模式不可用；清理确认/结果窗口若无清理则无法打开，所以三类均保持 `BLOCKED`，未触发清理。
- 同一 P2C run 的 Linux job 截至 17:03 UTC 仍在 `Install runner-only build and X11 dependencies`。APT update 已完成，安装步骤开始于 16:53:35 UTC；GitHub 对在运行 job 暂未提供日志，因此**APT 停滞根因未确认**。候选字体、Linux 生产截图、暗色对照器和 Overlay/Heatmap 尚未执行，不能标 PASS。
- 当前 HEAD 的 PR `build-and-test` run [40363344887](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40363344887) 已 PASS：治理、Shell、C/Python 安全、Zamak 版本/check/test/native build 全部成功。另有同 SHA push build run `40363337339` PASS。P2B 生命周期 run `40363344884` 尚 pending，队列中上一 HEAD 的 Weston job `40355264157` 仍运行；不取消正在生成验收证据的作业。
- 本机无 GUI 的本轮验证：Zamak 0.2.0；project verification 33 项 PASS；Python/C 单测 3/3 PASS；Zamak 30/30 PASS；Zamak check/build PASS；`scripts/build-arch.sh` PASS；全部 shell 脚本 `bash -n` PASS；`git diff --check` PASS。未运行本机 Xvfb/Wayland/桌面输入，也未启动 GUI 或访问活动 Hyprland。

**准确限制：**以上成功项只覆盖各自列明的 HEAD/job。Linux 截图/字体实验/暗色比较/Weston 生命周期尚无本轮 exact-head 结果；这些仍为 `IN_PROGRESS`，不是 PASS。最终证据须在当前最终 HEAD 重跑并记录各 artifact / PNG SHA。

## 2026-10-26 17:44 UTC 状态更正

本节覆盖上方较早查询的 job 状态。P2C exact-head run `40363399854` 已结束 FAILURE：Windows 原版来源 job PASS，Linux Xvfb job 在 APT 依赖安装处 FAILURE，Windows/Linux comparator 因 Linux 依赖 job 失败而 SKIPPED。诊断 artifact `12672670397`（digest `sha256:8a3a5b0912138113ef9a9af85ad28c888e5e99cd2933a45be58cf5f9f7fefcd2`）记录 `apt_exit_code=124`，APT 子命令达到 38 分钟硬超时。APT 日志把 `fonts-noto-cjk` 61.2 MB deb 标为尚未完成下载项，并显示一次 `Ign` 后重试；没有 DNS、锁或缺包证据。故障表现为大字体包下载阶段超时，底层网络/镜像根因未确认。字体候选、Linux 生产截图、深色配对图及比较 artifacts 均未生成。

Windows artifact `12670040771`（digest `sha256:74ca2a4a52afcde5fe04334918587a77b1fc6c469ffce03916dbb9920aa616c6`）的 41/41 PNG SHA 与 `result.txt` 相符，但只证明 Windows 3.5.2 参考资料。原始 artifact 已保存在用户指定产物目录 `报告与研究/Mem Reduct Linux/P2C-4/run-40363399854/`。同 SHA build run `40363344887` 及 push build run `40363337339` 均 PASS。

P2B exact-head run `40363344884` 在本次查询（17:44 UTC）仍处 APT 安装步骤，尚未有可报告的最终状态；不取消该验收作业。工作树的暗色颜色修改仍未提交，字体与生产字号未改，新的 exact-head 截图/比较验证尚待完成。

17:50 UTC 复查：同一 P2B run 的 production Xvfb/XTEST job 已 PASS。APT 安装耗时约 35 分钟；主窗口隔离输入测试通过，设置页重复场景按 PR 条件 SKIPPED（由 P2C workflow 覆盖）。Weston job 仍运行在其 APT 安装步骤，故 Wayland lifecycle 尚未判定。该 run 使用的还是推送头 `eea82a8...`，没有本工作树的暗色 palette 修正，不可作为本轮截图验收。

17:52 UTC 最终状态：P2B run `40363344884` 整体 FAILURE。Xvfb job PASS，APT dependency artifact `12673795414`，生产主窗 XTEST artifact `12673540822`；Weston job 因同一 APT 38 分钟超时 FAILURE，诊断 artifact `12672539453`（digest `sha256:1b7bd7c60786698250248e7196b8921e911431376b9f7661dcdf91110ccdcafb`）。Xvfb APT 成功约用 35 分钟，Wayland apt 日志同样在 `fonts-noto-cjk` 61.2 MB 下载项停止。这个共现不证明下载拥塞由并发导致，但支持减少重复和无用依赖；当前工作树移除仅做英文生命周期验证的 Weston job 对 `fonts-noto-cjk` 依赖，并令 P2B/P2C workflow 共享 PR concurrency group、关闭运行中取消。两项 workflow 改动都必须由新 head Actions 再验证。
