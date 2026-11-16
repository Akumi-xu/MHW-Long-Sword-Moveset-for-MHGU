#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""检查 ARM32 插件（ELF / NSO / NRO）能否在 Switch 实机上安全加载。

MHGU 的 SDK 在加载 NRO/NSO 时会按 MOD0 里的动态表做重定位。规则是：

* 只支持 ARM32 的 REL（不是 RELA/RELR）；
* 重定位目标必须落在**可写**的 data/BSS 里；
* 不能出现 DT_TEXTREL / DF_TEXTREL（那意味着加载器要写只读页）。

模拟器基本不管这些，实机会直接访存异常——这就是“模拟器能跑、实机崩”的常见原因。

本脚本改编自 forge PR #7（Lawrence-al）里的 tools/check_relocations.py，
补上了 NRO 的解析（forge 插件产出的是 NRO，不是 NSO）。

用法:
    python check_relocations.py ..\\02_build\\latest\\my_plugin.nro
    python check_relocations.py target\\armv7a-none-eabi\\release\\libmy_plugin.so
    python check_relocations.py *.nro
"""

import glob
import struct
import sys

# 段权限标记（与 PR 里一致）：5=R+X(text) 4=R(rodata) 6=R+W(data)
SEG_FLAGS = (5, 4, 6)


def check(image, segments, dynamic):
    """segments: [(addr, memsz, flags)], dynamic: 动态表在 image 中的偏移。"""
    tags = {}
    for pos in range(dynamic, len(image), 8):
        tag, value = struct.unpack_from('<II', image, pos)
        if tag == 0:
            break
        tags[tag] = value
    else:
        raise ValueError('动态表没有结束标记')

    # DT_TEXTREL 标签可能存在但值为 0（链接器留下的空标记），只有非 0 才真的要写只读页
    if tags.get(22, 0) != 0 or tags.get(30, 0) & 4:
        raise ValueError('DT_TEXTREL/DF_TEXTREL: 加载器要写只读页')
    if any(tags.get(tag, 0) for tag in (7, 8, 35, 36)):
        raise ValueError('使用了 RELA/RELR 重定位格式，加载器不支持')
    if tags.get(19, 8) != 8 or tags.get(20, 17) != 17:
        raise ValueError('不是 ARM32 的 REL 条目')

    # 游戏把 new/delete 换成了引擎堆，初始化阶段还不能用，导入就会崩
    symbol_count = struct.unpack_from('<I', image, tags[4] + 4)[0]
    for index in range(symbol_count):
        name_off, _, _, _, _, section = struct.unpack_from('<IIIBBH', image, tags[6] + 16 * index)
        if section:
            continue
        start = tags[5] + name_off
        name = bytes(image[start:image.index(0, start)]).decode('utf-8', 'replace')
        if name.startswith(('_Znw', '_Zna', '_Zdl', '_Zda')):
            raise ValueError('初始化阶段导入了 C++ 分配器: ' + name)

    counts = {'relative': 0, 'absolute': 0, 'global': 0, 'jump': 0}
    names = {23: 'relative', 2: 'absolute', 21: 'global', 22: 'jump'}
    for table_tag, size_tag in ((17, 18), (23, 2)):
        start, size = tags.get(table_tag, 0), tags.get(size_tag, 0)
        if not size:
            continue
        if size % 8 or start + size > len(image):
            raise ValueError('重定位表越界')
        for pos in range(start, start + size, 8):
            target, info = struct.unpack_from('<II', image, pos)
            kind = info & 255
            if kind == 0:
                continue
            if kind not in names:
                raise ValueError('不支持的 ARM 重定位类型 %d' % kind)
            if not any(flags & 2 and addr <= target and target + 4 <= addr + memsz
                       for addr, memsz, flags in segments):
                raise ValueError('重定位目标 0x%X 不在可写的 data/BSS 里' % target)
            counts[names[kind]] += 1

    return {
        'counts': counts,
        'total_writes': sum(counts.values()),
        'segments': segments,
    }


def _loads_to_image(elf, headers):
    loads = [h for h in headers if h[0] == 1]
    image = bytearray(max(h[2] + h[5] for h in loads))
    for _, off, addr, _, filesz, _, _, _ in loads:
        image[addr:addr + filesz] = elf[off:off + filesz]
    return image, loads


def check_elf(elf, headers):
    image, loads = _loads_to_image(elf, headers)
    dynamic = next(h[2] for h in headers if h[0] == 2)
    return check(image, [(h[2], h[5], h[6]) for h in loads], dynamic)


def check_nso(nso):
    if nso[:4] != b'NSO0' or struct.unpack_from('<I', nso, 12)[0] & 7:
        raise ValueError('不是未压缩的 NSO')
    loads = [struct.unpack_from('<III', nso, 0x10 + 16 * i) for i in range(3)]
    bss = struct.unpack_from('<I', nso, 0x3c)[0]
    image = bytearray(loads[2][1] + loads[2][2] + bss)
    for off, addr, size in loads:
        image[addr:addr + size] = nso[off:off + size]
    mod0 = struct.unpack_from('<I', image, 4)[0]
    if image[mod0:mod0 + 4] != b'MOD0':
        raise ValueError('没有 MOD0')
    dynamic = mod0 + struct.unpack_from('<i', image, mod0 + 4)[0]
    segments = [(addr, size + (bss if i == 2 else 0), flags)
                for i, ((_, addr, size), flags) in enumerate(zip(loads, SEG_FLAGS))]
    return check(image, segments, dynamic)


def check_nro(nro):
    """forge 插件产出的 NRO：0x10 处是 NRO0 头，前面 0x10 字节是 NroStart。"""
    if nro[0:4] == b'NRO0':
        hdr = 0
    elif nro[0x10:0x14] == b'NRO0':
        hdr = 0x10
    else:
        raise ValueError('不是 NRO0')
    segs = []
    for i in range(3):
        off = struct.unpack_from('<I', nro, hdr + 0x10 + i * 8)[0]
        size = struct.unpack_from('<I', nro, hdr + 0x14 + i * 8)[0]
        segs.append((off, size))
    bss = struct.unpack_from('<I', nro, hdr + 0x28)[0]

    # NRO 里段的文件偏移就等于它加载后的虚拟地址
    image = bytearray(nro)
    for off, size in segs:
        assert off + size <= len(nro), '段越界'
    if bss:
        image.extend(bytearray(bss))

    mod0_ptr = struct.unpack_from('<I', nro, 0x04)[0] if hdr == 0x10 else None
    mod0 = mod0_ptr if mod0_ptr else image.find(b'MOD0')
    if mod0 is None or image[mod0:mod0 + 4] != b'MOD0':
        raise ValueError('没有 MOD0（不是动态链接的 NRO？）')
    dynamic = mod0 + struct.unpack_from('<i', image, mod0 + 4)[0]

    segments = [(addr, size + (bss if i == 2 else 0), flags)
                for i, ((addr, size), flags) in enumerate(zip(segs, SEG_FLAGS))]
    return check(image, segments, dynamic)


def describe(path):
    data = open(path, 'rb').read()
    if data[:4] == b'\x7fELF':
        if data[4] != 1 or data[5] != 1:
            raise ValueError('不是 32 位小端 ELF')
        phoff = struct.unpack_from('<I', data, 28)[0]
        phentsize, phnum = struct.unpack_from('<HH', data, 42)
        headers = []
        for i in range(phnum):
            pos = phoff + i * phentsize
            p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = \
                struct.unpack_from('<8I', data, pos)
            headers.append((p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align))
        kind = 'ELF'
        result = check_elf(data, headers)
    elif data[:4] == b'NSO0':
        kind = 'NSO'
        result = check_nso(data)
    else:
        kind = 'NRO'
        result = check_nro(data)
    return kind, result


def main(argv):
    args = argv[1:]
    if not args:
        print(__doc__)
        return 1
    paths = []
    for a in args:
        hit = glob.glob(a)
        paths.extend(hit if hit else [a])

    failed = 0
    for p in paths:
        print('=' * 66)
        print('文件:', p)
        try:
            kind, r = describe(p)
        except (OSError, ValueError, IndexError, struct.error) as exc:
            failed += 1
            print('结果: FAIL — %s' % exc)
            continue
        c = r['counts']
        print('格式: %s' % kind)
        print('段  : ' + ', '.join('%s=0x%X(+%d)' % (n, a, s)
                                  for n, (a, s, _) in zip(('text', 'rodata', 'data'), r['segments'])))
        print('重定位: relative=%d absolute=%d global=%d jump=%d（共 %d 处写入）'
              % (c['relative'], c['absolute'], c['global'], c['jump'], r['total_writes']))
        print('结果: PASS — 无只读页重定位，实机可加载')
    print('=' * 66)
    if failed:
        print('有 %d 个文件不满足实机加载要求' % failed)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
