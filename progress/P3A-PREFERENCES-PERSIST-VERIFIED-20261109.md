## P3A 偏好持久化：端到端验证通过（#8）

2026-11-09 在私有 Xvfb（XTEST 真实指针/键盘事件）中驱动**已安装的生产版 GUI**
（`/usr/bin/memreduct-zamak`，sha256 `5a4e62bf…`）完成验证。

### 单元测试

`python3 -m unittest tests.test_preferences_store -v` → **26 tests OK**。
覆盖：XDG 绝对路径优先、相对/缺失/带 `..` 的路径回退与拒绝、符号链接根目标校验、
临时残留忽略、ENOSYS 时 openat 回退、两进程 flock 合并、未知未来 schema 不被降级、
写入/fsync 失败保留旧文件并清理自建临时件、生产库不含测试注入符号。

### 端到端（真实 GUI 点击 → 落盘 → 重启读回）

1. 首次启动，`$XDG_CONFIG_HOME/memreduct-zamak/preferences.conf` **不存在**（无配置时不写盘，符合预期）。
2. 通过 View → Language 菜单真实点击「简体中文」（target 602 → `persist_language(s,1)` → `mr_preferences_set_language`）。
3. 关闭进程后配置文件真实存在：
   ```
   schema=1
   language=zh-CN
   theme=light
   ```
4. 再次启动后该文件仍为同一内容，且界面按恢复状态渲染。

证据截图归档于 `artifacts/p3a/gui-persist/`（before / menu / after / restored）。

### 说明

本机 Xvfb 缺少 `openbox`/`xwininfo`/`xrdb`，仓库自带的
`scripts/ci/p3a-preferences-xvfb.sh` 依赖这些工具因而未直接运行；
上述验证用等价的最小 Xvfb + xdotool 序列完成，断言等价于测试意图。
补装这些包后可原样运行官方脚本做 CI 对齐。
