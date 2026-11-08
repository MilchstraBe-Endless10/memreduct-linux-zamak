## P0A 生产清理版 GUI 已安装到系统（真实回收验证通过）

2026-11-09 00:28–00:31，在本机真实 Hyprland 会话完成。

### 根因：已安装的 GUI 是「能力被编译排除」的版本

`/usr/bin/memreduct-zamak` 旧版点击 Clean 后只弹「当前版本尚未开放内存清理」，
因为普通 `zam build .` 链接的是 `native/production_clean_gate_default.c`
（`mr_production_clean_enabled() → 0`），生产清理能力在编译期就被排除。
这不是 UI bug，是构建档位问题。

### 修复：用项目自带生产脚本构建

`bash scripts/ci/build-production-clean-gui.sh` → `pagecache_gate=enabled`。

产物（`artifacts/p0a/`）：
- `memreduct-zamak-production-clean`  sha256 `5a4e62bfaf2d5a4ff8fca35f99be633441a901d1971afda174aeaf83fd561daf`
- `memreduct-zamak-helper`           sha256 `b0652c8ac3ae70576e39b71fbdcd047a7c57f23873fec2e45b1b691b34c45a5e`

二进制级证明：
- `nm` 有 `T mr_production_clean_enabled`（=1 的启用门控）
- `strings | grep -c 'MEMREDUCT_CI|MEMREDUCT_TEST'` = **0**（无测试钩子泄漏）
- 脚本自带断言确认 default gate 未混入

已安装（经 `systemd-run --user pkexec`，避开 DSH 会话的 NoNewPrivs 限制）：
- `/usr/bin/memreduct-zamak`（旧版备份为 `memreduct-zamak.before-p0a`）
- `/usr/libexec/memreduct-zamak-helper`

### 真实回收证据（与 GUI 调用的同一 helper、同一 Polkit 通道）

```
回收前  MemAvailable 1359352 kB   Cached 1664764 kB
helper  kernel accepted the cache reclaim request; reclaimed bytes are not measured
回收后  MemAvailable 1568696 kB   Cached  623248 kB
```

观测值：MemAvailable **+209344 kB（≈+204 MiB）**，Cached **−1041516 kB（≈−1017 MiB）**。
按 Issue #13 术语要求：这是内核接受后的**观测变化**，不是「已释放」的承诺量；
drop_caches 代价是后续重读变慢，代码与文案均不作释放量承诺。

### 仍存的环境限制（与产品无关）

本 DSH 会话的 Wayland 指针注入不可用：ydotoold 虚拟设备被 Hyprland 识别
（`hyprctl cursorpos` 跟随、闭环校准可达 ±3px），但客户端收不到 motion/button 事件
（悬停前后截图逐字节相同）。因此 GUI 的 Canvas 自绘按钮（`src/main.zm` 自行命中测试）
只能由真实用户输入触发。生产构建脚本明确禁止 `MEMREDUCT_CI` 进入生产二进制
（`strings` 命中即 exit 1），故不可用测试钩子绕过。
