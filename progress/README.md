# progress/ 索引

这是证据与结论的索引。每条只给一行结论 + 指向原始文档，**原始文档才是证据本体**。

最后更新：2026-11-09（第 29 轮）

---

## 已完成的验证（有实测数据）

| 主题 | 结论 | 文档 |
|---|---|---|
| M0-A 生产版安装 | GUI→Polkit→drop_caches 真实执行一次，+204 MiB / Cached −1017 MiB | `P0A-PRODUCTION-CLEAN-INSTALLED-20261109.md` |
| 偏好持久化（#8）| schema=1 / language / theme 写入并在重启后重载 | `P3A-PREFERENCES-PERSIST-VERIFIED-20261109.md` |
| 自身进程 PAGEOUT | 192 MiB 与 1 GiB 两档实测，覆盖 99.96%，数据未丢 | `P3B-SELF-RECLAIM-VERIFIED-20261109.md` |
| PAGEOUT 代价 | 换出后再访问慢 198~497 倍（0.47 ms → 92~231 ms）| `P3B-REFAULT-COST-MEASURED-20261109.md` |
| 客户区几何 | 337×369，标题栏 46 px 为 GTK 自绘 | `P2D-CLIENT-AREA-MEASURED-20261109.md` |
| 窗口按钮（#11）| **主窗口与设置窗的关闭/最小化/恢复/最大化全部实测**（自建 X11 WM 桩）| `P2D-WINDOW-BUTTONS-CLOSED-20261109.md`、`P2D-SETTINGS-WINDOW-BUTTONS-20261109.md` |
| 跨进程 GUI 门控 | 枚举、选择、Continue 未选时禁用 | `P3C-GUI-PICKER-VERIFIED-20261109.md`、`P3C-CONTINUE-GATED-20261109.md` |
| 跨进程目标就绪 | app-*.scope 候选可被识别，320 MiB 可回收 | `P3C-CROSSPROC-TARGET-READY-20261109.md` |
| 中文 i18n | 跨进程面板漏译已修；设置五页中文通过 | `P3C-ZH-I18N-FIXED-20261109.md`、`P2C-SETTINGS-ALL-PAGES-ZH-20261109.md`、`P2C-SETTINGS-ZH-VERIFIED-20261109.md` |
| 英文 i18n | 主窗口与设置五页英文通过 | `P2C-EN-VERIFY-BILINGUAL-20261109.md` |
| 双语对照审计 | 60 对文案，4 处中文多括号，分两类 | `P2C-BILINGUAL-AUDIT-20261109.md` |
| 上游尺寸基线 | 224×214 / 260×168 DLU，与实测三方互证 | `P2A-UPSTREAM-DIALOG-BASELINE-20261109.md` |
| 菜单树对照 | 四个弹出菜单项数/顺序/分隔全对应 | `P2A-MENU-TREE-PARITY-20261109.md` |

## 发现的缺口（未实现或待决定）

| 缺口 | 性质 | 文档 |
|---|---|---|
| 托盘功能 | **运行时完全不存在**，设置页仍展示其配置 | `P3D-TRAY-GAP-DISCLOSURE-20261109.md` |
| 键盘 Tab 到清理按钮 | **与上游一致**（上游也无 WS_TABSTOP），非缺陷 | `P2D-KEYBOARD-ACCESS-AUDIT-20261109.md` |
| 帮助三项 | 菜单在，点击仅提示"尚未实现" | `P2A-MENU-TREE-PARITY-20261109.md` |

## 阻塞中

| 项 | 阻塞原因 |
|---|---|
| 跨进程真实 `process_madvise(MADV_PAGEOUT)` | 需管理员密码（Polkit `auth_admin`）|


## 待用户裁决（见 Issue #13）

1. 气泡提示文案：A'（两侧补 Linux 不适用）/ B' / C'
2. 清理按钮键盘可达：保持复刻（推荐）/ 作为改进
3. 托盘缺口披露：A（设置页加说明）/ B（README 列出）/ C（不动）

## 上游素材存档

- `artifacts/p2d/upstream/resource.rc` —— 上游 371 行资源定义（commit `2f5c55d1`）
  尺寸、控件样式、菜单树的权威来源，离线可查。

## 已撤回的判断（保留记录以免重犯）

| 曾下的结论 | 何时撤回 | 原因 |
|---|---|---|
| 「zram 满导致第二次换出变慢」 | 第16轮 | 读错指标，zram 实际只占 6.73 GiB |
| 「原样不动即保真」（气泡提示）| 第22轮 | 中文侧已有手写标注，现状非原版 |
| 「清理按钮不可达是缺陷」 | 第25轮 | 上游同样无 WS_TABSTOP |
