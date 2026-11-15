# 公会太刀 · 世界招式移植 v1.1

本次PC版对应关系：

| 正式版本 | 内部版本 | 主要区别 |
| --- | --- | --- |
| [v1.0.8](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/tag/v1.0.8) | v13 | 新樱花命中特效；兜割下劈后受击时继续延迟结算 |
| [v1.1](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/tag/v1.1) | v11优化，保留v13刀线 | 四项内部优化；保留v11命中特效和受击暂停机制 |

## 版本对应

v1.1 = 内部 v11优化_保留V13刀线。在该内部基线最后追加默认特殊纳刀术3速度；版本号保持，资源与基线逐文件一致。

## 最终调整

特殊纳刀默认内置世界纳刀术3速度：起手有效阶段1.16倍，无需收刀技能；GU收刀术不重复叠加。后段等待、居合反击窗口和其他招式参数保持。

PC副本已测无技能与四种收刀技能来源：起手均1.16倍、后段均1倍。普通收刀仍由GU处理。

## 更新与差异

对应内部“v11优化_保留V13刀线版”。玩法以v11为准，保留v13的突刺红线和大居合第二道刀线。
- 缓存粒子资源及曲线，移除逐帧重复查找；保持float32累加顺序。
- 为298项音效建立招式索引，每次最多检查14项，保留音效时机、去重及同帧顺序。
- 无损合并36项重复TEX/EAN资源，194项变158项，特效ARC减少109936字节；保留像素和UV。
- 排除未走的旧控制器，清理无用标记和调试资源；插件文件减少4096字节。
- 动作值、派生、反击窗口、刀线长度/亮度/宽度和音效时机保持对应基线。

与v1.0.8的区别：本版按回退要求保留v11的兜割命中特效及结算方式，没有v12受击独立结算，也没有v13的新樱花替换。兜割被打断时，剩余延迟伤害和特效可能等人物僵直结束后继续。

验证：既有PC0.16.0副本记录覆盖两道保留刀线、看破、三种开刃大居合实际音画、自动兜割七次真实扣血。等价性检查覆盖41040次float32值、10260次完整粒子变换及200901组音效选择；未单独测量帧率或显存收益。

## 安装与操作

PC安装：关闭游戏，先把旧版MHW_WORLD_DEV整个移出mods，再将本包romfs放入游戏目录mods/MHW_WORLD_DEV/。不要直接叠加旧版残留文件，不要同时启用另一份公会太刀或其他太刀动作/派生MOD。使用公会风格太刀。
操作（GU按键）：攻击中R+A看破斩、R+B特殊纳刀、R+X气刃突刺；纳刀后X小居合、R大居合；开刃突刺命中起跳后可自动下劈或按R手动下劈。
参数：无新增配置菜单或外置ini；默认数值已包含在插件中，修改源码后可重新构建。
限制：主要验证MHGURecomp 0.16.0；本次未做安卓、模拟器或实机验收，也未重新覆盖所有第三方MOD组合；并非世界画面的完全还原。
卸载：关闭游戏，将mods/MHW_WORLD_DEV移出。存档与游戏配置无需改动。

## 下载和源码

- PC成品：[MHW-LS-MHGU-PC-v1.1.zip](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/download/v1.1/MHW-LS-MHGU-PC-v1.1.zip)
- 配套源码：[MHW-LS-MHGU-Source-v1.1.zip](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/download/v1.1/MHW-LS-MHGU-Source-v1.1.zip)。含C/Rust源码、生成头、Cargo.lock、构建配置、默认参数、测试、资源编辑及重新组装工具，步骤见BUILD.txt；修改资源以本页PC附件为基线。
- 本轮仅发布PC包；旧模拟器v1.0.7仍在历史发行页，本轮没有更新或重新验收。

制作/测试决策：Akumi-xu；OpenAI Codex辅助代码、分析及打包。原生游戏素材权利归CAPCOM；本次未变更第三方许可。

## SHA256

- `MHW-LS-MHGU-PC-v1.1.zip`：`4bd3928a85b0e836f9a90df9f61bc87cb59384ca11ca514272ccae4d55e2983b`
- `MHW-LS-MHGU-Source-v1.1.zip`：`79277855fbe8485f23c68b698a5385843ac42ffa926976dec6f696984a52235e`

## 旧模拟器版本

[模拟器v1.0.7](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU/releases/tag/v1.0.7)，需要现有Forge加载器。本轮PC改动未在模拟器、安卓或实机验收。仓库根目录“公会太刀_模拟器版.zip”仍为v1.0.7。

## 其他实用 MOD

[怪物显隐、无边缘传送、斩味条HUD](utility_mods/)保持原样。
