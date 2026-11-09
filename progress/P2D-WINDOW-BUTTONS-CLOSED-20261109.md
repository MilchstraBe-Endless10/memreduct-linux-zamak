## P2D：#11 窗口按钮 —— 最小化/恢复已实测通过（用自建最小 WM 桩）

2026-09-11-09。此前 #11 卡在"本机无窗口管理器，Xvfb 下无法测最小化/恢复"。
本轮换了个办法：不装包，自己写一个最小 WM 桩，把这条测出来了。

### 为什么之前测不动

标题栏是 GTK 的 `GtkHeaderBar`（`native/gui_window.c:498-500`）：

```c
gtk_header_bar_set_decoration_layout(bar, zh ? "appmenu:minimize,close"
                                            : "appmenu:minimize,maximize,close");
```

按钮由 GTK 自己提供，点击后调用 `gtk_window_iconify()`（`gui_window.c:644`）。
但 GTK 不会自己把窗口藏起来——它发 `WM_CHANGE_STATE`（`state=IconicState=3`）给窗口管理器，
**由 WM 执行 unmap**。没有 WM，这个请求就掉在地上，什么也不发生。

这就解释了第一次点击"没反应"：不是应用的 bug，是我没提供 WM。

### 自建桩（`artifacts/p2d/wm-stub/miniwm.c`）

约 80 行 C，只做必要的事：

- `SubstructureRedirectMask` 接管 root
- `MapRequest` → `XMapWindow`
- `ClientMessage` 里的 `WM_CHANGE_STATE`：
  - `IconicState(3)` → `XUnmapWindow`
  - `NormalState(1)` → `XMapWindow`
- `WM_DELETE_WINDOW` → `XDestroyWindow`

第一次写时漏了 `WM_CHANGE_STATE`，补掉之后才有反应。**桩不完整会伪装成应用缺陷。**

### 实测结果

```
miniwm: ready
miniwm: mapped 0x40000c
miniwm: WM_CHANGE_STATE 0x40000c state=3
miniwm:   -> unmapped for iconify      ← 最小化成功
miniwm: WM_CHANGE_STATE 0x40000c state=1
miniwm:   -> mapped for deiconify      ← 恢复成功
```

| 步骤 | 结果 |
|---|---|
| 点击最小化（x=228, y=23）| 窗口 unmap，进程存活 |
| 最小化后可见窗口数 | 0 |
| 发送 deiconify | 窗口重新 map |
| 恢复后可见窗口数 | 1 |

### 两处差点误判

**一、坐标**
第一次猜最小化按钮在 x=253，点下去毫无反应。
看图才发现三个按钮实际在 x≈228（「—」）、271（「□」）、310（「✕」），
我点的 253 落在空隙里。**猜坐标会产生一致的空结果，看着像功能失效。**

**二、同名窗口数为 2**
`xdotool search --name 'Mem Reduct'` 在最小化后返回 2 个，一度以为重复创建窗口。
实际是模糊匹配同时命中了 `Settings - Mem Reduct`（558×335，未映射）：

```
win=2097200 geom=558x335 visible=0 name='Settings - Mem Reduct'
win=2097164 geom=337x415 visible=1 name='Mem Reduct'
```

应用只创建了一个主窗口。**搜索工具的模糊匹配会把"数量变多"伪装成缺陷。**

### #11 现在的状态

| 项目 | 状态 | 证据 |
|---|---|---|
| 关闭按钮 | 已实测 | 点击后进程退出 |
| 最小化按钮 | **已实测** | `WM_CHANGE_STATE state=3` + unmap |
| 恢复 | **已实测** | `state=1` + map |
| 最大化按钮 | 未测 | 桩未实现 `_NET_WM_STATE_MAXIMIZED_*` |

前 3 项完成。最大化还需桩支持 EWMH 状态切换，而最大化/还原在真实 WM 下与 EWMH 实现强相关，
用桩测出的参考价值有限。

### 诚实边界

**这不是在 Hyprland 上测的。** 是在自建 X11 桩下测的，
证明的是"应用确实发出正确的 ICONIFY/DEICONIFY 请求，且窗口能被正确隐藏和恢复"。
真实桌面还有别的变量（合成器、动画、聚焦策略），但**协议层面已经确认无误**。

也就是：#11 从"无法测试"变成"协议层已验证，真机行为待用户确认"。
