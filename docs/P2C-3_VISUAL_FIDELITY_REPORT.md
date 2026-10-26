# P2C-3 — Mem Reduct 3.5.2 原生视觉差异收敛

## 固定基准

- 上游：Henry++ Mem Reduct `v.3.5.2`，tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- Windows 原生、Linux 生产截图及比较历史证据：run [40296808973](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40296808973)，artifacts [Windows 12638686728](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40296808973/artifacts/12638686728)、[Linux 12638048963](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40296808973/artifacts/12638048963)、[比较 12638282898](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40296808973/artifacts/12638282898)。
- Windows：GitHub-hosted Windows Server 2022、96 DPI、100% 缩放；设置外框 574×374，客户区 558×335。Linux：GitHub-hosted Ubuntu 24.04、Xvfb 1024×768、GTK 3.24.41；Canvas 558×335。
- 原图只按 Win32 实测客户区起点裁剪，不缩放、不拉伸、不平移、不重采样。

## P2C-2 全页变化率历史基线

这些值来自 exact-head 比较 artifact 12638282898。它们是原始变化像素比例，只用于回归比较，不代表像素验收。

| 页面 | English | 简体中文 |
|---|---:|---:|
| General | 13.8234% | 12.2297% |
| Memory cleaning | 16.7341% | 16.6041% |
| Appearance | 14.5183% | 13.8961% |
| Tray icon | 16.8042% | 15.8268% |
| Advanced | 7.3921% | 6.8528% |

## P2C-3 代码变化

- 英文内存清理列表的两个版本后缀按 3.5.2 固定源码改为 `win8.1+`、`win10+`；简中保持项目已要求的独立本地化。
- Windows runner 使用 `WM_GETFONT`、`LOGFONT`、GDI `TEXTMETRIC` 与文本测量导出实际控件字体和宽高；IDC_REGIONS / IDC_COLORS 行名由 Windows UI Automation 读取，避免在采集器里用硬编码文本冒充运行时显示。
- 隔离 Windows runner 采集 `IDC_REGIONS` 的顶部、中间、底部状态，并输出资源 ID 几何；Linux 生产 Xvfb 用同样的滚动偏移截屏并导出 Canvas 文本度量。
- 比较器输出来源文案矩阵、字体/文本度量、中间滚动墨迹框、逐控件几何、overlay/热力图、P2C-2 全页变化率回归表及 SHA-256 清单。两端图像不做缩放或任意配准。
- Windows runner 仅尝试打开原版菜单/分割按钮菜单、托盘上下文菜单和原版暗色主题命令。清理确认/结果弹窗若需执行真实清理才能触发，则保持 `BLOCKED`，不发送清理命令。
- 测试中用 GTK/Pango 只读记录系统字体解析和度量；没有复制或提交 Windows 专有字体。Zamak Canvas 没有按调用指定字体家族的接口，故在新证据生成前不盲目改字号或基线。

## 首轮 exact-head CI 及修复

- 首轮 P2C-3 head：`1f93abe75a3ba4dadf01860a79a44b62ccfdce0b`；PR #7 仍 OPEN / Draft。
- Windows release 验证确认官方 tag、release SHA 与 setup PGP 签名均通过，但新增 C# Win32 类型在编译时失败：字段 `LOGFONTW.CharSet` 遮蔽 `CharSet` 类型。对应 Windows run [40307471183](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40307471183)。
- 同一 head 的 Xvfb/Weston 在 GUI 启动前失败：测试探针直接包含 GTK/Pango 头，Zamak 生成编译单元没有 GTK include flags。对应 run [40307471104](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40307471104)；比较 job 因上游 job 失败而跳过。
- 修复：将 Win32 结构布局限定为 `System.Runtime.InteropServices.CharSet.Unicode`；探针改为与现有桥接一致的 header-free GTK/Pango ABI 声明，保持项目原构建链路不新增 GTK include-path 要求。
- 修复后的本机无 GUI 检查：`cc -fsyntax-only` PASS；Zamak 0.2.0 `check` PASS、`test` 29/29 PASS、`build` PASS。Python/C 安全单测 3/3 和 33 项项目治理检查在本轮变更前 PASS，最终 head 将再次运行。
- 修复后 exact-head Windows/Xvfb/Weston Actions、截图 SHA、变化率、几何行数和字体数据：待重新推送和运行；首轮失败不能标作通过，也不将历史截图/哈希作为本轮结果。

## 仍需单独报告的差异

- P2C-2 的历史几何是 172 项 PASS、12 项中间滚动状态未比较；P2C-3 只有在 Windows 与 Linux 都采到中间状态并由资源 ID 匹配后，才可报告新增的 ≤2 px 结果。
- Win32 与 GTK 的字体栅格化/控件绘制、Linux 安全禁用状态、Windows 原版简中未翻译的 8 个内存区域与 2 个 Advanced 标签，分别属于不同差异类别。
- 语言差异逐资源记录于 [P2C-3_SOURCE_LANGUAGE_MATRIX.md](P2C-3_SOURCE_LANGUAGE_MATRIX.md)。在产品负责人确认前，Linux 简体中文仍保留翻译，不以视觉一致为由退回英文。
- 像素变化率下降不等于全页像素相同；P2C-3 结束时仍需单独给出 `NOT PIXEL EQUIVALENCE` 或 `BLOCKED` 结论。
