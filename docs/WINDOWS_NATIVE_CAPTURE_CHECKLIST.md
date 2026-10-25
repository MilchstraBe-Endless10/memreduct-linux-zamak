# Windows Native 截图采集清单（P2A-R2）

用途：在真实 Windows 设备或隔离 Windows VM 上，采集 Mem Reduct 3.5.2 的可复核原生 UI 基准。Wine 只能作为辅助交互参考，不能标记为 Windows Native。

## 1. 固定发布版本与安全边界

- 上游仓库：Henry++ `memreduct`。
- 版本：`v.3.5.2`。
- 权威源码 Tag commit：`2f5c55d15f69a2c88859e036b16c6f4c971e1968`。
- 只使用官方 Release 页面提供的 3.5.2 可执行文件；记录 Release URL、原始文件名、下载日期、签名信息（若有）和 SHA-256。不得把后续 master/3.5.3 开发快照称为 3.5.2。
- 在隔离 VM 或专用测试机采集。不要在日常机器上执行实际内存清理、Swap 操作或更改系统内存设置。
- 清理确认弹窗只打开后取消。结果弹窗仅当 Release 自带的安全演示/测试路径可触发时采集；否则标记 BLOCKED，不通过真实清理制造结果截图。
- 托盘状态只记录实际平台能力，不虚构托盘菜单、通知或中键行为。

## 2. 冻结 Windows 显示条件

每一轮采集先记录：

- Windows Edition、版本、OS build。
- 显示器物理分辨率、DPI、缩放百分比、方向。
- Windows 主题/应用主题、对比度模式（如使用）、字体平滑设置。
- Mem Reduct UI 语言：English 或 简体中文（分开采集，不要同屏拼接双语）。
- Release 可执行文件版本、来源 URL 和 SHA-256。
- 客户区宽高（窗口外框和客户区分开记录）；设置页窗口同样测量。
- 采集日期、时区、截图工具及操作方式。
- 每个状态应使用同一显示器、同一 DPI/缩放、同一主题、同一窗口状态；源图保留原样，不裁剪、不缩放、不覆盖。

可用 PowerShell 记录 OS 版本和可执行文件散列：

```powershell
Get-ComputerInfo | Select-Object WindowsProductName, WindowsVersion, OsBuildNumber
Get-FileHash .\memreduct.exe -Algorithm SHA256
(Get-Item .\memreduct.exe).VersionInfo | Format-List FileVersion, ProductVersion, CompanyName
```

截图创建后也记录 SHA-256：

```powershell
Get-FileHash .\captures\*.png -Algorithm SHA256
```

## 3. 采集矩阵

每个可用 locale 至少记录以下状态；主题只在原版支持时采集并明确注明：

- 主窗口空闲：English / 简体中文。
- 设置页面：常规、内存清理、外观、托盘图标、高级；分别采集英文和简体中文。
- 主菜单：File、View、Settings、Help；每个展开菜单和相关子菜单。
- 清理 Split Button：主区、下拉箭头、每个清理区域的菜单项及选中/禁用状态；只打开菜单，不点击清理主操作。
- 托盘：图标、根菜单、清理区域、占用阈值、定时清理间隔及禁用/选中状态。若环境没有 Windows 托盘展示能力，单列，不伪造。
- 对话框：清理确认（在不执行清理的前提下打开并取消）、重置确认（打开并取消）、关于、更新状态。
- 清理结果、错误和权限拒绝状态：仅从安全演示/测试流程取得；否则标记 BLOCKED。
- 亮色、暗色：记录原版设置方式；若原版版本没有该主题能力，标记不适用，不依赖 Windows 系统深色模式推断应用外观。

建议文件名保持稳定且不含个人信息，例如：
`W352-MAIN-EN-L.png`、`W352-MAIN-ZH-L.png`、`W352-SET-GEN-EN.png`、`W352-SET-GEN-ZH.png`、`W352-MENU-FILE-EN.png`。

## 4. 每张图的元数据记录模板

| 字段 | 值 |
|---|---|
| Capture ID / 文件名 | |
| Mem Reduct 显示版本 | |
| 官方 Release URL | |
| EXE SHA-256 / 签名状态 | |
| Windows Edition / Version / Build | |
| 分辨率（物理像素） | |
| DPI / 缩放比例 | |
| 窗口客户区宽×高（物理像素） | |
| 标题栏/系统装饰是否包含 | |
| 主题/对比度模式 | |
| 语言 | |
| 字体平滑设置 | |
| 采集日期 / 时区 | |
| 截图工具 / 方法 | |
| 源图 SHA-256 | |
| 操作路径 / 是否取消了特权动作 | |
| 证据状态（PASS / BLOCKED / NOT RUN） | |

## 5. 原件保管与像素对照准入

- 原始 PNG 只读保存；派生裁切、overlay、diff 另存，不覆盖原图。
- 不发布含账户名称、桌面通知、私人文件或其他应用窗口的截图。
- 分享前检查图像元数据和画面隐私，公开仓库中不得包含用户路径、用户名、令牌或其他个人数据。
- Windows RC 坐标以 Dialog Units 为基础。先用真实 Windows 字体和 DPI 建立 DLU→像素转换；不可把资源坐标直接当物理像素。
- Linux 对照图必须在获批的独立测试会话中采集。没有真实的 Windows 与 Linux 对应截图、环境元数据、源图 SHA、overlay 与差异图时，像素验收保持 NOT RUN。
- 2 px 控件几何偏差是后续目标阈值，不是截图采集完成即可通过的结论。
