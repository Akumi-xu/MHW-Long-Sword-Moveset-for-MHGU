"""Apply the v1.2 World-style motion values to an original v1.2 w07.arc."""
import argparse
from pathlib import Path
import struct
import zlib
from tools.arc_tool import ENTRY_SIZE, HDR_SIZE, read_entries, unpack_one

# Private HDP row -> original physical motion value in the released v1.2 ARC.
# 100–103: Iai opening strikes; 107–109: Helm hits 1–6; 110–112: Iai
# delayed hits; 113–115: Helm hit 7 with its original part modifier.
BASE_VALUES = {
    100: 17, 101: 19, 102: 31, 103: 39,
    107: 10, 108: 15, 109: 25,
    110: 55, 111: 72, 112: 86,
    113: 10, 114: 15, 115: 25,
}
HDP_NAME = 'player/hit/pl_we07_hitdata'


def scale_arc(source, output):
    raw, version, members = read_entries(source)
    hdps = [member for member in members if member['name'].replace('\\', '/') == HDP_NAME]
    if version != 17 or len(hdps) != 1:
        raise ValueError('Expected the v1.2 Switch weapon ARC and one private HDP table')
    [hdp] = hdps
    original, codec = unpack_one(raw, hdp)
    if codec != 'zlib' or original[:4] != b'HDP\0' or len(original) != 16 + 116 * 56:
        raise ValueError('Unexpected HDP format or row extent')
    if struct.unpack_from('<I', original, 8)[0] != 116:
        raise ValueError('Unexpected HDP row count')

    updated = bytearray(original)
    changes = {}
    for row, old in BASE_VALUES.items():
        record = 16 + row * 56
        # Native loader copies an unsigned halfword from each source word.
        # Round fractional motion values to the nearest integer with .5 up.
        new = (3 * old + 1) // 2
        for offset in (4, 48):
            if struct.unpack_from('<I', original, record + offset)[0] != old:
                raise ValueError(f'HDP row {row} differs from the original v1.2 build')
            struct.pack_into('<I', updated, record + offset, new)
        changes[row] = [old, new]

    blob = zlib.compress(updated, 9)
    arc = bytearray(raw)
    arc.extend(bytes(-len(arc) % 16))
    offset = len(arc)
    arc.extend(blob)
    entry = HDR_SIZE + ENTRY_SIZE * hdp['index']
    struct.pack_into('<I', arc, entry + 68, len(blob))
    struct.pack_into('<I', arc, entry + 76, offset)
    with output.open('xb') as f:
        f.write(arc)

    result, result_version, after = read_entries(output)
    assert version == result_version and len(members) == len(after)
    for before, actual in zip(members, after):
        if before['index'] == hdp['index']:
            assert unpack_one(result, actual) == (updated, 'zlib')
        else:
            assert actual == before
            at, size = before['offset'], before['csize']
            assert result[at:at + size] == raw[at:at + size]
    return changes


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    for row, (old, new) in scale_arc(args.source, args.output).items():
        print(f'HDP {row}: {old} -> {new}')
