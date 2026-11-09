## P2D：设置窗口的窗口按钮 —— #11 的最后一处覆盖缺口

2026-09-11-09。第 39 轮。

上一轮我说"已经没有任何可以验证但没验证过的项了"。**这句话说错了。**

#11 我只测了主窗口。设置窗口是**另一个** `mr_decoration_state`，
布局字符串不同，从未测过。这是个真实的覆盖缺口。

### 两个窗口的布局不同

```c
/* 主窗口 */
static struct mr_decoration_state main_decoration =
    {"Mem Reduct", NULL, "appmenu:minimize,maximize,close", 0};

/* 设置窗 */
static struct mr_decoration_state settings_decoration =
    {"Settings - Mem Reduct", "Mem Reduct", "appmenu:minimize,close", 0};
```

注释说明了原因：

> The settings dialog is modal in the original and has no maximize affordance;
> asking for one would surface a visible-but-disabled button (Issue #11).

**设置窗只有 minimize + close，没有 maximize。这是刻意的。**

### 实测

| 操作 | 结果 |
|---|---|
| 点击「最小化」 | `WM_CHANGE_STATE state=3` → unmap，设置窗消失，**主进程存活** |
| 发送 deiconify | `state=1` → map，设置窗恢复可见 |
| 点击「关闭」 | 设置窗 unmap，**主进程存活，主窗口仍可见** |

WM 日志：

```
miniwm: mapped 0x400030
miniwm: WM_CHANGE_STATE 0x400030 state=3
miniwm:   -> unmapped for iconify
miniwm: WM_CHANGE_STATE 0x400030 state=1
miniwm:   -> mapped for deiconify
miniwm: unmapped 0x400030        ← 关闭
```

### 两个值得注意的点

**一、设置窗最小化不影响主窗口**
最小化设置窗时只有 `0x400030` 被 unmap，`0x40000c`（主窗口）一直是 mapped。
说明两个窗口的生命周期是解耦的，没有"关设置窗把应用关了"的问题。

**二、关闭设置窗不杀进程**
`正常可见=0` 但 `主进程存活=yes`、`主窗口可见=1`。
这是正确的——设置窗是子窗口，关它不该退出应用。

如果这点没测，很容易因为"点 X 后窗口没了"就误以为应用退出了。

### 由此确认的 #11 完整覆盖

| 窗口 | 关闭 | 最小化 | 恢复 | 最大化 | 还原 |
|---|---|---|---|---|---|
| 主窗口 | ✅ | ✅ | ✅ | ✅ | ✅ |
| 设置窗口 | ✅ | ✅ | ✅ | **不适用**（刻意无此按钮）| — |

### 关于我上一轮的断言

我上一轮写"没有未验证但能验证的项"，是基于"#11 已完成"的记忆，而没有实际检查
"#11 完成"这个结论覆盖了几个窗口。

**这是同一类错误的第三次**：先把结论写下，再去检查结论的边界。
前两次分别是"原样即保真"和"键盘不可达是缺陷"。

三次都是同一个毛病：**把曾经为真的一次判断，当成永久为真的全称命题。**
