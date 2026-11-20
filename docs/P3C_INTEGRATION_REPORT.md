# P3C 集成报告 — Issue #11 窗口线 + Issue #12 内存线

分支 `p3c/integration`，基线提交 `0107bc1`（feat/issue-10-p3b-clean-closure HEAD），
两条线各自在隔离 worktree 完成后再合入，无冲突。

## 合并结果

```
1eb63f0 Merge branch 'p3c/issue11-window-real' into p3c/integration
e5c720b fix(issue11): real header-bar buttons; window size still not honoured
72bee2e wip(issue11): header bar and content-size contract land, window size policy still open
65f86e7 fix(crossproc): resolve the proc paths, then add the missing regressions
56a37ed fix(crossproc): repair the three fixture bugs that emptied every plan
```

- 窗口线：`p3c/issue11-window-real` @ `e5c720b`
- 内存线：`p3c/issue12-crossproc` @ `65f86e7`
- 集成：`p3c/integration` @ `1eb63f0`
- 只改了 `native/gui_window.c`、新增 `native/crossproc_*` 与 `tests/test_crossproc.py`、
  `scripts/ci/p2b-production-*.sh` 的断言、`zam.toml` 的 sources 列表。
- `src/main.zm` 未被任何一线修改，无需 Integrator 手工合入。

## 集成后验证（真实输出）

```
python3 -m unittest discover -s tests  -> Ran 99 tests ... OK
bash -n scripts/*.sh scripts/ci/*.sh   -> clean
zam check .                            -> CHECK OK memreduct_zamak
zam test .                             -> test result: ok. 76 passed; 0 failed
zam build .                            -> build/memreduct_zamak-6784c8a3...
```

真机 Hyprland 0.56.2 上运行集成产物：

```
CI_TITLEBAR attached=true title=Mem Reduct layout=appmenu:minimize,close
CI_MAIN_CONTENT_INSET old=12 new=0 margins=start:0,end:0,top:0,bottom:0 verified=true
CI_WINDOW_SIZE_POLICY main_found=1 main_resizable=0 settings_found=1 settings_resizable=0
CI_CONTENT_SIZE_APPLIED tag=main requested=337x369 reached=389x422 overshoot=+52x+53
hyprctl: size=[389,469] floating=true
```

## 仍然未闭合

1. 窗口尺寸：横向多出 52px。用独立 GTK3 程序（与产品完全相同的控件嵌套）标定：
   请求 337×369 得到 389×421，且 canvas 被分配到 389×357——即这 52px 加在**内容**上，
   不是包在外面的边框。三种窗口侧手段（lift resizable lock、pin holder、pin root）
   与 geometry hints 都无法改变它。所以这不是 C 侧能修的，需要在 Zamak 侧
   （`src/main.zm` / `ui_geometry`）处理，本轮未动。
   纵向 357 是正确的（canvas 高度未变）。
   注意：之前报告里写的「honoured=0」用 `>=` 做判定——任何不比请求小的窗口都会通过，
   那是个恒真检查，已删除；现在只打印请求值、实测值与带符号的超出量。
2. 三按钮的真实点击（最小化→恢复、关闭→退出并清理）未在用户实时会话中点击验证；
   没有向该会话注入合成输入。按钮存在且非灰已由截图与 CI 标记证实。
3. 最大化已从装饰布局中移除（`appmenu:minimize,close`）。固定尺寸窗口在平铺
   compositor 上无法最大化，保留它只会得到一个可见但无效的按钮，Issue #11 明确禁止。

## 安全边界（内存线）

- `process_madvise` 可写调用仍是编译期 + 运行时双重禁用：
  未定义 `MEMREDUCT_ENABLE_PRODUCTION_ADVISE` 时 `mr_crossproc_advise()` 返回 DISABLED。
- 静态取证：`native/crossproc_*` 中无 `setcap`、`pkexec`、`drop_caches`、
  `cap_sys_nice`、`sudo`、`chown root`、`/etc/systemd`；无 `kill(`、`ptrace(`、`system(`。
- 生产真实跨进程动作仍为 `OWNER_APPROVAL_REQUIRED`。
