# MHGU Utility Mods

怪物猎人 XX / Monster Hunter Generations Ultimate（Switch，Title ID `0100770008DD8000`，v1.4.0）的三个 Forge 插件，包含现有安装包、Rust 源码与构建/打包工具。

## 下载与功能

| MOD | 下载 | 功能 |
| --- | --- | --- |
| 怪物显隐 | [安装包](02_build/%E6%88%90%E5%93%81MOD/%E6%80%AA%E7%89%A9%E6%98%BE%E9%9A%90/MHGU%E6%80%AA%E7%89%A9%E9%9A%90%E8%97%8F.zip) | L + 十字键左 / 右，分别隐藏或恢复两只大型怪物；隐藏时停止活动 |
| 无边缘传送 | [安装包](02_build/%E6%88%90%E5%93%81MOD/%E6%97%A0%E8%BE%B9%E7%BC%98%E4%BC%A0%E9%80%81/%E6%97%A0%E8%BE%B9%E7%BC%98%E4%BC%A0%E9%80%81.zip) | 阻止普通走跑触发区域切换，保留蹲下、攀爬、跳下和长按 L 等放行方式 |
| 斩味条 HUD | [主机版](02_build/%E6%88%90%E5%93%81MOD/%E6%96%A9%E5%91%B3%E6%9D%A1HUD/%E6%96%A9%E5%91%B3%E6%9D%A1-%E4%B8%BB%E6%9C%BA%E6%A8%A1%E5%BC%8F.zip) / [掌机版](02_build/%E6%88%90%E5%93%81MOD/%E6%96%A9%E5%91%B3%E6%9D%A1HUD/%E6%96%A9%E5%91%B3%E6%9D%A1-%E6%8E%8C%E6%9C%BA%E6%A8%A1%E5%BC%8F.zip) / [可调整版](02_build/%E6%88%90%E5%93%81MOD/%E6%96%A9%E5%91%B3%E6%9D%A1HUD/%E6%96%A9%E5%91%B3%E6%9D%A1L%20+%20%E5%8D%81%E5%AD%97%E9%94%AE%E4%B8%8B%20+%20Y%E5%8F%AF%E8%B0%83%E6%95%B4%E7%89%88.zip) | 显示斩味条；可调整版用 L + 十字键下 + Y 打开位置和大小设置 |

斩味条三个版本只能安装一个。压缩包按原样保留，具体操作、配置与安装步骤见包内说明。

## 安装

- Eden：把包内的 `MHGU_*` 文件夹放入 `%APPDATA%\eden\load\0100770008DD8000\`，启用 MOD 后完全重启游戏。
- Switch / Eden 需要已安装兼容的 Forge 加载器；本仓库的这五个包只含插件，不附加载器。
- PC / 安卓 MHGURecomp 自带插件加载支持时不需要另装 Forge 前置；具体目录见各插件与包内说明。
- 卸载时删除对应 MOD 文件夹；同功能插件只保留一份。

## 源码

- [怪物显隐](01_plugins/monster_hide/)：说明、完整 Cargo 工程、链接脚本。
- [无边缘传送](01_plugins/no_area_move/)：说明、配置、完整 Cargo 工程。
- [斩味条 HUD](01_plugins/sharpness_hud/)：说明、完整 Cargo 工程、三个版本的构建脚本。
- [无边缘传送机制](07_docs/%E6%97%A0%E8%BE%B9%E7%BC%98%E4%BC%A0%E9%80%81-%E6%9C%BA%E5%88%B6.md)。

## 构建与打包

把仓库克隆到纯 ASCII 路径，例如 `C:\mhgu-mods`。准备 Rust nightly、rust-src、cargo-mhgu-forge 1.5.0、ARM GNU Toolchain（`arm-none-eabi-gcc`）与 Python 3，并把工具加入 PATH。工程使用 `mhgu-forge 1.5.0`，Cargo.lock 随源码提供。

进入仓库的 `utility_mods` 目录后运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\06_tools\build.ps1 -Project monster_hide
powershell -NoProfile -ExecutionPolicy Bypass -File .\06_tools\build.ps1 -Project no_area_move
powershell -NoProfile -ExecutionPolicy Bypass -File .\01_plugins\sharpness_hud\make_release.ps1

python .\06_tools\patches\pack_monster_hide.py
python .\06_tools\patches\pack_no_area_move.py
```

斩味条构建脚本会编译并打包三个变体。构建脚本默认执行重定位检查；也可手动运行：

```powershell
python .\06_tools\check_relocations.py .\01_plugins\monster_hide\monster_hide.nro
```

## 本次上传的检查范围

2026-11-14：五个已有 ZIP 完整性检查通过，包内五个 NRO 重定位检查全部 PASS。本次归档未重新编译，也未重新进行 Eden 或实机游戏测试；PASS 仅代表重定位检查通过。

源码为上传时的工作区快照；不宣称与历史安装包可逐字节复现。仓库不包含游戏 ROM、密钥、转储、构建缓存或其他 MOD。
