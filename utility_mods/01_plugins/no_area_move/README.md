# no_area_move — 只拦"普通走到边缘"，保留蹲下 / 攀爬 / 跳下 / 切道具换图

MHGU 的地图按区域（a00/a01/...）分区，走到区域边界的"区域移动点"会淡出并加载下一区。
本插件 hook 每帧检测这个触发点的函数 `sStage::checkAreaMove`（main 偏移 `0x50005C`），
**默认返回 0（不换图）**，只有检测到下面几种状态时才调用原函数放行：

| 方式 | 判定 | 配置 |
| --- | --- | --- |
| 蹲下 / 蹲走 | `act_no = 23 / 24 / 25`（另含轻重弩蹲下相关 27/28/31/32/35/37） | `allow_crouch` + `crouch_acts` |
| 攀爬 | `act_no = 810 上爬 / 880 下爬 / 1240 攀爬跳跃` | **固定开启，不配置** |
| 跳下 | `act_type = 4` | **固定开启，不配置** |
| 切道具 | 长按 L（道具轮盘）；点按 L 可配；**ZL 相关默认全关** | `item_by_*` 系列开关 |

普通走/跑（`1/0`、`1/7`、`2/x`）仍然被拦；任务开始/结束、菜单返回营地等流程不走这个函数，不受影响。

## 配置

存档目录的 `no_area_move.ini`（首次运行自动生成）。字段缺省时用默认值，改完重启游戏生效：

```ini
[]
enabled = true
log_act = false

allow_crouch = true
allow_item = true

crouch_acts = 23,24,25,27,28,31,32,35,37
crouch_walk_types = 2
crouch_walk_acts = 7

item_by_tap = false
item_by_hold = true
item_by_tap_zl = false
item_by_hold_zl = false
item_hold_frames = 18

item_acts =
item_types =
```

| 键 | 含义 |
| --- | --- |
| `enabled` | 总开关，false = 完全恢复原版 |
| `log_act` | true 时动作变化写 `forge-loader.log`（排障） |
| `allow_crouch` | 蹲下/蹲走放行 |
| `allow_item` | 切道具放行 |
| `crouch_acts` | 蹲下站定的 act_no 列表 |
| `crouch_walk_types` | 蹲着轻推摇杆（慢走）的 act_type（实测 2） |
| `crouch_walk_acts` | 蹲着轻推摇杆的 act_no（实测 7） |
| `item_by_tap` | 点按一下 L 就放行（默认关） |
| `item_by_hold` | 长按 L（道具轮盘）放行 |
| `item_by_tap_zl` | 点按一下 ZL 就放行（默认关） |
| `item_by_hold_zl` | 长按 ZL（另一种键位设置）放行（默认关） |
| `item_hold_frames` | 长按判定帧数，默认 18（60fps ≈ 0.3 秒） |
| `item_acts` | 可选：按玩家 act_no 放行切道具 |
| `item_types` | 可选：按玩家 act_type 放行切道具 |

- 蹲走必须用"类型+动作号"匹配（`2/7`）；只看 act 号会把普通走 `1/7` 也放行。
- 蹲下满推会变成 `2/6`（和普通跑同号）：插件在蹲下/蹲走时进入"蹲模式"，
  满推跑 `2/6` 期间保持该模式，直到停下（`1/0`）或摇杆回中才退出。
  所以"蹲下后满推"能换图，而平时直接跑不能。
- `item_hold_frames` 按调用帧计数；手感不对就加减这个数（越小越容易触发）。
- 攀爬、跳下是硬编码始终开启，ini 里不再出现这两项。
- 实际下发的 ini 每一行都有中文注释。

## 依据（GU v1.4.0）

- `0x50005C` 由任务场景任务 `sub_6069F9FC` 每帧调用，参数是 sStage（`dword_618977A0`）。
- 它用玩家坐标（`player+0x40`）和当前区域（`player+4180`）遍历
  `sStage+904+4*area` 里的触发点（pos / height / radius）。
- 踩中后：`area+74 = 下一区域`、`area+81 = 出现点`、`sStage+420 |= 2`；
  调用方看到 bit2 就把任务切到过渡状态，开始淡出/加载。
- 函数开头有门控 `!sub_603A5ED0(sQuest,5,1) && sQuest+85 <= 0xC`。
  任务完成进入结算流程后 sQuest 阶段会走到 9，门控直接返回 0 ——
  这就是"结算等待时走到边缘不传送"的原因。
- 该函数只有 `sub_6069F9FC` 一个调用者，且调用方不看返回值。

## 玩家状态字段

| 字段 | 偏移 | 说明 |
| --- | --- | --- |
| act_type | `player+0x2506`（u16） | 动作类型 |
| act_no | `player+0x250A`（u16） | 动作号 |

## 已知限制

- 只拦"普通走到边缘"；任务开始/结束、菜单返回营地等流程照常。
- 跳下用 `act_type == 4` 判定，type 4 里可能还包含少数空中/翻滚动作；
  目前固定开启，如果发现误换图需要调整，只能改插件。
- 切道具按输入判定；不想要 L 或 ZL 任何一项就把对应的 `item_by_*` 改成 false。
- 偏移按 GU v1.4.0（`0100770008DD8000`）写死；换版本会因函数头校验失败自动跳过。
