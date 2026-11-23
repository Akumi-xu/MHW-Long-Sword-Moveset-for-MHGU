#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MHGU (MT Framework) .arc 资源包查看 / 解包工具。

格式（MHGU / MT Framework，版本 17）
------------------------------------
文件头 0x0C 字节:
    0x00  char[4]  "ARC\0"
    0x04  u16      版本 (MHGU 为 17)
    0x06  u16      条目数
    0x08  u32      保留 (0)
条目表紧接在 0x0C，每条 0x50 字节:
    0x00  char[0x40] 资源路径（'\' 或 '/' 分隔，NUL 结尾）
    0x40  u32        名称 hash
    0x44  u32        压缩后大小
    0x48  u32        解压后大小 | 标志位（低 30 位是大小，高位为标志，实测 0x40000000）
    0x4C  u32        数据在文件中的绝对偏移
数据区紧随条目表，单条数据一般是 zlib 流，解压失败则按原样写出。

用法
----
    python arc_tool.py list    romfs\\nativeNX\\arc\\weapon\\w07.arc
    python arc_tool.py list    w07.arc --filter pel
    python arc_tool.py unpack  w07.arc  out\\w07
    python arc_tool.py unpack  w07.arc  out\\w07  --filter effect/

注意: 只做读取。改完资源后想重新打包，请保留原始条目表另写脚本，
不要直接用“同名文件”覆盖游戏原版 ARC。
"""

import argparse
import os
import struct
import sys
import zlib

HDR_SIZE = 0x0C
ENTRY_SIZE = 0x50
NAME_SIZE = 0x40


def read_entries(path):
    data = open(path, 'rb').read()
    if len(data) < HDR_SIZE or data[:4] != b'ARC\0':
        raise ValueError('不是 ARC 文件 (magic=%r)' % data[:4])
    version, count = struct.unpack_from('<HH', data, 4)
    table = HDR_SIZE
    if table + count * ENTRY_SIZE > len(data):
        raise ValueError('条目表超出文件大小 (count=%d)' % count)

    entries = []
    for i in range(count):
        off = table + i * ENTRY_SIZE
        raw = data[off:off + NAME_SIZE].split(b'\0')[0]
        name = raw.decode('utf-8', 'replace')
        h, csize, sz, foff = struct.unpack_from('<4I', data, off + NAME_SIZE)
        entries.append({
            'index': i,
            'name': name,
            'hash': h,
            'csize': csize,
            'dsize': sz & 0x3FFFFFFF,
            'flags': sz & 0xC0000000,
            'offset': foff,
        })
    return data, version, entries


def unpack_one(data, ent):
    blob = data[ent['offset']:ent['offset'] + ent['csize']]
    try:
        return zlib.decompress(blob), 'zlib'
    except zlib.error:
        return blob, 'raw'


def cmd_list(args):
    data, version, entries = read_entries(args.arc)
    sel = entries
    if args.filter:
        sel = [e for e in entries if args.filter.lower() in e['name'].lower()]
    print('文件   :', os.path.abspath(args.arc))
    print('大小   : {:,} 字节'.format(len(data)))
    print('版本   : {}    条目: {}    匹配: {}'.format(version, len(entries), len(sel)))
    print()
    print('  {:>4}  {:<52} {:>10} {:>10} {:>10}'.format('#', '路径', '压缩', '解压', '偏移'))
    print('  ' + '-' * 92)
    for e in sel:
        print('  {:>4}  {:<52} {:>10} {:>10} 0x{:08X}'.format(
            e['index'], e['name'], e['csize'], e['dsize'], e['offset']))
    return 0


def cmd_unpack(args):
    data, version, entries = read_entries(args.arc)
    sel = entries
    if args.filter:
        sel = [e for e in entries if args.filter.lower() in e['name'].lower()]
    if not sel:
        print('没有匹配的条目')
        return 1

    ok = 0
    for e in sel:
        if e['csize'] == 0 or e['offset'] + e['csize'] > len(data):
            print('  跳过(越界) ', e['name'])
            continue
        body, how = unpack_one(data, e)
        dst = os.path.join(args.outdir, *e['name'].replace('\\', '/').split('/'))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, 'wb') as f:
            f.write(body)
        ok += 1
        note = ''
        if how == 'zlib' and e['dsize'] and len(body) != e['dsize']:
            note = '  (大小不符，期望 %d)' % e['dsize']
        print('  {:<52} {:<4} {:>9} -> {:>9}{}'.format(e['name'], how, e['csize'], len(body), note))
    print()
    print('解出 {} 个文件到 {}'.format(ok, os.path.abspath(args.outdir)))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description='MHGU .arc 查看/解包')
    sub = ap.add_subparsers(dest='cmd')

    p = sub.add_parser('list', help='列出条目')
    p.add_argument('arc')
    p.add_argument('--filter', help='只显示路径包含该关键字的条目')
    p.set_defaults(func=cmd_list)

    p = sub.add_parser('unpack', help='解包到目录')
    p.add_argument('arc')
    p.add_argument('outdir')
    p.add_argument('--filter', help='只解包含该关键字的条目')
    p.set_defaults(func=cmd_unpack)

    args = ap.parse_args(argv)
    if not args.cmd:
        ap.print_help()
        return 1
    try:
        return args.func(args)
    except (OSError, ValueError) as exc:
        print('错误:', exc)
        return 1


if __name__ == '__main__':
    sys.exit(main())
