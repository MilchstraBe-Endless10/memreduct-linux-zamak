## P2A：主菜单树与上游逐项对照

2026-09-11-09。上一轮拿到上游 `IDM_MAIN` 定义，本轮把四个弹出菜单逐项比对。

### 上游 `IDM_MAIN`（resource.rc:24-56）

| # | 菜单 | 项 |
|---|---|---|
| 1 | 第一个 | `IDM_SETTINGS` / SEPARATOR / `IDM_EXIT` |
| 2 | 第二个 | `IDM_ALWAYSONTOP_CHK` / `IDM_USEDARKTHEME` / `IDM_REDUCTCONFIRMATION_CHK` / SEPARATOR / 子菜单 `IDX_LANGUAGE` |
| 3 | 第三个 | `IDM_LOADONSTARTUP_CHK` / `IDM_STARTMINIMIZED_CHK` / `IDM_SKIPUACWARNING_CHK` / `IDM_CHECKUPDATES_CHK` |
| 4 | 第四个 | `IDM_WEBSITE` / `IDM_CHECKUPDATES` / `IDM_ABOUT` |

### 本项目对照（`root_menu_target`，main.zm:319-334）

| # | 行号映射 | 项数 | 与上游 |
|---|---|---|---|
| 1 | row0→101, row2→102 | 2 项（中间是分隔线）| 一致 |
| 2 | row0→201, row1→202, row2→203, row4→204 | 4 项（row3 分隔线）| 一致，含语言子菜单 |
| 3 | row0→301, row1→302, row2→303, row3→304 | 4 项 | 一致 |
| 4 | row0→401, row1→402, row2→403 | 3 项 | 一致 |

四个菜单的项数、顺序、分隔线位置全部对应。**这一层是忠实复刻。**

### 实际行为（第二轮核对）

「帮助」三项的标签正确：

```zig
draw_menu_item(..., 401.0, label(s.zh, "Website", "项目网站"), ...)
draw_menu_item(..., 402.0, label(s.zh, "Check for updates", "检查更新"), ...)
draw_menu_item(..., 403.0, label(s.zh, "About", "关于"), ...)
```

但三项的处理是**同一个分支**：

```zig
elif target == 401.0 or target == 402.0 or target == 403.0:
    s.status = label(s.zh, "This help action is not implemented", "帮助操作尚未实现")
    s.popup = 0
```

即：菜单结构保真，但功能尚未实现，且**明确告知用户未实现**。

### 判定

| 维度 | 结论 |
|---|---|
| 菜单树的形状（项数/顺序/分隔）| 与上游一致 |
| 菜单标签文案 | 与上游一致（英/中）|
| 三项的实际动作 | **未实现，且状态栏明说「尚未实现」** |

这不是缺陷，是**已知未实现**。符合项目规则「不支持时明确说明，不得伪造」。
若日后要实现，应作为独立任务（打开 URL / 检查更新 / 关于对话框），
且需要新的网络与版本检查能力，属于架构变更。

### 未验证

- `IDM_CHECKUPDATES`（菜单项 3 的第四项）与「检查更新」（Help 的第二项）
  在上游是两个不同入口，我们目前都指向未实现。
- Website / About 的真实内容无从取得，不做猜测。
