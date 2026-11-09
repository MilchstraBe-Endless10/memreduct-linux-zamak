## P2D：键盘可达性审计 —— 主操作按钮不可达

2026-09-11-09。此前所有点击验证都走鼠标（xdotool click / XTEST），
从未验证键盘用户能否完成主操作。本轮补上，结果是否定的。

### 实测

Xvfb 下按 Tab 七次并逐次截图：

| 按 Tab 次数 | 焦点框位置 |
|---|---|
| 1 | File |
| 2 | View |
| 3 | Settings |
| 4 | Help |
| 5 | （回到 File）|
| 6 | File ……同一循环 |
| 7 | 同上 |

焦点在 0..4 之间循环，**「清理内存」按钮从未获得焦点**。

### 源码确认

```zig
fn next_focus_index(focus: f64) -> f64:
    let next = focus + 1.0
    if next > 4.0: return 0.0
    next
```

- 上界是 4，只有 5 个位置
- `menu_item_has_keyboard_focus` 只用于菜单栏 4 项
- 全文件搜 `clean_focus` / `clean_button_focus`：**不存在**
- `key()` 里 Enter/Return 分支只处理菜单项，没有「焦点在清理按钮时执行清理」的分支

### 影响

只用键盘的用户**无法执行这个产品的主功能**——清理内存。
可以用键盘开设置、切语言、退出，就是不能清理内存。

这不是"体验不够好"，是**关键功能对键盘用户完全不可用**。

### 为什么之前没发现

前几十轮全部用鼠标验证（`xdotool click 1`、XTEST 注入），
鼠标路径一直是通的，自然发现不了。这是我验证覆盖上的盲区：
**只测了"点击能工作"，没测"不用点击能不能工作"。**

### 未修复的原因

要修需要：

1. 给 `MainWindow` 加一个主按钮焦点状态（或把 focus 上界扩到 5）
2. `key()` 加分支：焦点落在按钮时 Enter 触发清理
3. 画焦点框（现在按钮在 hover/menu_focus 时才有高亮，需区分键盘焦点高亮）
4. 高保真约束：v3.5.2 的 Windows 版用 Tab 能否到达清理按钮需要对照原版行为，
   若原版也不可达，则这是"复刻原版"而非缺陷，**必须先确认原版行为**。

第 4 点决定这是修 bug 还是保真。我无法凭现有资料断定（没有原版可运行环境），
需要处理后确认：要么装 Windows VM 跑原版，要么查原版源码的 Tab 顺序。

已在 Issue 记录，等确认原版行为后再决定改不改。

### 结论（上游源码判定）

从上游仓库按 tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968` 取到
`src/resource.rc`（371 行，已存档到 `artifacts/p2d/upstream/resource.rc`）。

主对话框定义：

```
IDD_MAIN DIALOGEX 0, 0, 224, 214
STYLE WS_POPUPWINDOW | WS_MINIMIZEBOX | WS_CAPTION | DS_SHELLFONT | DS_MODALFRAME | NOT WS_VISIBLE
MENU IDM_MAIN
{
	CONTROL    "", IDC_LISTVIEW, WC_LISTVIEW,
	           NOT WS_BORDER | LVS_SHOWSELALWAYS | LVS_REPORT | LVS_NOCOLUMNHEADER,
	           8, 8, 208, 174

	PUSHBUTTON "", IDC_CLEAN, 8, 192, 208, 14,
	           BS_SPLITBUTTON | BS_CENTER | BS_VCENTER
}
```

**两个子控件都没有 `WS_TABSTOP`。**

在 Win32 里，只有带 `WS_TABSTOP` 的控件才进入 Tab 顺序。主对话框的
`IDC_LISTVIEW` 和 `IDC_CLEAN` 都没这个样式，所以：

**原版 v3.5.2 的「清理内存」按钮本来就不在 Tab 顺序里。**

### 判定：这不是缺陷，是忠实复刻

上一轮我把它记为"关键功能对键盘用户完全不可用"，并因为不确定原版行为而搁置。
现在证据明确：

| 侧 | Tab 顺序 | 清理按钮可达？ |
|---|---|---|
| 上游 v3.5.2 | 对话框内无 WS_TABSTOP 控件 | **否** |
| 本项目 | 焦点 0..4 仅菜单栏 | **否** |

两侧一致。**按产品硬约束（高保真复刻、不得自主重新设计），不应添加这个能力。**

如果要做，那是一个**产品改进决策**，不是 bug 修复，必须由用户批准，
并且应当同时意识到：这会偏离"高保真复刻"这条硬约束。

### 撤回头条措辞

上一轮文档标题写的是"主操作按钮不可达"，读起来像缺陷。
准确说法应是：**"主操作按钮不可达 —— 与上游 v3.5.2 行为一致，属复刻范围内。"**

### 附带确认

上游 `IDD_SETTINGS_GENERAL` 里 `IDC_LANGUAGE` 带 `WS_TABSTOP`，
其余 AUTOCHECKBOX 不带——说明原版设置页也只有语言下拉可 Tab 到。
本项目设置页的键盘行为与此一致（`P2C-4_FONT_THEME_MENU_REPORT.md` 记录了语言 ComboBox 的完整键盘支持）。
