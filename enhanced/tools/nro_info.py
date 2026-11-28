#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""查看任天堂 NRO（Switch 插件/自制程序）的头部信息。

用法:
    python nro_info.py my_plugin.nro
    python nro_info.py ..\\02_build\\latest\\*.nro

输出 NRO 头、三个段的偏移/大小、bss、MOD0 动态段、build-id 与 SHA256。
forge 插件是动态链接的 NRO，打包后可用它确认产物正常。
"""

import glob
import hashlib
import os
import struct
import sys

PAGE = 0x1000


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def describe(path):
    data = open(path, 'rb').read()
    size = len(data)
    print('=' * 66)
    print('文件   :', os.path.abspath(path))
    print('大小   : {:,} 字节'.format(size))
    print('SHA256 :', hashlib.sha256(data).hexdigest())

    # 标准 NRO 前面有一段 0x10 字节的 NroStart（crt0 写），MOD0 偏移在其中。
    # 也有直接把 NRO0 放在 0 的变体，两种都认。
    if data[0:4] == b'NRO0':
        hdr = 0
        mod0_ptr = None
    elif len(data) > 0x14 and data[0x10:0x14] == b'NRO0':
        hdr = 0x10
        mod0_ptr = u32(data, 0x04)
    else:
        print('不是 NRO0（可能是 ELF 或未打包的镜像）')
        return

    nro_size = u32(data, hdr + 0x08)
    print('声明大小: {:,} 字节  (一致: {})'.format(nro_size, nro_size == size))
    print()
    print('  段        文件偏移      大小        页数')
    print('  --------  ----------    ----------  ----')
    for i, name in enumerate(('text', 'rodata', 'data')):
        off = u32(data, hdr + 0x10 + i * 8)
        sz = u32(data, hdr + 0x14 + i * 8)
        print('  {:<9} 0x{:08X}    {:<10}  {}'.format(name, off, sz, (sz + PAGE - 1) // PAGE))

    bss = u32(data, hdr + 0x28)
    print('  {:<9} {:<13} {:<10}  {}'.format('bss', '-', bss, (bss + PAGE - 1) // PAGE))

    build_id = data[hdr + 0x30:hdr + 0x50].hex()
    if build_id.strip('0'):
        print()
        print('build-id :', build_id)

    if mod0_ptr:
        print('MOD0 偏移: 0x{:X}'.format(mod0_ptr))
        if data[mod0_ptr:mod0_ptr + 4] == b'MOD0':
            print('动态段   : 存在（dynamic-linking NRO，forge 插件的正常形态）')
        else:
            print('警告: 该偏移处不是 MOD0 魔数')
    else:
        at = data.find(b'MOD0')
        if at >= 0:
            print('MOD0 位置: 0x{:X}（无 NroStart 头的变体）'.format(at))

    marks = [b'forge_onLoad', b'forge_onUpdate', b'forge_onImGuiInit',
             b'forge_onImGuiRender', b'forge_onImGuiFreeRender']
    found = [m.decode() for m in marks if m in data]
    print('导出标记 :', ', '.join(found) if found else '(未检出，可能已被 strip 或不是 forge 插件)')


def main(argv):
    args = argv[1:]
    if not args:
        print(__doc__)
        return 1
    paths = []
    for a in args:
        hit = glob.glob(a)
        paths.extend(hit if hit else [a])
    for p in paths:
        if not os.path.isfile(p):
            print('跳过（不存在）:', p)
            continue
        describe(p)
    print('=' * 66)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
