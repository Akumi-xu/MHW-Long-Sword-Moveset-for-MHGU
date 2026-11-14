# sharpness_hud —— MHGU 斩味条 HUD

画在游戏画面上的斩味（锋利度）条。源码在这个工程里，构建走工作区的
`06_tools\build.ps1`，产物 `02_build\latest\sharpness_hud.nro`。

---

## 1. 为什么要重写

你给的那份是**成品 NRO**（`sharpness_hud_docked.nro` / `sharpness_hud_handheld.nro`），
没有源码，而且它本身满足不了实机：

```
$ python 06_tools/check_relocations.py <旧 nro>
结果: FAIL — DT_TEXTREL/DF_TEXTREL: 加载器要写只读页
```

`DT_TEXTREL` 意味着加载时要做写到只读段的动态重定位。模拟器不管，实机直接访存异常——
这就是它"只支持模拟器"的真正原因。**这个属性是链接阶段决定的，事后改 NRO 补不回来**，
只能重新编。所以这里把它的数据逻辑从二进制里逆出来，重新写了一遍。

## 2. 从旧插件逆出来的数据链

旧插件的 `forge_onUpdate`（`0x894`）反汇编后逻辑很干净，逐条还原如下（都是 GU main
模块偏移，`base = forge::mem::text_addr()`）：

| 偏移 / 函数 | 含义 |
| --- | --- |
| `*(base + 0x1885B58)` | `sPlayer` 单例指针 |
| `fn_0x276278(sPlayer)` | 取本地任务中玩家对象（`sub_60276278`），返回值 < 0x10000 视为无效 |
| `*player` | 玩家对象 vtable，用来判断武器种类（大厅玩家 `0x1748534` 会被判无效） |
| `*(player + 0x2774)` | **当前斩味值** |
| `*(player + 0x2778)` | **斩味总长**（最大斩味值，普通武器常见 150） |
| `*(player + 0x13F0)` | 游戏缓存的当前斩味色阶（`sub_6029CDFC` 的结果），>7 说明数据没就绪 |
| `fn_0x29CDFC(player, i)` | **第 i 点斩味的颜色**，0..6 对应 红橙黄绿蓝白紫 |

`fn_0x29CDFC` 就是反编译里的 `sub_6029CDFC`，它顺着
`player+0x13C4 → 装备数据 → 武器参数 → 斩味等级` 一路取，最后返回色阶；
`main.c` 里 `*(a1 + 5104) = sub_6029CDFC(a1, v2);`（5104 = 0x13F0）也印证了这一点。

旧插件的做法：把 `i` 从 1 跑 `max`，`bucket[color] = i`，再做一次"取前缀最大值"的
单调化，得到每一色的结束位置 `ends[0..6]`，然后按 `ends` 画色段。这里完全照搬。

武器 vtable（来自 `05_reference\decompiled\mhgu_dti.json` 的 `uPlayerQuest*` 类）：

| 武器 | vtable | 武器 | vtable |
| --- | --- | --- | --- |
| w00 大剑 | `0x1749A88` | w09 铳枪 | `0x174E110` |
| w01 片手剑 | `0x174A354` | w10 弓 | `0x174E9E0` |
| w02 大锤 | `0x174AC20` | w11 双剑 | `0x174F2B4` |
| w03 长枪 | `0x174B4F0` | w12 狩猎笛 | `0x174FB80` |
| w04 重弩 | `0x174BDC4` | w13 操虫棍 | `0x1750450` |
| w06 轻弩 | `0x174C694` | w14 盾斧 | `0x1750D20` |
| w07 太刀 | `0x174CF64` | w15 猎猫 | `0x17515EC` |
| w08 斩击斧 | `0x174D838` | | |

## 3. 相对旧版改了什么

