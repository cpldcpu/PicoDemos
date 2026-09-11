"""Unpack the original PowerPacker executable and extract its two text bitmaps.

This reads data only; it does not execute the downloaded Amiga binary.
Reference files are preserved. Generated assets contain no animation frames.
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]


def unpack(data):
    # The original's second hunk contains the backwards PP bit stream.
    packed = data[680:-4]
    position = len(packed) - 4
    bits = left = 0

    def read(n):
        nonlocal position, bits, left
        value = 0
        for _ in range(n):
            if not left:
                position -= 4
                assert position >= 0
                bits = int.from_bytes(packed[position:position + 4], 'big')
                left = 32
            value = (value << 1) | (bits & 1)
            bits >>= 1
            left -= 1
        return value

    result = bytearray(int.from_bytes(packed[-4:], 'big') >> 8)
    destination = len(result)
    read(packed[-1])
    while destination:
        if read(1) == 0:
            count = 1
            while True:
                n = read(2)
                count += n
                if n != 3:
                    break
            for _ in range(count):
                destination -= 1
                result[destination] = read(8)
            if not destination:
                break
        kind = read(2)
        n = (9, 10, 12, 13)[kind]
        count = kind + 2
        if kind == 3:
            if read(1) == 0:
                n = 7
            offset = read(n)
            while True:
                n = read(3)
                count += n
                if n != 7:
                    break
        else:
            offset = read(n)
        for _ in range(count):
            destination -= 1
            result[destination] = result[destination + offset + 1]
    assert len(result) == 41116 and result[:4] == bytes.fromhex('000003f3')
    return result


def hunks(data):
    # PowerPacker's compact relocation lists: word counts, byte deltas.
    position, index = 36, 0
    blocks, relocations = {}, []

    def read(n):
        nonlocal position
        value = int.from_bytes(data[position:position + n], 'big')
        position += n
        return value

    while position < len(data):
        position = (position + 3) & ~3
        kind = read(4)
        if kind in (0x3e9, 0x3ea, 0x3eb):
            size = read(4) * 4
            if kind != 0x3eb:
                blocks[index] = data[position:position + size]
                position += size
            index += 1
        elif kind == 0x3ec:
            while True:
                position = (position + 1) & ~1
                count = read(2)
                if not count:
                    break
                target, offset = read(2), read(4)
                offsets = [offset]
                for _ in range(count - 1):
                    delta = read(1)
                    if not delta:
                        delta = read(3)
                    offset += delta * 2
                    offsets.append(offset)
                relocations.append((index - 1, target, offsets))
        else:
            assert kind == 0x3f2, hex(kind)
    return blocks, relocations


def main():
    reference = ROOT / 'reference'
    raw = unpack((reference / 'STELLAR-DarkRoom').read_bytes())
    blocks, relocations = hunks(raw)
    (reference / 'unpacked.bin').write_bytes(raw)
    for number, block in blocks.items():
        (reference / f'hunk{number}.bin').write_bytes(block)
    (reference / 'relocations.json').write_text(json.dumps(relocations, indent=2) + '\n')
    arrays = {
        'original_title': blocks[0][0x9328:0x9328 + 1280],
        'original_credits': blocks[3][0x16c:],
        'original_copper': blocks[3][:0x16c],
        'original_module': (reference / 'darkroom.mod').read_bytes(),
    }
    with (ROOT / 'darkroom/assets.c').open('w', encoding='utf-8') as f:
        f.write('/* Original data: Dweezil and Strobo / Stellar, Darkroom (1994). */\n#include "assets.h"\n')
        for name, values in arrays.items():
            f.write(f'const unsigned char {name}[{len(values)}]={{\n')
            for offset in range(0, len(values), 24):
                f.write(','.join(str(x) for x in values[offset:offset + 24]) + ',\n')
            f.write('};\n')
    (ROOT / 'darkroom/assets.h').write_text('#pragma once\n' + ''.join(
        f'extern const unsigned char {name}[{len(values)}];\n' for name, values in arrays.items()))
    print('Extracted original title, credits, copper data and module.')


if __name__ == '__main__':
    main()
