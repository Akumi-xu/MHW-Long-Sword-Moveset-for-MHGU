# 公会太刀 · MHGU PC 重编译版

在公会风格太刀上加入《怪物猎人：世界／冰原》的看破斩、特殊纳刀、居合拔刀斩、居合拔刀气刃斩、气刃突刺和气刃兜割。

安装：关闭游戏，在支持 Forge 插件的 MHGU PC 重编译版中，把压缩包内的 romfs 放到游戏目录的 mods/MHW_WORLD_DEV/ 下。安装后应能看到 mods/MHW_WORLD_DEV/romfs/nativeNX/plugins/mhw_ls_rebuild.nro。升级时先将旧版 MOD 文件夹移出 mods，再安装新版。启动后选择公会风格与太刀。

操作（GU 按键）：攻击中 R+A 接看破斩，R+B 接特殊纳刀，纳刀后 X 出小居合、R 出大居合。攻击中 R+X 出气刃突刺，开刃后突刺命中可接气刃兜割。发动收刀术后特殊纳刀起手为 1.16 倍速。小居合及兜割命中后可获得练气回复效果。

兼容：在 PC 重编译版 1920×1080 下已与 MHGU_HP_OVERLAY 同场验证。盾斧变4G可共同加载，但其动作插件的部分补丁受重编译运行时限制。本包仅在 PC 版验证。当前版本无单独的参数设置菜单。

卸载：关闭游戏，移除 mods/MHW_WORLD_DEV 文件夹。

## 编译插件

在纯 ASCII 路径下展开源码，保留目录结构。使用项目指定的 Rust 工具链、ARM 编译器及 cargo-forge。在 PowerShell 中执行：

```powershell
cd 01_plugins/mhw_ls_rebuild
$env:ML_WORLD_COMBAT="1"
$env:ML_WORLD_INPUT_VM="1"
$env:ML_WORLD_PRESENTATION="1"
cargo forge build --release
```

资源转换脚本另需本地游戏转储及工作区工具；安装成品无需重新生成资源。