| | 旧版 | 现在 |
| --- | --- | --- |
| 实机 | ❌ 有 `DT_TEXTREL` | ✅ `arm-none-eabi-gcc` + `-nostartfiles` + `-Wl,-z,text`，构建自动跑重定位检查 |
| 远程/猎猫 | 照样显示 | w04 重弩 / w06 轻弩 / w10 弓 / w15 猎猫 直接隐藏 |
| 底座 vs 掌机 | 两个 NRO 分开编 | 同样分两个版本，一条命令出齐（见第 4 节） |
| 绘制方式 | 开了一个 ImGui 窗口 | 直接用前景绘制列表（`DrawList::foreground()`），不占窗口 |
| 已消耗部分 | 原版：同色 alpha 60 | MHRise 风格：同色压到 30% 不透明，看起来是"变暗"而不是"变透明" |
| 整体观感 | 平面色块 | 深色轨道 + 上浅下深竖向渐变 + 顶部高光 + 当前值白色边缘 + 红斩时边框闪红 |
| forge 菜单 | — | 菜单打开时不画，避免遮挡 |
| 日志 | `[sharp] player=…` | 换武器时打一条：武器号、当前/最大斩味、7 段边界 |

## 4. 版面：两个构建版本

运行时判断模式不可靠（ImGui 的 `DisplaySize` 在切换底座/掌机时不跟着变，旧插件
也是分两个版本），所以坐标在**编译期**定死，而且是**原始像素、不做缩放**——
早期版本按 `DisplaySize` 等比缩放，结果两套基准被缩放成同一个位置，两个版本看起来
一模一样。：

| 版本 | 触发方式 | 基准坐标 x0,x1,y0,y1 | 基准分辨率 |
| --- | --- | --- | --- |
| 底座 Docked | 默认，或 `SHARPNESS_LAYOUT=docked` | 185, 404, 62, 85 | 1920x1080 |
| 掌机 Handheld | `SHARPNESS_LAYOUT=handheld` | 125, 288, 41, 58 | 1280x720 |

一条命令出两个版本和两个安装包：

```powershell
.\01_plugins\sharpness_hud\make_release.ps1
```

产物：

```
02_build\variants\sharpness_hud_docked.nro
02_build\variants\sharpness_hud_handheld.nro
02_build\release\MHGU_SHARPNESS_DOCKED\    (+ .zip)   模拟器
02_build\release\MHGU_SHARPNESS_HANDHELD\  (+ .zip)   模拟器
02_build\NS实机\MHGU_SHARPNESS_DOCKED\    (+ _NS.zip)  实机
02_build\NS实机\MHGU_SHARPNESS_HANDHELD\  (+ _NS.zip)  实机
```

坐标是**原始像素、不做任何缩放**（旧插件也是写死的）。早期版本按 `DisplaySize`
等比缩放，结果 125×1.5≈187、41×1.5≈61，两套基准被缩成同一个位置，两个版本看起来一样——
这就是"两个版本都是主机模式位置"的原因，已去掉。

**只装一个**，两个同时装会出现两条斩味条。装完看日志确认版本：

```
[sharp] 已加载：底座 Docked 1080p 版（实机构建）
[sharp] 已加载：掌机 Handheld 720p 版（实机构建）
```

### 运行时微调

forge 菜单（默认按 `key = 73`）里有四个滑块：X / Y / 宽 / 高，拖动即时生效，
不用重新编译。每次改动会写一行日志：

```
[sharp] layout x=131 y=47 w=168 h=17（掌机 Handheld 720p 版默认 125,41,163,17）
```

把这行的数值告诉我，我就把它写成该版本的默认 `PROFILE`。

想直接改源码也行，`PROFILE` 现在是 `(x, y, 宽, 高)` 整数：

```rust
#[cfg(sharpness_handheld)]
const PROFILE: (i32, i32, i32, i32) = (128, 41, 109, 17);
#[cfg(not(sharpness_handheld))]
const PROFILE: (i32, i32, i32, i32) = (189, 62, 166, 27);
```

配色取自旧插件的颜色表（`PALETTE`，0..6 = 红橙黄绿蓝白紫）：

