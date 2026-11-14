#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""打包斩味条 HUD 的三个版本（只含插件本体，加载器用 forge 前置包）。

插件从 02_build/插件本体/ 取（由 01_plugins/sharpness_hud/make_release.ps1 编出来）：

    02_build/成品MOD/斩味条HUD/
        斩味条-主机模式.zip          MHGU_SHARPNESS_DOCKED
        斩味条-掌机模式.zip          MHGU_SHARPNESS_HANDHELD
        斩味条L + 十字键下 + Y可调整版.zip   MHGU_SHARPNESS_ADJ

用法：python 06_tools/patches/pack_sharpness.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import modpack as mp  # noqa: E402

VARIDIR = os.path.join(mp.ROOT, "02_build", "插件本体")
CAT = os.path.join(mp.ROOT, "02_build", "成品MOD", "斩味条HUD")

ADJ_NOTES = """本版本没有固定位置：位置和大小都在游戏里用手柄面板调，改完自动保存。
首次启动会按画面高度自动选：1080p 类用主机坐标，720p 类用掌机坐标。

1. 打开 / 关闭面板
    同时按住 L + 十字键下 + Y（按住不放只触发一次）

2. 面板里怎么操作
    十字键 上 / 下     选择要调的项
    十字键 左 / 右     调整当前项（MODE、STEP 是切换，X/Y/WIDTH/HEIGHT 是加减）
    X                  切换步长 1 <-> 10
    A                  切换 主机(DOCKED) / 掌机(HANDHELD)
    B                  关闭面板

3. 六项的含义
    MODE     当前在编辑哪一套配置：DOCKED（主机）或 HANDHELD（掌机）
    STEP     左右键每次的步长，1 或 10（调 HEIGHT 建议用 1）
    X / Y    斩味条左上角的屏幕坐标（像素）
    WIDTH    斩味条长度
    HEIGHT   斩味条粗细

4. 保存位置（存档目录，自动生成）
    sharpness_hud_docked.ini      主机模式的坐标
    sharpness_hud_handheld.ini    掌机模式的坐标
    sharpness_hud_adj.ini         上次用的是哪个模式
    每按一次左右键立刻写盘，不需要手动保存；
    下次启动会自动恢复上次的模式和坐标，不会回到默认位置。

5. PC / 安卓重编译版（MHGURecomp / gu-native）
    重编译版自带 Forge，不需要 forge 前置。
    把 MHGU_SHARPNESS_ADJ 文件夹整个放进 mods 目录：
      PC MHGURecomp：mhgurecomp.exe 旁边的 mods\\
      安卓 MHGURecomp：/sdcard/MHGURecomp/mods/
      gu-native（com.mhgu.recomp）：
          /sdcard/Android/data/com.mhgu.recomp/files/mods/
    首次进游戏按画面高度自动选主机/掌机；之后仍可用手柄面板微调。
    如果旧版切过掌机，sharpness_hud_handheld.ini 里可能还是主机坐标，
    删掉该文件让插件重写掌机默认值即可。

6. 注意事项
    * 面板打开时游戏没有暂停，建议在营地或安全区调整；
    * 数值有保护：X/Y >= 0，WIDTH >= 40，HEIGHT >= 6；
    * 如果 L+十字键下+Y 和游戏动作冲突，可以换成别的组合；
    * 不要和 world-sharpness-gauge 等其他斩味条 MOD 同时装，会抢同一块 HUD；
    * 调好之后如果想固化，可以把 ini 里的数值发回给制作方，
      改到固定位置版里，就不用每次调了。"""

README = """MHGU 斩味条 HUD：%(mod)s
============================================================
插件：%(plugin)s
位置：%(desc)s
游戏：怪物猎人 XX / Generations Ultimate（Switch），Title ID %(title)s，版本 1.4.0

============================================================
安装
============================================================
%(install)s
============================================================
%(notes)s
============================================================
注意
============================================================
1. 主机模式 / 掌机模式 / 可调整版三个包只能装一个。
2. 换版本时直接删掉旧的那个 MHGU_SHARPNESS_* 文件夹。
3. 前置只装一次；插件以后升级不用动前置。
4. 日志级别在 romfs\\nativeNX\\forge\\config.ini（在前置包里），默认 info。

============================================================
文件清单（SHA256）
============================================================
%(manifest)s
"""

VARIANTS = [
    dict(mod="MHGU_SHARPNESS_DOCKED", nro="sharpness_hud_docked.nro",
         plugin="sharpness_hud.nro", zip="斩味条-主机模式.zip",
         desc="底座/主机 1080p，位置写死 (192,62) 164×27", notes=""),
    dict(mod="MHGU_SHARPNESS_HANDHELD", nro="sharpness_hud_handheld.nro",
         plugin="sharpness_hud.nro", zip="斩味条-掌机模式.zip",
         desc="掌机 720p，位置写死 (128,41) 109×17", notes=""),
    dict(mod="MHGU_SHARPNESS_ADJ", nro="sharpness_hud_adj.nro",
         plugin="sharpness_hud_adj.nro", zip="斩味条L + 十字键下 + Y可调整版.zip",
         desc="位置可调：L+十字键下+Y 开菜单，十字键调，B 关",
         notes="============================================================\n"
               "使用方法\n"
               "============================================================\n" + ADJ_NOTES),
]


def build(v):
    src = os.path.join(VARIDIR, v["nro"])
    if not os.path.isfile(src):
        raise SystemExit("找不到 %s（先跑 01_plugins/sharpness_hud/make_release.ps1）" % src)
    data = open(src, "rb").read()
    manifest = "%-52s %10d  %s" % ("romfs\\nativeNX\\plugins\\" + v["plugin"],
                                   len(data), mp.sha256_bytes(data))
    readme = (README.replace('%(mod)s', v["mod"])
                    .replace('%(plugin)s', v["plugin"])
                    .replace('%(desc)s', v["desc"])
                    .replace('%(title)s', mp.TITLE_ID)
                    .replace('%(install)s', mp.install_block(v["mod"]).rstrip())
                    .replace('%(notes)s', v["notes"])
                    .replace('%(manifest)s', manifest))
    out_zip = os.path.join(CAT, v["zip"])
    mp.write_mod_zip(out_zip, v["mod"],
                     [(data, "romfs/nativeNX/plugins/" + v["plugin"])],
                     readme)
    mp.report(out_zip)


def main():
    for v in VARIANTS:
        build(v)


if __name__ == "__main__":
    main()
