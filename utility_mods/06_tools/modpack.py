#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MOD 打包公共件。

约定（2026-10-31 起）：**一个 MOD 包只放插件本体**。
加载器 `exefs/` + `romfs/nativeNX/forge/` 单独打成 forge 前置包
（`02_build/成品MOD/forge前置/forge前置.zip`，由 `patches/pack_forge.py` 生成），
所有插件包共用它，不再每个包重复一遍 1.7 MB 的 subsdk9。
"""

import hashlib
import os
import zipfile

TITLE_ID = '0100770008DD8000'
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIN = os.path.join(ROOT, '02_build', '成品MOD')
LOADER = os.path.join(ROOT, '03_loader', 'hardware')

FORGE_CAT = 'forge前置'
FORGE_FOLDER = 'MHGU_FORGE_LOADER'
FORGE_ZIP = os.path.join(FIN, FORGE_CAT, 'forge前置.zip')


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def install_block(mod_name):
    """所有 MOD 说明共用的安装段落（前置 + 本体）。"""
    return """本包只含插件本体，不含加载器。
加载器（forge 前置）是单独一个包，先装一次：
    forge前置.zip（和本包一起下载的那个包）
（已经装过别的 forge MOD 或 forge 前置的话，跳过这一步。）

【模拟器 Eden】
    1) 前置：把 "MHGU_FORGE_LOADER" 文件夹放进
           %APPDATA%\\eden\\load\\0100770008DD8000\\
    2) 本插件：把 "{mod}" 文件夹也放进同一个 load 目录
           %APPDATA%\\eden\\load\\0100770008DD8000\\
       Eden 会合并两个 MOD：前置提供 exefs，本包提供 plugins。
    3) 启动 Eden -> 右键游戏 -> 属性 -> MOD，确认两个都已勾选。

【NS 实机（大气层）】
    所有内容合并进同一个 title id 目录：
        SD:\\atmosphere\\contents\\0100770008DD8000\\
    1) 前置：把前置包里的 exefs\\ 和 romfs\\ 复制进去
    2) 本插件：把本包里的 romfs\\ 也复制进去（和前置的 romfs 合并）
    最终应该能看到：
        ...\\exefs\\subsdk9
        ...\\romfs\\nativeNX\\forge\\config.ini
        ...\\romfs\\nativeNX\\plugins\\<插件>.nro
    重启机器后，SD 卡根目录会生成 forge-loader.log。

卸载：删掉对应的 plugins\\<插件>.nro 就行；前置留着给别的插件用。
""".replace('{mod}', mod_name)


def write_mod_zip(out_zip, folder, entries, readme, readme_name='安装说明.txt'):
    """打一个 MOD 包。

    out_zip : 目标 zip
    folder  : 解压后最外层的 MOD 文件夹名（Eden 的 MOD 名）
    entries : [(源文件路径或 bytes, 包内相对路径)]，相对路径用 / 分隔
    readme  : 说明文本（UTF-8，自动换行成 CRLF）
    """
    os.makedirs(os.path.dirname(out_zip), exist_ok=True)
    tmp = out_zip + '.tmp'
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as z:
        for src, rel in entries:
            arc = folder + '/' + rel.replace('\\', '/')
            if isinstance(src, (bytes, bytearray)):
                z.writestr(arc, bytes(src))
            else:
                z.write(src, arc)
        if readme:
            z.writestr(folder + '/' + readme_name,
                       readme.replace('\n', '\r\n').encode('utf-8'))
    if os.path.exists(out_zip):
        os.remove(out_zip)
    os.replace(tmp, out_zip)
    return out_zip


def report(out_zip, extra=None):
    data = open(out_zip, 'rb').read()
    print('zip    :', os.path.relpath(out_zip, ROOT))
    print('size   : %.1f KB' % (len(data) / 1024.0))
    print('sha256 : %s' % sha256_bytes(data)[:16].upper())
    if extra:
        print(extra)