| 色 | RGB | 用法 |
| --- | --- | --- |
| 红 | `#E6362E` | 色段 0 |
| 橙 | `#F28C1E` | 色段 1 |
| 黄 | `#F4D43D` | 色段 2 |
| 绿 | `#4ED64A` | 色段 3 |
| 蓝 | `#2F7BFF` | 色段 4 |
| 白 | `#F0F0F0` | 色段 5 |
| 紫 | `#B06CFF` | 色段 6 |

画法也照旧：每一段先用同名颜色 **alpha 60** 铺满，再把 `0..当前斩味` 的部分用
**alpha 255** 盖上去；段之间画 `0x70000000` 竖线；外框 `0xD8000000`、内槽
`0xF0000000`、边框 `0xE0202020`。

## 5. 已知限制

- **偏移绑定游戏版本**：上面所有偏移是对 MHGU 1.4.0 的 `sub_xxxx` 与旧插件反汇编得出的，
  游戏更新（或换 Build ID）后要重新核对，尤其是 `0x29CDFC` 和 `0x276278` 两个函数。
- **没有实机验证**：构建、重定位、符号导入导出都检查过，但画面效果和数值正确性要你跑一次。
  跑完 `forge-loader.log` 里应该有一行 `[sharp] W07 cur=… max=… bands=[…]`，
  对一下数字就知道偏移对不对。

## 6. 原始资料

