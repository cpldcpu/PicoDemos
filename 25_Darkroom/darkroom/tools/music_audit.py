#!/usr/bin/env python3
"""Audit Strobo's original Darkroom MOD and an optional native WAV capture."""
from __future__ import annotations

import argparse
import hashlib
import re
import struct
import wave
from pathlib import Path

AUDIO_RATE = 24_000
PAL_TICK_Q31 = 0x029067A6
ORDER_TICKS = 32 * (8 + 5)
F00_TICK = 8 * ORDER_TICKS + 32 * 8 + 31 * 5
EXPECTED_PCM_HASH = 0x1E1A9967


def sample_at_tick(tick: int) -> int:
    return tick * PAL_TICK_Q31 * AUDIO_RATE >> 31


def read_module(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) != 57_862 or data[1080:1084] != b"M.K.":
        raise SystemExit(f"FAIL {path}: expected the 57,862-byte four-channel MOD")
    return data


def audit_module(data: bytes) -> None:
    title = data[:20].rstrip(b"\0").decode("ascii", "replace")
    song_len = data[950]
    orders = list(data[952 : 952 + song_len])
    pattern_count = max(orders) + 1
    sample_bytes = sum(struct.unpack_from(">H", data, 20 + i * 30 + 22)[0] * 2 for i in range(31))
    expected_size = 1084 + pattern_count * 1024 + sample_bytes
    if expected_size != len(data):
        raise SystemExit(f"FAIL sample data ends at {expected_size}, file ends at {len(data)}")

    effects: dict[str, int] = {}
    f_rows: list[tuple[int, int, int, int]] = []
    delayed = 0
    for order, pattern in enumerate(orders):
        for row in range(64):
            for channel in range(4):
                at = 1084 + pattern * 1024 + row * 16 + channel * 4
                effect = data[at + 2] & 15
                param = data[at + 3]
                if effect or param:
                    key = f"{effect:X}"
                    if effect == 14:
                        key += f"{param >> 4:X}"
                    effects[key] = effects.get(key, 0) + 1
                if effect == 15:
                    f_rows.append((order, row, channel, param))
                if effect == 14 and param >> 4 == 13:
                    delayed += 1

    expected_orders = [2, 3, 4, 4, 0, 0, 1, 5, 6]
    if orders != expected_orders:
        raise SystemExit(f"FAIL order list {orders}, expected {expected_orders}")
    if set(effects) != {"A", "C", "E1", "ED", "F"}:
        raise SystemExit(f"FAIL effect set {sorted(effects)}, expected A C E1 ED F")
    if len(f_rows) != song_len * 64:
        raise SystemExit(f"FAIL expected one F command per played row, found {len(f_rows)}")
    for i, (order, row, _channel, param) in enumerate(f_rows):
        expected = 0 if i == len(f_rows) - 1 else (8 if row % 2 == 0 else 5)
        if param != expected:
            raise SystemExit(f"FAIL unexpected F{param:02X} at order {order}, row {row}")
    if f_rows[-1][:2] != (8, 63):
        raise SystemExit(f"FAIL F00 location is {f_rows[-1][:2]}, expected (8, 63)")

    for i in range(31):
        at = 20 + i * 30
        length = struct.unpack_from(">H", data, at + 22)[0] * 2
        loop = struct.unpack_from(">H", data, at + 26)[0] * 2
        repeat = struct.unpack_from(">H", data, at + 28)[0] * 2
        if repeat > 2 and loop + repeat > length:
            raise SystemExit(f"FAIL instrument {i + 1} loop exceeds sample")

    ideal_end = F00_TICK * AUDIO_RATE // 50
    pal_end = sample_at_tick(F00_TICK)
    print(f'PASS MOD "{title}" sha256={hashlib.sha256(data).hexdigest()}')
    print(f"  orders={orders} patterns={pattern_count} effects={effects} delayed_notes={delayed}")
    print(f"  PAL={2**31/PAL_TICK_Q31:.9f} Hz order={sample_at_tick(ORDER_TICKS)/AUDIO_RATE:.6f} s")
    print(f"  order4/6/8 samples={sample_at_tick(4*ORDER_TICKS)}/{sample_at_tick(6*ORDER_TICKS)}/{sample_at_tick(8*ORDER_TICKS)}")
    print(f"  F00 tick={F00_TICK} sample={pal_end} time={pal_end/AUDIO_RATE:.6f} s")
    print(f"  idealized-50-Hz F00={ideal_end/AUDIO_RATE:.6f} s (early by {(pal_end-ideal_end)/AUDIO_RATE:.6f} s)")


def audit_assets(path: Path, module: bytes) -> None:
    source = path.read_text(encoding="ascii")
    match = re.search(r"const unsigned char original_module\[57862\]=\{(.*?)\};", source, re.S)
    if not match:
        raise SystemExit(f"FAIL {path}: original_module array not found")
    embedded = bytes(map(int, re.findall(r"\d+", match.group(1))))
    if embedded != module:
        raise SystemExit(f"FAIL {path}: embedded module differs from the reference MOD")
    print(f"PASS assets.c embeds the reference MOD byte for byte ({len(embedded)} bytes)")


def audit_wav(path: Path) -> None:
    with wave.open(str(path), "rb") as wav:
        params = (wav.getframerate(), wav.getnchannels(), wav.getsampwidth())
        frames = wav.getnframes()
        pcm = wav.readframes(frames)
    if params != (AUDIO_RATE, 2, 2):
        raise SystemExit(f"FAIL {path}: expected 24 kHz, stereo, signed 16-bit PCM; got {params}")
    samples = struct.unpack(f"<{len(pcm)//2}h", pcm)
    audible = [i // 2 for i, value in enumerate(samples) if abs(value) > 8]
    audible_end = audible[-1] if audible else -1
    if audible_end > sample_at_tick(F00_TICK) + 32:
        raise SystemExit(f"FAIL {path}: signal continues past F00 ({audible_end})")
    hash_value = 2166136261
    for value in samples:
        hash_value = ((hash_value ^ (value & 0xFFFF)) * 16777619) & 0xFFFFFFFF
    if frames == 80 * AUDIO_RATE and hash_value != EXPECTED_PCM_HASH:
        raise SystemExit(
            f"FAIL {path}: PCM hash={hash_value:08x}, expected {EXPECTED_PCM_HASH:08x}"
        )
    print(f"PASS WAV {frames/AUDIO_RATE:.3f} s, >8-LSB signal through {audible_end/AUDIO_RATE:.6f} s, PCM hash={hash_value:08x}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mod", type=Path, required=True)
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--wav", type=Path)
    args = parser.parse_args()
    module = read_module(args.mod)
    audit_module(module)
    if args.assets:
        audit_assets(args.assets, module)
    if args.wav:
        audit_wav(args.wav)


if __name__ == "__main__":
    main()
