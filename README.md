# MHGU 太刀 · 世界风格

## 世界风格太刀仿崛起增强版 / Rise-Inspired Enhanced World Style

当前版本包含《崛起》式大居合派生、原版 330 号快速收刀、看破接无双、狩技收招接特殊纳刀，以及只扣血的气刃突刺。

The current release adds Rise-inspired Iai follow-ups, sheathe motion 330, Foresight into Spirit Reckoning, Special Sheathe after Hunter Arts, and HP-only Spirit Thrust.

下载独立安装包，把 `romfs` 解压到 `mods/MHGU_WORLD_STYLE/`，然后在装备箱选择「世界风格」。已在 Windows Recomp 0.18.1 和 Forge 2.0.0 离线副本测试。手机版、模拟器、主机和联机未测。

Download the standalone package, extract `romfs` to `mods/MHGU_WORLD_STYLE/`, then select World Style at the equipment box. Tested offline with Windows Recomp 0.18.1 and Forge 2.0.0. Mobile Recomp, emulators, console and multiplayer are untested.

[下载 / Download](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/download/v1.2-Rise-Enhanced-Recomp/MHGU-World-Long-Sword-Rise-Enhanced_v1.2-Recomp.zip) · [发布说明 / Release notes](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/tag/v1.2-Rise-Enhanced-Recomp) · [完整说明 / Full instructions](enhanced/README.md) · [构建 / Build](enhanced/BUILD.md)

## 历史版本 / Archived release

以下为 2026-11-23 公开版的原始说明。 / The original notes for the November 23, 2026 public release follow.

[下载 Windows 安装包](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/download/v1.2/MHGU_WORLD_STYLE_v1.2-Recomp.zip) · [v1.2 发布页](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/tag/v1.2) · [源码构建](BUILD.txt)

## 世界风格 v1.2-Recomp

太刀新增独立的「世界风格」。在装备箱选择该风格即可使用看破斩、特殊纳刀、大小居合和登龙兜割。适用于 MHGU 1.4.0、Windows Recomp 0.18.1 和 Forge 2.0.0。

### 更新内容

- **伤害**　大居合首刀和延迟伤害、登龙兜割七段的物理动作值均按原值乘 1.5。GU 以整数存储动作值，半点四舍五入。
- **出招**　突刺接特殊纳刀、预输入居合和手动兜割在原执行窗口直接切换动作。精简了每帧的按键扫描。
- **连招**　气刃连段每段最多可转向 45°，新增招式的收招可接狩技。攻击后前、左、右滚使用 GU 原生动作，向后翻滚保持朝向。
- **战斗**　练气不会自然衰减。小居合命中使用统一的最轻卡肉，兜割离地后到触地前有无敌判定。
- **音画**　大小居合和兜割使用本版刀线。大居合空挥的拔刀声较轻，第二道刀光音效的音量与反击成功时一致。兜割空挥也有下劈刀线和声音。

| 刃色 | 大居合首刀 | 大居合延迟伤害 | 兜割每段 |
| --- | ---: | ---: | ---: |
| 无刃 | 17 → 26 | — | — |
| 白刃 | 19 → 29 | 55 → 83 | 10 → 15 |
| 黄刃 | 31 → 47 | 72 → 108 | 15 → 23 |
| 红刃 | 39 → 59 | 86 → 129 | 25 → 38 |

### 安装与操作

1. 退出游戏，将旧版太刀动作 MOD 移出 `mods` 并备份。
2. 解压附件的 `romfs` 到 `mods/MHGU_WORLD_STYLE/`。
3. 重启后在装备箱给太刀选择「世界风格」。

攻击后按 **R+A** 看破斩、**R+B** 特殊纳刀、**R+X** 气刃突刺。纳刀后按 **X** 小居合或 **R** 大居合。白刃以上气刃突刺命中会起跳，可在空中按 **X** 提前下劈，也可等待自动下劈。

卸载前先在装备箱换回原生风格，再退出游戏并停用该 MOD。

### 验证

Windows Recomp 副本测试覆盖输入窗口、转向、翻滚、狩技、白刃大居合两段命中与反击保刃、红刃兜割七段和白刃兜割空挥。本版的实战验收平台为 Windows Recomp。

## 源码与旧版本

主工程在 [01_plugins/mhw_ls_rebuild](01_plugins/mhw_ls_rebuild/)，依赖和构建命令见 [BUILD.txt](BUILD.txt)。运行 `repack.py` 可用自行编译的 NRO 重新组装对应的 1.2 安装包。

[公会太刀 v1.1](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/tag/v1.1) · [旧模拟器 v1.0.7](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/tag/v1.0.7)

其他实用 MOD：[怪物显隐](https://github.com/Akumi-xu/MHGU-Monster-Hide)、[无边缘传送](https://github.com/Akumi-xu/MHGU-No-Area-Move)、[斩味条 HUD](https://github.com/Akumi-xu/MHGU-Sharpness-HUD)。

制作与测试决策由 Akumi-xu 完成，OpenAI Codex 辅助开发。原生游戏素材权利归 CAPCOM。