- 旧成品：`D:\AppData\AAAAA\ns\新建文件夹\MHGU斩味条\`（保留着，没动）
- 反编译：`05_reference\decompiled\main.c`（`sub_6029CDFC` / `sub_60276278`）
- DTI：`05_reference\decompiled\mhgu_dti.json`（vtable 表）

## 7. 调试版（定位原生斩味条）

想让它不只是"盖住"、而是真的关掉游戏自带的斩味条，需要先知道是哪个 UI 节点在画它。
调试版为此准备：

```powershell
& ._toolsuild.ps1 -Project sharpness_hud -NoCopy -EnvVars @{SHARPNESS_LAYOUT='docked'; SHARPNESS_DEBUG='1'}
```

进任务后会对每个玩家对象打一次 `[dbg]` 区块，内容包括：

- 玩家对象地址、武器号、当前/最大斩味、缓存色阶；
- 武器数据块 `player+0x2760 .. 0x27A0` 的原始 dword；
- 全部单例：`[dbg] S addr=0x… class=… size=0x…`；
- **谁引用了玩家指针**：`[dbg] REF <类名> +0x偏移 (<属性名>) = player`，
  这是找"玩家 → HUD"连线的关键。

拿到这些就能定位驱动原生斩味条的 UI 单元，下一步再决定是 hook 绘制还是关节点。

注意：调试版会多打一百多行日志，且单例枚举跑在游戏线程上，只跑一次。
如果进任务后卡住/崩溃，日志会停在最后一行，从那一行能看出是卡在哪一步。

### 外形（MHRise 剑形）

条本身是像素画的剑形轮廓，不是矩形：

- **右侧**：收成三角刀尖，长度 = 高度 × 1.35，颜色取最后一档斩味（斩味见底时压暗）；
- **左侧**：护手（高出一截的竖条）+ 剑柄头（圆）；
- **轮廓**：2px，颜色 = 当前斩味档位颜色；
- **黑底**：矩形黑底比剑形大 3~5px，作用是把底下游戏自带的贴图斩味条整个盖住。

预览图见 `_re/mock_sword.png`（满斩/绿斩/红斩三态）。

### 外形（对齐 MHRise 截图）

按用户提供的崛起截图重画：整把剑 = 当前斩味色的**实心剪影**，中间挖一个黑边窗口放色块。

- 刀身：右端斜切成刀尖（削尖角，非三角对称）；
- 剑柄：护手（竖条）+ 握把（窄条）+ 柄头（圆）；
- 窗口：黑边 1.5px，里面是 7 段色块（剩余亮、已消耗压到 30%）；
- 最外圈：放大 4px 的黑色剑形，既是描边，也用来盖住游戏自带的贴图斩味条。

### 隐藏原生斩味条（HUD 布局补丁）

任务 HUD 布局在 `<语言>/arc/quest_<语言>.arc` 的 `eng\GUI_quest\hud\hud0`，
里面有按名字引用的节点 `sharp`、`sharp_0..6`、`sharp_0a..6a`。

`06_tools/make_hud_patch.py` 把这些名字**等长改名**成 `xharq*`，文件大小与偏移全不变；
脚本按名查不到节点，斩味条就不会被驱动。产物 `04_assets/hud_patch/quest_chS.arc`
会被 `package.ps1 -HudPatch` 打进 `romfs/nativeNX/chS/arc/`。

### 位置微调 + 持久化

forge 菜单里有 X / Y / Width / Height 四个滑块，拖动即时生效。

- 停手约 0.6 秒后自动保存到游戏存档目录：
  - 主机版 → `sharpness_hud_docked.ini`
  - 掌机版 → `sharpness_hud_handheld.ini`
- 下次启动自动读取；两个版本各存各的，互不影响。
- 日志会打印读取/保存的数值，方便回传：
  `[sharp] 已保存 sharpness_hud_docked.ini x=... y=... w=... h=...`

### 外形（崛起样式）

- 剑身（柄头三角 + 握把 + 护手 + 刀身 + 斜切刀尖）= **当前斩味档位颜色**；
- 中间窗口比剑身窄一圈矮一圈（垂直内缩 26%），带 1.5px 黑边；
- 窗口内是 7 段彩色斩味条，当前值之前实色、之后压到 26%；
- 刀尖横向长度 = 高度 × 0.95（比之前更尖）。

## 8. 调整版（SHARPNESS_ADJ=1）

固定版把位置写死了；需要现场微调时用调整版：

```powershell
& ._toolsuild.ps1 -Project sharpness_hud -NoCopy -EnvVars @{SHARPNESS_LAYOUT='docked'; SHARPNESS_ADJ='1'}
```

- 菜单里两个大按钮切 **DOCKED / HANDHELD**，各读各的 ini；
- 四个参数各四颗大按钮（±1 / ±10，高度是 ±1 / ±5），没有滑块 —— 手机上拖滑块很难点，按钮更好按；
- 每改一次立刻写盘（`sharpness_hud_docked.ini` / `_handheld.ini`）；
- 体积 60 KB（比固定版多 32 KB，因为带 ini 解析）。

固定版（不带 ADJ）保持 28 KB，不受影响。

## 9. 部署脚本的坑（2026-10-29 修）

`deploy.ps1 -Clean` 原来会 `Remove-Item $modDir -Recurse`，把 `exefs\`（加载器）一起删了。
如果随后拷贝又被占用打断，加载器就永久缺失 → **所有插件 MOD 同时失效**（不只是斩味条）。

现在：

- `-Clean` 只删 `romfs
ativeNX\plugins\*.nro`，不碰 exefs/forge；
- 拷贝 `forge` 改成拷内容（`forge\*` → `forge\`），避免出现 `forgeorge\`；
- 结尾加自检：缺 `exefs\subsdk9` 或 plugins 为空直接红字报警。

排查口诀：**所有插件一起失效 = 加载器没了**，先看 `romfs
ativeNXorge\` 和 `exefs\subsdk9` 在不在。

## 10. 自带菜单（调整版，纯手柄控制）

forge 原菜单只能键盘开，所以调整版自带面板，完全不依赖它，也不吃触屏。

| 操作 | 按键 |
| --- | --- |
| 开关面板 | **同时按住 L + 十字键下 + Y** |
| 选行 | 十字键 上 / 下 |
| 调整数值 | 十字键 左 / 右 |
| 步长 1 ↔ 10 | X |
| 主机 ↔ 掌机 | A |
| 关闭 | B |

- 只有选中行响应左右键，屏幕上会高亮当前行；
- 数值改动立刻写进 `sharpness_hud_{docked,handheld}.ini`；
- 面板按 `dh/1080` 缩放，720p 掌机不溢出；文字用 `ImFont::current()` + 26*s 号绘制。

## 11. PC / 安卓重编译版（只影响可调整版）

重编译版（MHGURecomp / gu-native）的插件运行方式和 Switch 一样，固定版和调整版本来都能加载。
这次只动**可调整版**，补了三个在重编译版/安卓上会踩的问题：

1. **首次启动自动选主机/掌机**
   以前调整版默认永远用 DOCKED 坐标：PC 1080p 没问题，安卓/720p 会画在游戏原生斩味条
   下面，看起来像“没对齐”。现在第一次进游戏时按 `ImGui DisplaySize.y` 自动选：
   - `< 900` → 掌机 `(128,41,109,17)`
   - `>= 900` → 主机 `(192,62,164,27)`
   选择结果会写进 `sharpness_hud_adj.ini`，之后仍可在面板里按 `A` 切换。

2. **两种模式的默认坐标分开**
   以前切到 HANDHELD 时，如果 `sharpness_hud_handheld.ini` 不存在，会用 `LayoutCfg::default()`
   生成一份**主机坐标**，等于切了模式位置不变。现在 `cfg_load(mode)` 按模式各写各的默认值：
   - DOCKED：`192,62,164,27`
   - HANDHELD：`128,41,109,17`
   旧版留下的错误 `sharpness_hud_handheld.ini` 删掉即可让插件重写。

3. **安卓上剑身整块消失**
   gu-native 安卓运行时的 ImGui 桥对 `ImDrawList_AddConvexPolyFilled`（数组版多边形）
   不可靠：刀身/柄头画不出来，只剩护手矩形和中间的彩色窗口，看起来像“只有一条彩条”。
   现在刀身改成「矩形 + 三角形」、柄头改用 `add_triangle_filled`，不再用数组多边形；
   另外在画之前 `push_clip_rect_full_screen()`，防止别的斩味条 MOD 留下的 scissor 把剑形裁掉。

固定版（`sharpness_hud_docked.nro` / `sharpness_hud_handheld.nro`）的代码路径没有改。

### 实测（PC MHGURecomp）

| 画面 | 自动模式 | 日志 |
| --- | --- | --- |
| 1280×720 | 掌机 | `切到 掌机 模式 (128,41,109,17)`、`bar=(128,41)-(237,58)` |
| 1920×1080 | 主机 | `切到 主机 模式 (192,62,164,27)`、`bar=(192,62)-(356,89)` |

另外实测：`L+十字键下+Y` 打开面板、左右键改 X 立刻写
`sharpness_hud_docked.ini`、按 `A` 切模式并写 `sharpness_hud_adj.ini` 均正常。

安卓（Retroid Pocket 5，gu-native 0.1.6）用调试构建在村里画假数据验证：
剑身、护手、柄头、彩色窗口都完整显示，日志 `disp=1920x1080`。
真机任务内还需要你装回正式包再确认一次。

### 不要和 world-sharpness-gauge 同时装

`world-sharpness-gauge` 是另一个斩味条 MOD，它会 hook 原生斩味条并往 UI 里插
scissor mask。两个同时装会抢同一块 HUD 区域，日志里能看到它自己的
`WSG r65 ...` 报错。**只保留一个斩味条 MOD**。

### 安装路径

把 `MHGU_SHARPNESS_ADJ` 文件夹整个放进 `mods`：

| 版本 | mods 目录 |
| --- | --- |
| PC MHGURecomp | `mhgurecomp.exe` 旁边的 `mods\` |
| 本工作区安卓 APK | `/sdcard/MHGURecomp/mods/` |
| gu-native（`com.mhgu.recomp`） | `/sdcard/Android/data/com.mhgu.recomp/files/mods/` |
| Eden / 实机 | 原来的 Forge 目录，仍需 Forge 前置 |

重编译版自带 Forge，不需要再装前置。
