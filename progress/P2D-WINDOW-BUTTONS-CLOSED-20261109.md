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

---

## 补充：最大化 / 还原也测通了（同一桩，第二轮补强）

上一轮把最大化记为"未测"。本轮把它补上了，过程中撞到一个关键点。

### 第一次尝试：两次点击都发 maximize，没有还原

```
miniwm: _NET_WM_STATE action=1 ... -> maximized to 1024x768
miniwm: _NET_WM_STATE action=1 ... -> maximized to 1024x768     ← 又是 1，不是 0
```

我一度怀疑是程序的 toggle 逻辑坏了。查源码注释（`gui_window.c:625-628`）：

> GtkHeaderBar's minimize/maximize buttons only emit **gtk-window-toggle-maximized**

**toggle 语义是对的。** 问题在我的桩：GTK 判断"当前是否已最大化"靠读窗口的
`_NET_WM_STATE` 属性，而我的桩改完窗口尺寸却**没写这个属性**。
GTK 每次都读到"没最大化"，所以每次都往"最大化"方向 toggle。

补上属性维护后才正确：

```c
/* 最大化时 */
Atom states[2] = { max_h, max_v };
XChangeProperty(d, w, net_wm_state, XA_ATOM, 32, PropModeReplace,
                (unsigned char *)states, 2);
/* 还原时 */
XChangeProperty(d, w, net_wm_state, XA_ATOM, 32, PropModeReplace, NULL, 0);
```

### 结果

```
miniwm: _NET_WM_STATE action=1 ... -> maximized to 1024x768, _NET_WM_STATE set
miniwm: _NET_WM_STATE action=0 ... -> restored to 337x415,  _NET_WM_STATE cleared
```

| 步骤 | 几何 | 事件 |
|---|---|---|
| 初始 | 337×415 | — |
| 点最大化 | **1024×768** | `action=1` |
| 再点（还原）| **337×415** | `action=0` |

### 这次误判的性质与上一次不同

- 第一次（未处理 `WM_CHANGE_STATE`）：**桩缺能力**，请求变成无操作。
- 这一次（未维护 `_NET_WM_STATE`）：**桩撒了谎**——窗口实际变了，
  但状态属性说没变，导致程序基于错误信息做决策。

第二种更隐蔽：程序的行为看起来像有 bug（toggle 失效），
实际是环境提供了假信息。**这跟"设备没反应"和"设备说成功了但没做"的区别一样。**

### #11 完成情况（更新）

| 项目 | 状态 |
|---|---|
| 关闭 | 已实测 |
| 最小化 | 已实测 |
| 恢复 | 已实测 |
| **最大化** | **已实测**（1024×768）|
| **还原** | **已实测**（回 337×415）|

#11 四项全部走通。
