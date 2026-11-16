# P3C 第二轮集成报告 — Issue #11 + #12 审查意见落实

基线 `19fc60b6ca4d4268b2cf65fe8197b4672c6cccb6`，两条线各自独立完成后合入，无冲突。

| 用途 | 分支 | SHA |
|---|---|---|
| 窗口线 | `p3c/issue11-window-real` | `5beb2e9f1a51337b6cf2b4a6a85b00d118beaf24` |
| 内存线 | `p3c/issue12-crossproc` | `74ab8248818e0d786e00db523103cabb9e57730b` |

## #11 窗口：+52px 根因与三按钮

审查指出「+52 是边框常数」未经证明。用与该产品完全同构的独立 GTK3 程序做单变量实验：

| 调用 | 结果 |
|---|---|
| `gtk_window_resize()` only | 325x357（被忽略） |
| `gtk_window_set_default_size()` only | 389x421（**+52，双向**） |
| 两者都用 | 389x421 |
| min==max hints + resize | 精确命中 |

每层 `gtk_widget_get_preferred_width()` 都是 325，HeaderBar 只要 172..211。
所以 +52 既不是边框常数、也不是 canvas 最小尺寸，而是 `set_default_size()` ——
GTK3 把它当作**总尺寸**再叠加 CSD。

修复（`native/gui_window.c`）：
1. 不再调用 `set_default_size()`
2. 在窗口 map **之前**安装 min==max geometry hints
   （从 idle 里装太晚，实测会被 tiler 接管成 941x1030 且再也回不来）
3. hint 高度 = 内容高 + header bar 高
   （只按内容高钉住会得到 337x322，标题栏从内容里吃掉 47）
4. idle 只观测不再重设（重设会与合成器打架）

因为窗口恢复 resizable，最大化按钮回到装饰布局
（`appmenu:minimize,maximize,close`，进程级，不改 gsettings）。

实机 Hyprland 0.56.2 单实例验证：
```
CI_CONTENT_SIZE_APPLIED content=337x369 expected=337x369 reached=337x369 delta=+0x+0 resizable=1
CI_WINDOW_SIZE_POLICY main_found=1 main_resizable=1 settings_found=1 settings_resizable=0
初始:    hyprctl [337,416] floating=true
最大化:  hyprctl [1916,1050] fullscreen=1
恢复:    hyprctl [337,416] fullscreen=0 floating=true
```
337x416 = 337x369 客户区 + 47px 标题栏。三按钮（−/□/×）在截图中齐全且非灰。

## #12 内存：可执行行为复核

审查要求「静态 grep 不够，要可执行二进制级证据」。产品 ELF：
```
nm      | grep -c mr_crossproc            = 20   (符号已编入)
objdump | grep -c '\$0x1b8,'              =  0   (process_madvise syscall 440 立即数为 0)
strings | grep -c 'cap_sys_nice|drop_caches' =  0
```
`pkexec` 字符串来自既有 pagecache helper（`async_privileged.c`），crossproc 中 0 处。

修掉的两个生产缺陷（都是"失败看不见"）：
1. `advise_target()` 用 `(void)` 丢弃最后一批的返回值 —— 前面的批次成功后
   flush 失败，状态仍是 OK，调用方被告知 advice 已发生
2. `classify_errno()` 只在 error_code 为 0 时写，且不覆盖此前的 OK ——
   成功之后的失败会描述一次并未发生的操作

审查点中确实缺失的一项：路径截断静默失败。新增 `proc_path()`，装不下就拒绝并上报，
调用方一律按硬失败处理，绝不当成「进程已退出」。

新增回归（覆盖审查点名项）：
- 路径 63/64/65/111 字节全部可解析（旧的 `char path[64]` 从 64 起就坏）
- 超长路径 → 无目标且状态非 OK
- `comm` 含空格、含 `)`、空串均可解析；无 `(comm)` 的行不可解析，绝不成目标
- EPERM/EACCES/ESRCH/EINVAL/ENOSYS/EFAULT/EIO 各自映射到独立状态（每个用例独占进程）
- 真正的短返回 = PARTIAL；0 返回 ≠ 成功

## 集成验证

```
python3 -m unittest discover -s tests  -> Ran 107 tests ... OK (skipped=1)
bash -n scripts/*.sh scripts/ci/*.sh   -> clean
zam check .                            -> CHECK OK
zam test .                             -> 76 passed; 0 failed
zam build .                            -> memreduct_zamak-110edd08...
```

skip 的那一条是文件系统拒绝 4096+ 字节路径，属环境限制而非放过。

## 仍然 NOT RUN / 未关闭

- **三按钮的真实鼠标点击**：未在用户活动会话注入合成输入。截图证明按钮存在且可用，
  最大化/恢复经 Hyprland dispatcher 验证；点击本身属用户自证。
- **真实跨进程 advice**：`OWNER_APPROVAL_REQUIRED`，本轮未对任何其他进程发出
  `process_madvise`，未安装 root helper，未 setcap，未改 Swap/ZRAM。
- 设置页 558×335 未在实机重新确认尺寸（本轮改动只针对主窗口的契约路径）。
