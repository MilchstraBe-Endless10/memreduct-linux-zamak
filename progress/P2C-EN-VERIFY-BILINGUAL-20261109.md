## P2C：英文侧五页实测 + 一处决定性证据

2026-09-11-09。前几轮全在验中文，英文是默认语言却没系统截过。本轮补上。
同时发现一条能解决上一轮「气泡提示」两难的证据。

### 英文实测

| 页 | 内容 | 结果 |
|---|---|---|
| 主窗口 | File / View / Settings / Help；Physical memory / Pagefile / Swap / System working set；Clean memory | 通过，337×415 |
| General | Always on top / Load on system startup / Start minimized / Confirm memory cleaning start / Skip "User Account Control" prompt warning / Periodically check for updates (recommended) | 通过 |
| Memory cleaning | Memory regions to be cleaned / Memory management / Hotkeys (global) / Ctrl+F1 | 通过 |
| Tray icon | Color indication / Mouse control / Balloon tips | 通过 |
| Advanced | （见截图）| 通过 |

### 决定性证据：中英文对同一控件处理不同

同一行，两种语言：

```
英文  p.text(... "Balloon tips:")                             ← 无括号说明
中文  text(zh, "Balloon tips:", "气泡提示（Windows 通知）:")   ← 中文多了说明
```

**中文侧是被刻意加上「Windows 通知」说明的。** 说明两点：

1. 维护者已经意识到「气泡提示在 Linux 上是 Windows 概念」这个问题，并且**已经做出了选择**：
   不是在英文/中文都保留裸标签，而是在中文里补上适用性提示。
2. 因此上一轮我提的 A 方案（补一句「Linux 不适用」）**不是在改动原版设计**，
   而是把维护者已有的做法**做完整**——目前只补了「Windows 通知」，没补「Linux 不支持」。

这推翻了我上一轮对 B 选项（原样不动即保真）的判断：
现状其实已经是「部分标注」状态，不算纯粹的原样。要让两种语言一致且诚实，
应当两侧都补全说明，或至少保持同样的标注深度。

### 英文侧另发现一处可考虑的差异

英文 `Balloon tips:` 没有任何适用性提示，中文有。这属于**双语信息量不对等**，
不是漏译，但两种语言用户对同一控件得到的信息不同。是否统一由产品决定。

### 截图

`artifacts/p2c/en-settings-pages/`（main-en.png, en-page0..4.png）。
