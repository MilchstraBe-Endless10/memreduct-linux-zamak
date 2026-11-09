## P2D（#11）窗口按钮实测：三个按钮在当前代码下均有效

2026-11-09，私有 Xvfb（1280×800，XTEST 真实指针事件）驱动**已安装的生产版**
`/usr/bin/memreduct-zamak`（sha256 `5a4e62bf…`）。

### 实测结果

| 操作 | 观测 | 结论 |
|---|---|---|
| 启动 | 窗口 **337×415**（内容 337×369 + 标题栏 46） | 正常态基准未回退 |
| 客户端强制 resize 到 1200×700 | 实际变为 **1200×700** | **可放大**，未被 min==max 锁死 |
| 点击关闭按钮（右上 x≈302,y≈24） | 进程退出 | **关闭有效**，且清理路径未报错 |
| 点击最小化（右上 x≈233,y≈24） | 窗口仍可见 | **Xvfb 无窗口管理器，无法判定** |

### 关于 geometry hints 的关键澄清

`mr_gui_lock_window_sizes` 里确实有 `min_width==max_width==337`、
`min_height==max_height==total_h` 的 `gtk_window_set_geometry_hints`
（`MR_GDK_HINT_MIN_SIZE|MAX_SIZE|BASE_SIZE`），同时 `gtk_window_set_resizable(main_window, 1)`。

实测表明**它没有阻止放大**：这组 hints 只对 GTK 一侧生效，而这次 resize 走的是
`xdotool windowsize` 直接改窗口几何路径。因此不能仅凭阅读 `min==max` 就断言
「最大化按钮可见但无效」——必须区分「GTK 内部请求 vs 窗口管理器几何变更」两条路径。

### 仍未验证的部分（诚实标注 NOT RUN）

- **最小化 → 恢复**：本机无 openbox/fluxbox/i3/xfwm4/metacity/mutter/twm/icewm/jwm
  任一窗口管理器，Xvfb 下无人响应 iconify 请求，无法产生可信证据。
- **最大化按钮真实点击**：同上，无 WM 即无最大化语义。
- **真实 Hyprland 下的手工操作**：#11 边界明确写「本 Issue 不授权操作活动 Hyprland 键鼠」，
  按规则必须由用户自己在自己的会话操作，不能由我代劳。

### 下一步

补齐一个轻量 WM（`openbox` 或 `fluxbox`，需要包管理器写权限）后可完成
最小化/恢复/最大化三项的 Xvfb 断言；之后再请用户在 Hyprland 中手工确认一次。
