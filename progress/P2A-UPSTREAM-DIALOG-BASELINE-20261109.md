## P2A：上游对话框尺寸基线（取自 tag commit，可回查）

2026-09-11-09。上一轮取到上游 `src/resource.rc`，本轮用它核对尺寸链是否自洽。

### 上游声明（`resource.rc`，commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`）

| 资源 | DLU |
|---|---|
| `IDD_MAIN` | 224 × 214 |
| `IDD_SETTINGS_GENERAL` | 260 × 168 |
| `IDD_SETTINGS_MEMORY` | 260 × 168 |
| `IDD_SETTINGS_APPEARANCE` | 260 × 168 |
| `IDD_SETTINGS_TRAY` | 260 × 168 |
| `IDD_SETTINGS_ADVANCED` | 260 × 168 |

### 本项目的换算（`src/ui_geometry.zm`）

```zig
fn x_dlu_to_logical_px(v) -> (v * 56) // 40     # 1.400
fn y_dlu_to_logical_px(v) -> (v * 12) // 8      # 1.500
```

224 × 1.400 = 313.6 → **313**
214 × 1.500 = 321.0 → **321**

### 为什么 x 与 y 系数不同、且必须不同

DLU 是"基于字体度量单位"，x 基准是平均字符宽（取 8pt 系统字体的 7 px 级别），
y 基准是字符高。两者**不等价**，用同一个系数会把窗口压扁或拉高。
这也正是产品约束里"原版 DLU 需按真实字体/DPI 换算，不能直接当像素"的含义。

### 完整尺寸链核对

```
main_client        = 313 × 321
+ gtk_content_inset (24)              → 宽 313+24 = 337
+ menu_height      (24) + inset(24)   → 高 321+24+24 = 369
```

**结果 337 × 369，与 Xvfb 实测的窗口内容区完全一致**（见
`P2D-CLIENT-AREA-MEASURED-20261109.md`，那里通过像素扫描确认标题栏 46 px、
内容区 369 px）。

三处独立来源互证：

1. 上游 RC 的 DLU 声明
2. 本项目换算公式
3. Xvfb 实际渲染的像素测量

### 设置页

260 × 168 → 宽 260×1.400 = 364，高 168×1.500 = 252。
实测设置窗 558×335（含标题栏 46 与两侧留白）。

### 存档

上游 RC 全文已存 `artifacts/p2d/upstream/resource.rc`（371 行），
后续任何尺寸/控件断言都可以直接对照，不必再联网取。
