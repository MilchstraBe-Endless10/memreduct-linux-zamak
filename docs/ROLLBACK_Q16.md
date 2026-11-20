# 回退说明 — Q16 均衡清理候选

## 结论先说

**本候选不需要改动 helper，也不需要改 Polkit 规则。**

理由：均衡清理走的是 cgroup v2 的 `memory.reclaim` 接口，写的是调用者自己 user slice 下的
应用 scope。它不需要 root，不需要 pkexec，也不经过任何特权 helper。
候选产物里 `mr_production_clean_enabled` 与 `mr_xproc_capability_enabled` 都是
`xor %eax,%eax; ret`（恒 0），也就是说需要特权的那两条路在这个构建里根本不可达。

**所以安装本候选不涉及任何系统级配置变更，回退就是换回一个二进制文件。**

补充一条与回退无关但必须知道的事实：菜单里的**深度清理**在这份候选里
S1 不可执行（被同一个页缓存 gate 挡死），S2/S3 作用于整个 user session 而非单个应用，
S4 的写入不带 `swappiness=0`、可能换出匿名页。本 PR 不为这条管线背书。
回退不需要考虑它，因为它没有被本 PR 启用或验收。

## 回退前先核对

```bash
# 1) 当前装的是哪个版本
sha256sum /usr/bin/memreduct-zamak
# 本机记录：235e0471d7dffeadf29119cc1bf7d9ebc5579cab212f06b2249602469a071264

# 2) 要装的是哪个版本
sha256sum <候选路径>
# 本候选：373c8a271d5c728abfb960d5d858b7f002338e8a712923cb9e615d9d81ef675d

# 3) 两者必须不同；相同说明你没换成东西
```

## 备份

```bash
# 只备份这一个文件，不需要备份别处
sudo cp -a /usr/bin/memreduct-zamak /usr/bin/memreduct-zamak.pre-q16
sha256sum /usr/bin/memreduct-zamak.pre-q16     # 应等于上面的 235e0471…
```

建议同时记下来：

```bash
ls -l /usr/bin/memreduct-zamak > /root/memreduct-q16-backup.txt
sha256sum /usr/bin/memreduct-zamak >> /root/memreduct-q16-backup.txt
```

## 安装（如果决定安装）

```bash
sudo install -m 0755 <候选路径> /usr/bin/memreduct-zamak
sha256sum /usr/bin/memreduct-zamak              # 应等于 373c8a27…
```

## 回退

```bash
# 1) 先确认备份还在且哈希正确
sha256sum /usr/bin/memreduct-zamak.pre-q16      # 必须是 235e0471…

# 2) 换回去
sudo install -m 0755 /usr/bin/memreduct-zamak.pre-q16 /usr/bin/memreduct-zamak

# 3) 核对
sha256sum /usr/bin/memreduct-zamak              # 必须等于 235e0471…
```

如果备份文件的哈希对不上，**不要继续**——先找回原始包重装，不要拿可疑的备份覆盖。

## 回退了会丢什么

- 按应用均衡清理（菜单 File → Balanced clean）不可用，因为旧版没有这条路径。
- 执行记录文件 `~/.local/state/memreduct/balanced-last.txt` 不再更新。
  旧文件留着无害，可自行删除：`rm -rf ~/.local/state/memreduct`。
- 设置项：候选新增了 preferences store，回退后这些设置不再被读取（不回滚、不损坏）。

## 不会因为回退而损坏的东西

- 本候选没有迁移用户数据，没有写系统配置，没有装服务，没有改 Polkit。
- 没有创建需要清理的临时状态。

## 这个候选明确没做的事

- 没有替换 `/usr/bin/memreduct-zamak`（本机至今仍是 235e0471…）
- 没有推送、没有合并、没有发版
- 没有改 helper，没有改 Polkit
- 没有改任何阈值、预算或自动策略
