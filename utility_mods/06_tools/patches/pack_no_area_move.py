#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""打包「无边缘传送」：只含插件本体，加载器用 forge 前置包。

用法：
    python 06_tools/patches/pack_no_area_move.py

产出：
    02_build/成品MOD/无边缘传送/无边缘传送.zip
        MHGU_NO_AREA_MOVE/
            说明.txt
            romfs/nativeNX/plugins/no_area_move.nro
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import modpack as mp  # noqa: E402

ROOT = mp.ROOT
PLUGIN = os.path.join(ROOT, "01_plugins", "no_area_move", "no_area_move.nro")
OUTDIR = os.path.join(ROOT, "02_build", "成品MOD", "无边缘传送")
ZIP = os.path.join(OUTDIR, "无边缘传送.zip")
FOLDER = "MHGU_NO_AREA_MOVE"

README = """MHGU 无边缘传送（no_area_move）v1.0
====================================

作用
  普通走到地图边缘不再换区；保留这些合法换图：
    - 蹲下 / 蹲走（轻推慢走、满推跑动都算"蹲模式下"）
    - 攀爬（上爬 / 下爬 / 攀爬跳跃）
    - 跳下（跳/空中动作）
    - 切道具（长按 L 打开道具轮盘；ZL 默认关闭，可在 ini 打开；点按是否生效也见 ini）

============================================================
安装
============================================================
%(install)s
============================================================
调数值
============================================================
第一次运行后，在游戏存档目录生成 no_area_move.ini：
    Eden：%APPDATA%\\eden\\nand\\user\\save\\0000000000000000\\<用户ID>\\0100770008DD8000\\
    实机：需要用 JKSV 等工具导出/导入存档后编辑
常用键（改完重启游戏生效）：
    enabled = true            总开关，false = 完全恢复原版
    allow_crouch = true       蹲下 / 蹲走
    allow_item = true         切道具
    item_by_tap = false       点按一下 L 是否放行
    item_by_hold = true       长按 L（道具轮盘）
    item_by_tap_zl = false    点按一下 ZL 是否放行（默认关）
    item_by_hold_zl = false   长按 ZL（另一种键位设置，默认关）
    item_hold_frames = 18     长按判定帧数（越小越容易触发）
    crouch_acts = 23,24,25,27,28,31,32,35,37   蹲下站定的 act_no
    crouch_walk_types = 2                       蹲着轻推摇杆走的 act_type
    crouch_walk_acts = 7                        蹲着轻推摇杆走的 act_no
    log_act = false           排障日志，平时关

============================================================
已知限制
============================================================
  - 只支持 MHGU v1.4.0（0100770008DD8000）；插件装 hook 前会核对函数头，
    版本不符只报错、不改游戏。
  - 蹲下满推会变成 2/6（和普通跑同号）；插件用"蹲模式"持续到停下，
    所以蹲下后满推跑到边缘也会换图。
  - 跳下用 act_type = 4 判定，可能包含少数空中/翻滚动作；目前固定开启。
  - 切道具不改变动作号，只能按输入判定；如果你的 L 还兼其它功能，
    按住时也会放行，不需要就把对应 item_by_* 改成 false。
  - 拦截后玩家可能继续走进原来的换区通道/墙边，游戏不会自动传送。

卸载
  删掉 MHGU_NO_AREA_MOVE 文件夹（实机就是删 0100770008DD8000 里对应的 plugins 文件）。
"""


def main():
    if not os.path.exists(PLUGIN):
        raise SystemExit("找不到插件：%s" % PLUGIN)
    readme = README.replace('%(install)s', mp.install_block(FOLDER).rstrip())
    mp.write_mod_zip(ZIP, FOLDER,
                     [(PLUGIN, "romfs/nativeNX/plugins/no_area_move.nro")],
                     readme, readme_name="说明.txt")
    mp.report(ZIP, "  no_area_move.nro  %6d B  %s" % (
        os.path.getsize(PLUGIN), mp.sha256(PLUGIN)[:16].upper()))


if __name__ == "__main__":
    main()
