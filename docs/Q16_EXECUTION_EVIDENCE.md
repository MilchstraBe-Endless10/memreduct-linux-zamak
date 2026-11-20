# Q16 真实执行取证（2026-11-16 11:54–11:56）

## 一、执行确实发生了

通过 `MEMREDUCT_CI_BALANCED_RUN` 钩子走**与 Run 按钮完全相同的路径**：

```
CI_BALANCED_RUN outcome=executed enrolled=4 batches=1
  requested=17709056 accepted=17709056 stop=no_target record_id=1 writes=2 record_reason=none
```

导出文件 `~/.local/state/memreduct/balanced-last.txt`：

```
MEMREDUCT-BALANCED id=1 scopes=4
  scope[0]=app-org.chromium.Chromium-490522.scope#0
  scope[1]=app-org.chromium.Chromium-9007.scope#2
  scope[2]=app-org.chromium.Chromium-11084.scope#3
  scope[3]=app-com.google.Chrome-729959.scope#4
  target=/sys/.../app-org.chromium.Chromium-9007.scope/memory.reclaim
  writes=2
  w[0]={bytes=14 text="0 swappiness=0" ret=14 errno=0 class=accepted}
  w[1]={bytes=21 text="17709056 swappiness=0" ret=21 errno=0 class=accepted}
  reason=none
```

两次写入逐条可查：先清空预算（`0 swappiness=0`），再提交 17,709,056 字节，
两次都 `errno=0 class=accepted`。**这是完整的写入证据，不是推断。**

## 二、目标 scope 对齐采样（实际写入时刻为 t0）

`app-org.chromium.Chromium-9007.scope`：

| 时刻 | file | inactive_file | anon | workingset_refault_file |
|---|---|---|---|---|
| 写入前 | 73.7 MB | **50.4 MB** | 45.3 MB | 10.83 MB |
| ~1 s | 73.7 MB | 50.4 MB | 45.3 MB | 10.83 MB |
| ~10 s | 62.9 MB | 46.1 MB | 46.3 MB | 10.83 MB |
| ~30 s | 51.1 MB | **44.2 MB** | 46.3 MB | 10.83 MB |

**refault 全程 10.83 MB，纹丝不动（Δ = 0.00）。**

## 三、三问分开答

### ① 是否写对目标 —— **是**

- target 指向 `app-org.chromium.Chromium-9007.scope/memory.reclaim`
- 与导出记录里确认的 4 个 scope 之一对应（索引 #2）
- 没有扩大到整会话，没有碰全机缓存
- 写入内容带 `swappiness=0`，只动文件页

### ② 目标计数如何变化 —— **file/inactive 下降，无快速重读**

- `file` 73.7 → 51.1 MB（−22.6，−31%）
- `inactive_file` 50.4 → 44.2 MB（−6.2）
- **`workingset_refault_file` 10.83 → 10.83，Δ=0.00**

按预设判据（30 秒内 refault 增量 > 0.1 MB 视为快速重读），**不成立**。
请求 16.9 MiB，实际 file 下降 22.6 MB，量级相符。

### ③ 日常收益 —— **未证实**

- 请求量 17.7 MB，不到本机 MemAvailable 自然波动（实测 2329 MiB）的 1%
- 未采集到 PSI 或前台响应的可比变化
- 不构成任何方向的收益证据

## 四、策略行为核对

`enrolled=4` —— 与库探测的 4 个 eligible scope 一致。
`batches=1`、`stop=no_target` —— 第一批后策略自己判定没有更多目标，停止。
这正是设计意图：够用就停，不追求"清到某个数字"。

## 五、附带发现（真实缺陷，未修）

GUI 的 picker 视图文字曾显示 "No application scope is reclaimable right now."，
但同一份 C 代码在库探针和 CI 构建里都得到 `scopes=6`、`can_run=true`。
说明该文案在某条 UI 状态下会误报。已在 `progress/Q16-HANDOFF.md` 记录，
本轮未修（不在"修通记录链"范围内）。

## 六、修复后在真实 GUI 里验证（13:26）

用修复后的生产构建 `527b41b1…`，通过 **真实鼠标点击** File → Balanced clean：

```
MEMREDUCT-BALANCED id=1 scopes=4
  scope[0]=app-org.chromium.Chromium-490522.scope#0
  scope[1]=app-org.chromium.Chromium-9007.scope#2
  scope[2]=app-org.chromium.Chromium-11084.scope#3
  scope[3]=app-com.google.Chrome-729959.scope#4
  target=/sys/.../app-com.google.Chrome-729959.scope/memory.reclaim
  writes=2
  w[0]={bytes=14 text="0 swappiness=0" ret=14 errno=0 class=accepted}
  w[1]={bytes=21 text="23899136 swappiness=0" ret=21 errno=0 class=accepted}
  reason=none
```

对比修复前：同样的点击只能得到 `writes=0 reason=policy:no_trigger` 或干脆没有 picker。
现在 picker 正确列出 4 个 scope，并且**真的执行了 23.9 MB 的文件页回收**。

## 七、修复内容

`src/main.zm` 两处打开 picker 的地方，原本：

```zamak
s.bal_view = 1
let count = mr_bal_bridge_scan_scopes()   # 结果只进局部变量
```

`s.bal_scope_count` 保持初始值 0，而 `draw_balanced_picker` 正是用它决定是否显示
"No application scope is reclaimable right now."。

改为调用 `bal_rescan(s)`（该函数的职责就是写这个字段），入队循环读 `s.bal_scope_count`。

## 八、回归测试

`tests/test_q16_picker_scope_count.py`（4 项）：每处 `s.bal_view = 1` 附近必须有
`bal_rescan`、picker 块内不得扫描到局部、`bal_rescan` 必须写该字段、空态文案必须挂在它后面。
源码级断言，因为缺陷是"漏了一个赋值"，任何运行时输入都救不了旧代码。
