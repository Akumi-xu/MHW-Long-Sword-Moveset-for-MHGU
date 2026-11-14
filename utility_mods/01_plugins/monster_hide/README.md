# monster_hide — 大型怪物手动显隐/冻结

MHGU（Switch，Title `0100770008DD8000`，实测 1.4.0）forge 插件。
任务里最多同时有两只大型怪物，用组合键分别隐藏/恢复它们：

| 组合键 | 作用 |
| --- | --- |
| 按住 `L` + 按 `十字键左` | 隐藏 / 恢复第一只大型怪物 |
| 按住 `L` + 按 `十字键右` | 隐藏 / 恢复第二只大型怪物 |

每按一次切换一次，两只互不影响；场上没有对应怪物时按键不做事。
插件加载后两只都正常显示，不会自动藏怪。

隐藏后怪物会**完全停止活动**：不再移动、不出招、没有攻击特效和音效。
恢复后怪物从原来的状态自然继续，不需要打它一下才动。

---

## 原理与偏移（MHGU 1.4.0）

```text
base          = forge::mem::text_addr()
sEnemy        = SingletonManager::get_by_name("sEnemy")
sFestaUnit    = SingletonManager::get_by_name("sFestaUnit")

sEnemy + 0x1C 起是当前区域最多 10 只活着的敌人（大怪 + 小怪）：
  enemy[k]    = *(u32 *)(sEnemy + 0x1C + 4*k), k = 0..9
  status      = *(u32 *)(enemy + 0x1428)       // 状态块
  net_id      = *(u16 *)(status + 0x1B6)        // 网络 ID

大怪/小怪用对象自己的 DTI 类名区分：
  uEm###_00   -> 大怪
  uEms###_00  -> 小怪
只把前两只大怪放进 slot 0 / slot 1。

sFestaUnit + 0x88 / + 0x8C = 两只大怪的 unit 指针

大怪 +0x15F0 = 体型倍率（游戏每帧拿它算 mScale）
```

### 真正起作用的机制

游戏的 unit 更新循环用的是 **sFestaUnit + 0x88/+0x8C 这个 unit 列表**，
不是 `sEnemy` 的敌人数组。

隐藏时插件每帧重建这个列表：**只把没隐藏的怪按 slot 顺序排成连续前缀**，
隐藏的怪直接不出现在列表里。游戏的更新循环遇到 0 就停，
所以：

- 隐藏第一只时，第二只被挪到 `0x88`，继续正常活动；
- 隐藏第二只时，列表是 `[第一只, 0]`，第一只继续正常活动；
- 两只都隐藏时，列表是 `[0, 0]`，两只都停。

同时把隐藏怪的体型倍率 `+0x15F0` 写成 `0.001`，模型不可见。

恢复时把体型写回原值，下一帧重建列表时它又会被排回去，
AI 从原来的动作/状态继续——**不要**去清零动作 `+0x73E0` 或帧步长 `+0x1C`：
实测清零动作会让怪恢复后僵住，要被打出硬直才动。

### vtable 合法性判断必须用 `text_addr()` 动态算

模块每次启动的加载基址会变（实测见过 `0x00ED7000`、`0x010A4000`、`0x005E0000` 等），
怪物 vtable 落在 `base + 0x172F000 .. base + 0x184AD78`（main 的 data 段）。
早期版本把范围写死成 `0x02000000..0x04000000`，换一次启动就会把所有按键
误判成“没有活怪”，表现就是按键完全没反应。

---

## 编译

```powershell
cd C:\gumod\01_plugins\monster_hide
cargo forge build --release
```

产物：`01_plugins\monster_hide\monster_hide.nro`（约 56 KB）。

## 部署（Eden）

只放插件、不重复装加载器（加载器已经在别的 MOD 里）：

```powershell
cd C:\gumod
powershell -NoProfile -ExecutionPolicy Bypass -File .\06_tools\deploy.ps1 `
    -ModName MHGU_MONSTER_HIDE `
    -Plugin .\01_plugins\monster_hide\monster_hide.nro `
    -Loader none
```

装完在 Eden 的 MOD 列表里勾选 `MHGU_MONSTER_HIDE`，完全重启游戏（插件只在启动时加载）。
实机把同一个 `.nro` 放到 `SD:\atmosphere\contents\0100770008DD8000\romfs\nativeNX\plugins\`。

## 排查

日志：Eden 是 `%APPDATA%\eden\sdmc\forge-loader.log`，实机是 SD 卡根目录。

- 加载成功：
  `[hide] monster_hide loaded: L+Left / L+Right toggles large monster 1 / 2`
- 识别到大怪：
  `[hide] slot 0 monster=47E50060 id=60`
- 按键隐藏：
  `[hide] slot 0 HIDDEN (size 1.12)`
- 按键恢复：
  `[hide] slot 0 SHOWN (size 1.12)`

如果隐藏后还能出招：
1. 确认是在任务里、大怪都在场；
2. 检查 `sFestaUnit` 单例是否能取到（取不到时列表不会重建）；
3. 游戏更新后偏移可能整体变化，需要重新核对 `sFestaUnit+0x88/0x8C`
   和 `+0x15F0`。

## 已知限制

- 只处理“当前场上最多两只大型怪物”的情况；第三只/第四只是前一只死亡后才刷，
  刷出来后插件会重新扫描 10 敌数组，把它当作新的 slot，默认显示。
- 隐藏靠“从 unit 更新列表摘除 + 缩小”，怪物对象本身还在；恢复时写回列表。
- 两只都隐藏时列表是 `[0,0]`；恢复顺序按按键来，和隐藏顺序无关。
- 偏移对应 MHGU 1.4.0。

---

## 重编译版（PC / 安卓 MHGURecomp）修复：2026-11-03

重编译版的 guest 堆在 `0x0Cxxxxxx` 一带，而旧版把“合法对象”下限写死成 `0x4000_0000`，
于是两只大怪一个都认不出来；`update()` 仍每帧把 `sFestaUnit + 0x88/0x8C` 写成 `[0,0]`，
表现就是**怪直接消失、按键也没反应**。修了两处：

1. `plausible_obj` 下限改为 `0x0020_0000`（仍然靠 vtable 范围校验挡脏指针）；
2. 只有已经认出至少一只大怪时才改写 `sFestaUnit` 的 unit 列表。

在 PC 重编译版实测（quest 305，双 Great Maccao）：

```text
[hide] slot 0 monster=0C184860 id=0
[hide] slot 1 monster=0C1B2910 id=1
[hide] slot 1 HIDDEN (size 0.96)      # 体型 0x3A83126F = 0.001
sFestaUnit+0x88/0x8C: [0C184860, 00000000]   # 只把第二只从更新列表摘掉
[hide] slot 1 SHOWN (size 0.96)       # 体型写回原值
```

安装（安卓）：把 `MHGU_MONSTER_HIDE` 整个文件夹放进

```text
/sdcard/MHGURecomp/mods/MHGU_MONSTER_HIDE/romfs/nativeNX/plugins/monster_hide.nro
```

重编译版由自身加载插件，不需要 Forge 前置；设置面板「MOD」页里能看到它。
