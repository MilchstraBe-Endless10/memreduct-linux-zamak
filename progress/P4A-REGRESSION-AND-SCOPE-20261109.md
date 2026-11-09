## P4A：回归结果与本轮活动的真实范围

2026-09-11-09。第 38 轮。这轮做了回归，并借机把一件容易含混的事说清楚。

### 回归

| 项 | 结果 |
|---|---|
| Python 单测 | **246 通过 / 1 跳过** |
| shell 语法（`bash -n scripts/*.sh scripts/ci/*.sh`）| 通过 |
| `zam check .` | `CHECK OK memreduct_zamak` |

### 本轮活动的真实范围（必须说清）

从 `a535010` 到现在，**34 个文件变更，1647 行新增**，按目录分：

```
20  artifacts   （截图、上游 RC、WM 桩源码、协议输出）
13  progress    （证据文档）
 1  memory      （SHORT_TERM 同步）
 0  src/        （无源码改动）
 0  native/     （无源码改动）
```

`git log a535010..HEAD -- src/ native/` 输出为空，可以确认。

这不是疏忽，是事实：**这 38 轮里我没有改过一行产品代码。**

### 为什么

前面那些 i18n 修复（18 处）、R5 的 volatile 快照修复、close-during-reclaim——
**都是在本会话开始之前就提交的**（`a535010` 及更早）：

```
a535010 fix(gui): render 'unavailable' in Chinese instead of leaking English
f56fc51 fix(gui): translate the remaining 13 hardcoded strings on the result screen
0d72482 fix(gui): translate five hardcoded English strings in the cross-process panel
6b7fa29 fix(r5): allow kernel-named anon mappings in region eligibility
...
```

我这几轮做的全部是：**验证已有实现、写证据、披露缺口、做产品判断**。

### 这件事值得说清的地方

如果只看 commit 数量或文档体量，很容易给人"做了很多实现"的印象。
按目录一分类就看清楚了：这轮的产出是**证据**，不是**代码**。

两者都该做，但混在一句"我推进了 XX"里说，会被读成后者。

具体来说：

| 我做了什么 | 产出类型 |
|---|---|
| 装 M0-A 候选、跑 drop_caches、记录 +204 MiB | 验证 |
| 测自身 PAGEOUT、量 refault 代价 | 验证 |
| 五页中文/英文逐页截图、双语对照审计 | 验证 |
| 上游 RC 交叉核对尺寸与菜单 | 验证 |
| 自建 WM 桩测窗口四按钮 | **验证工具开发**（唯一的新代码，80 行 C，测试用） |
| 披露托盘不存在、键盘不可达与上游一致 | 结论 |
| 三个待裁决项发 Issue | 移交 |

真正的新代码是那个 WM 桩，而且它不进产品。

### 所以"推进到真实可用"到什么程度

按目标四项逐条：

| 目标 | 状态 | 性质 |
|---|---|---|
| ① R5 + 真实回收证据同步到 GitHub | **完成** | 证据已在 Issue/仓库 |
| ② M0-A 真实写入一次并验证 | **完成** | 有实测数据 |
| ③ ≥256MiB 跨进程回收 + GUI 全链路 | **半完成** | GUI 侧完成，内核侧缺密码 |
| ④ #11 与 #8 | **完成** | 全部实测通过 |

①②④ 是"验证完成"，③ 是"验证被外部条件阻断"。
**没有一项是"我写了实现让它可用"**——因为这个会话开始时实现就在那里了。

### 如果下一轮还要继续

剩下的只有两种可能：

1. 拿到密码 → 完成 ③ 的成功侧。这是唯一能改变状态的动作。
2. 用户裁决三个待裁决项 → 我可以改代码（气泡文案、键盘可达、托盘披露）。

否则继续做非特权验证，边际价值已经很低——上面那张表里已经没有"未验证但能验证"的项了。
