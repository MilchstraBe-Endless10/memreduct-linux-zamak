# Q16 动作记录导出链：根因与修复

## 结论先行

**不是"没调用"，是调用的时机错了，而且失败被吞掉。**

旧实现把写文件放在 GUI 构造状态字符串的副作用里（`bal_action_record_text`）。
一旦状态没被重建、或被后续赋值覆盖，文件就不存在；而 `export_path()` 对
"没有 HOME"、"mkdir 失败"、"open 失败"、"写短了" 一律返回 `""`，
调用方无法区分。这就是本机 `~/.local/state/memreduct/` 为空且没有任何提示的原因。

## 三个具体缺陷

| # | 缺陷 | 后果 |
|---|---|---|
| 1 | 导出挂在 GUI 文案构造上 | 没重绘 = 没文件；后续 `s.status` 覆盖 = 没文件 |
| 2 | `mkdir` 用 `(void)` 吞掉返回值 | mkdir 失败时只报 open 失败，看不到真因 |
| 3 | 所有失败都返回 `""` | "没写"与"写不进去"无法区分 |

## 第一次修复引入的新缺陷（已被测试拦住）

改成"一进 close_auto 就导出"之后，导出发生在**判定 reason 之前**。
测试立刻暴露：拒绝路径写进文件的是 `reason=not_observed`——文件有了，但内容是废的，
比不写更糟。

正确顺序：**先定 reason，再导出**。因此拆出 `action_resolve_reason()`，
`close_auto()` 现在是：

```c
if (!action.valid) return;
action_resolve_reason();   /* 先 */
action_export_after_close();  /* 后 */
```

## 正式构建里的调用链证据

`objdump -d artifacts/p2b/memreduct-zamak-production --disassemble=mr_bal_bridge_action_close_auto`：

```
bed11: movl $0x1,0x74df5(%rip)        ; reason_observed = 1
bed1b: add  $0x8,%rsp
bed1f: jmp  be7f0 <mr_bal_bridge_action_export_path>    ← 尾调用导出
```

五个 reason 分支（`none` / `no_scope` / `not_run` / `write_skipped` / `policy:*`）
全部汇到 `bed11`，再统一尾调用导出。**这不是只存在于测试里的路径。**

符号确认（正式构建，非 test 构建）：

```
00000000000beda0 T mr_bal_bridge_action_export_error
00000000000be7f0 T mr_bal_bridge_action_export_path
0000000000134640 b action_export_error
```

## 错误可见性

`mr_bal_bridge_action_export_error()` 返回具体原因，例如：

- `no XDG_STATE_HOME and no HOME`
- `mkdir /path: errno 13`
- `open /path/balanced-last.txt: errno 30`
- `write /path: errno 28 after 120 of 300 bytes`

GUI 在 `bal_action_record_text` 里读它，失败时显示
「记录文件未写入：<原因>」而不是静默降级成"只能读文本"。

## EAGAIN 的语义

`EAGAIN` 单列为 `eagain` 类别：内核接受了参数但拒绝回收这么多，
**可能已部分回收**。它既不是零回收，也不是全成功。
`mr_deep_stop_reason` 已有 `MR_DEEP_STOP_PARTIAL = 5` 对应这一状态。
新测试 `test_eagain_is_its_own_class_not_a_failure` 钉住这一点，
防止后续改动把它悄悄归成失败或成功。

## 测试覆盖（不依赖真实清理）

`tests/test_q16_action_record.py` 新增 6 项，全部只驱动 C 接口：

| 测试 | 覆盖 |
|---|---|
| `test_closing_the_record_writes_the_file_without_any_gui` | 成功：无 GUI 也写出 |
| `test_a_refused_click_still_leaves_a_file` | 跳过/拒绝：仍留文件且 reason 正确 |
| `test_export_error_is_empty_on_success` | 成功时无错误 |
| `test_a_failed_mkdir_is_reported_not_swallowed` | 导出失败：报 mkdir + errno |
| `test_a_missing_home_is_named` | 导出失败：报 no HOME |
| `test_eagain_is_its_own_class_not_a_failure` | EAGAIN 归属 |

## 验证

```
python3 -m unittest discover -s tests  ->  426 tests, OK (skipped=4, expected failures=1)
zam check .                            ->  CHECK OK
scripts/ci/build-production-gui.sh     ->  成功
  artifacts/p2b/memreduct-zamak-production
  sha256 82e26e92d39a9435...
```

## 本轮未做的

- 没有为了调试日志而执行真实清理。导出链的正确性由注入测试 + 反汇编证明。
- 未改阈值、未扩范围、未动已安装版、未推送/部署。

## 遗留

GUI 点击的坐标定位在本机不稳定：`ydotool mousemove` 是**相对位移**且实测有偏移
（发 188,91 落到 959,472），导致多次点击落在菜单栏而非 File 菜单项。
这影响的是"我如何触发一次点击"，不影响导出链本身的正确性。
若需再做一次取证清理，应先用 computer-use 插件定位，而不是继续拼 ydotool。
