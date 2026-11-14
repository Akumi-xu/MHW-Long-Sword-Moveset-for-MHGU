#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""打包 MHGU 怪物显隐 MOD（只含插件本体，加载器用 forge 前置包）。

结构：MHGU_MONSTER_HIDE\\{romfs, 安装说明.txt}
  * 模拟器：整个文件夹放到 %APPDATA%\\eden\\load\\<titleid>\\
  * 实机  ：把里面的 romfs 合并进 SD:\\atmosphere\\contents\\<titleid>\\

从 01_plugins/monster_hide/monster_hide.nro 现场组装。
用法：python 06_tools/patches/pack_monster_hide.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import modpack as mp  # noqa: E402

ROOT = mp.ROOT
MOD_NAME = 'MHGU_MONSTER_HIDE'
PLUGIN_NAME = 'monster_hide.nro'
PLUGIN_SRC = os.path.join(ROOT, '01_plugins', 'monster_hide', PLUGIN_NAME)
FIN = os.path.join(ROOT, '02_build', '成品MOD', '怪物显隐')
ZIP = os.path.join(FIN, 'MHGU怪物隐藏.zip')

README = '''MHGU 大型怪物显隐 MOD
============================================================
插件：%(plugin)s
游戏：怪物猎人 XX / Generations Ultimate（Switch），Title ID %(title)s，版本 1.4.0

============================================================
安装
============================================================
%(install)s
============================================================
操作
============================================================
    按住 L + 十字键左   隐藏 / 恢复第一只大型怪物
    按住 L + 十字键右   隐藏 / 恢复第二只大型怪物

隐藏后怪物完全停止更新：不再移动、不出招、没有攻击特效和音效。
再按一次恢复，怪物从原来的状态自然继续。

============================================================
兼容性自检
============================================================
  * 插件重定位全部落在可写 data 段，0 处写只读页
  * 无 inline hook，只做游戏内存读写
  * 加载器用 forge 前置包（03_loader\\hardware，实机 + 模拟器通用）

============================================================
注意
============================================================
1. 不同 MOD 的 plugins 目录会合并，同功能插件只能留一个。
2. 前置只装一次；换插件不用动前置。
'''


def main():
    assert os.path.isfile(PLUGIN_SRC), PLUGIN_SRC
    readme = (README.replace('%(plugin)s', PLUGIN_NAME)
                    .replace('%(title)s', mp.TITLE_ID)
                    .replace('%(install)s', mp.install_block(MOD_NAME).rstrip()))
    mp.write_mod_zip(ZIP, MOD_NAME,
                     [(PLUGIN_SRC, 'romfs/nativeNX/plugins/' + PLUGIN_NAME)],
                     readme)
    mp.report(ZIP)


if __name__ == '__main__':
    main()
