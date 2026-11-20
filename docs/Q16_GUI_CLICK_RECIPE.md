# 在本机对 Mem Reduct GUI 完成一次真实点击的可靠做法

## 之前反复失败的真因

**窗口不在当前工作区。** 候选二进制启动后落在 workspace 4，而 activeworkspace 是 3。
点击坐标 (815,403) 在屏幕上是有效的，但那个位置在当前工作区里没有窗口，
于是点击被送到别处（或只让窗口获得焦点），菜单永远不弹。

这不是 ydotool 坐标不准，也不是 GTK 菜单需要特殊手势。我先后怀疑过：
相对位移漂移、按压时长、F10/Alt+F 快捷键、popup surface 不在 grim 裁剪区——
全部是误判，浪费了很多轮。

## 可靠步骤

```bash
SIG=$(ls -t /run/user/1000/hypr/ | head -1)
export HYPRLAND_INSTANCE_SIGNATURE=$SIG
export YDOTOOL_SOCKET=/run/user/1000/.ydotool_socket
export XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-1

nohup <binary> > /tmp/g.log 2>&1 &
sleep 7

# 1) 查窗口落在哪个工作区
hyprctl clients -j | python3 -c "
import json,sys
for c in json.load(sys.stdin):
    if 'memreduct' in c.get('class','').lower():
        print(c['address'], c.get('workspace',{}).get('id'), c['at'], c['size'])
"
# 2) 不在当前工作区就移过去
hyprctl dispatch "hl.dsp.window.move({ workspace = <当前ws>, window = \"address:0x...\" })"

# 3) 迭代逼近目标坐标（ydotool mousemove 是相对位移）
#    先移到窗口中部（建立 motion 链），再移到菜单项
mv 960 550; sleep 0.4
mv 815 403; sleep 0.6            # File 菜单

# 4) 按下与释放分开发（0x40=仅按下，0x80=仅释放）
ydotool click 0x40; sleep 0.2
ydotool click 0x80; sleep 1.5
```

## 坐标（窗口位于 at=(792,346)、size=337x416 时）

| 目标 | 屏幕坐标 |
|---|---|
| File 菜单 | (815, 403) |
| Balanced clean 行 | (880, 448) |
| Deep clean 行 | (880, 463) |
| 底部 Clean memory 按钮 | (960, 751) |

## 验证成功的判据

```bash
cat ~/.local/state/memreduct/balanced-last.txt
```

有这一行就是真点到了：

```
MEMREDUCT-BALANCED id=1 scopes=4 scope[0]=... target=.../memory.reclaim writes=0 reason=policy:no_trigger
```

## 教训

**动手前先查 activeworkspace。** 这条在项目记忆里早有记录
（「Windows VM 占据的 workspace 会吞点击」），我没照做，直接从坐标开始调试，
把四个不存在的假设全试了一遍。
