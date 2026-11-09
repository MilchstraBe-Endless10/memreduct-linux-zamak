## P2C：设置窗中文界面实测通过（含两处方法性发现）

2026-11-09。此前只验证了主窗口与跨进程面板的中文，设置窗从未截图确认。

### 中文实测

用 F2 打开设置窗（源码 `main.zm:2250` 的 F2 绑定），Xvfb 下截图：

| 区域 | 内容 |
|---|---|
| 窗口标题 | `Settings - Mem Reduct`（GTK 标题栏，沿用英文；与原版一致）|
| 侧栏 | 常规 / 内存清理 / 外观 / 托盘图标 / 高级 |
| 选项组 | 窗口始终置顶 / 开机自启 / 最小化启动（不显示窗口）/ 在内存清理前手动确认 / 跳过"用户账户控制（UAC）"提示 / 自动检查更新（推荐）|
| 语言 | `语言: (Language)` / `选择语言：` / 下拉显示 `简体中文` |
| 按钮 | `重置` / `关闭` |

无一处英文残留（除标题栏与 `(Language)` 两处，见下）。

### 发现一：`语言: (Language)` 是刻意保留，不是漏译

源码 `settings_ui.zm:439`：

```zig
frame(s, p, w, h, 108, text(zh, "Language: (Language)", "语言: (Language)"), fg)
```

中文分支同样带 `(Language)`。这是原版 3.5.2 的写法，刻意保留。
**扫描时若只看到中文里混英文就"顺手修掉"，会破坏高保真复刻要求。**
这类情况必须先查源码确认是有意还是遗漏。

### 发现二：无窗口管理器时 `xdotool key --window` 不生效

这轮花了多次尝试才打开设置窗，原因是我一直用：

```
xdotool key --window "$WID" --clearmodifiers F2       # 无效
```

Xvfb 无 WM 时焦点不归任何窗口，`--window` 发送的按键到达不了客户端，
`XGetInputFocus returned the focused window of 1` 就是证据。必须改成：

```
xdotool windowactivate --sync "$WID" || xdotool windowfocus "$WID"
xdotool key --clearmodifiers F2                        # 不带 --window
```

区别在于：前者绕过窗口管理器直接指定接收者，后者先把焦点真正交给窗口再发送。

### 发现三：漏设 `GDK_SCALE` 会让几何数据翻倍

临时脚本里忘了 `GDK_SCALE=1`，测出主窗口 **674×830**——正好是 337×415 的两倍。
差点当成程序缺陷去查。所有 Xvfb 测量脚本都必须显式设 `GDK_SCALE=1`，
否则得到的尺寸是缩放后的值，和源码断言的基准对不上。

### 结论

设置窗中文完整。`settings_ui.zm`（1522 行）经扫描无硬编码英文残留。
这三条经验已写在上方，后续脚本与扫描都按此执行。
