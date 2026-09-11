"""Bind the release evidence to exact firmware, host, source, and media bytes."""
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

from board_run import audit, host_reference

ROOT = Path(__file__).resolve().parents[2]
UF2 = ROOT / "darkroom_vga_rp2350.uf2"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def uf2_flash_bytes(raw):
    assert len(raw) % 512 == 0
    seen = set()
    ignored = 0
    image_end = 0x10000000
    for offset in range(0, len(raw), 512):
        m0, m1, flags, address, length, number, total, family = struct.unpack_from(
            "<8I", raw, offset
        )
        assert (m0, m1) == (0x0A324655, 0x9E5D5157)
        assert struct.unpack_from("<I", raw, offset + 508)[0] == 0x0AB16F30
        if family == 0xE48BFF57:
            assert offset == 0 and flags == 0xA000 and address == 0x10FFFF00
            assert length == 256 and number == 0 and total == 2
            assert struct.unpack_from("<I", raw, offset + 32 + length)[0] == 0x9957E304
            ignored += 1
            continue
        assert family == 0xE48BFF59 and flags == 0x2000 and length == 256
        assert 0x10000000 <= address and address + length <= 0x10400000, "4 MiB flash budget"
        assert total == len(raw) // 512 - 1 and number not in seen
        seen.add(number)
        image_end = max(image_end, address + length)
    assert ignored == 1
    assert seen == set(range(len(raw) // 512 - 1))
    return image_end - 0x10000000


def symbol(map_text, name):
    match = re.search(
        r"(0x[0-9a-f]+)\s+(?:PROVIDE \()?" + re.escape(name) + r"\b", map_text
    )
    assert match, f"missing linker symbol {name}"
    return int(match.group(1), 16)


def main():
    raw = UF2.read_bytes()
    firmware_sha = hashlib.sha256(raw).hexdigest()
    flash_payload = uf2_flash_bytes(raw)
    host = ROOT / "darkroom" / "build_host" / "darkroom.exe"
    reference = host_reference(host)

    runs = []
    negative_controls = 0
    for name in ("release_01", "release_02"):
        stored = json.loads((ROOT / "validation" / f"{name}.json").read_text(encoding="utf-8"))
        lines = (ROOT / "validation" / f"{name}.log").read_text(encoding="utf-8").splitlines()
        checked = audit(lines, reference)
        snapshot = ROOT / "validation" / f"{name}_programmed.uf2"
        assert stored["passed"] and checked["passed"], checked["failures"]
        assert stored["uf2_sha256"] == firmware_sha == sha256(snapshot)
        assert stored["host_exe_sha256"] == sha256(host)
        assert stored["host_pcm_sha256"] == reference["pcm_sha256"]
        assert stored["host_final_hash"] == reference["final_hash"]
        assert stored["checked_audio_hashes"] == checked["checked_audio_hashes"] == 80
        assert stored["checked_visual_hashes"] == checked["checked_visual_hashes"] == 9

        audio_match = re.search(r"AHASH s=\d+ ([0-9a-fA-F]{8})", "\n".join(lines))
        visual_match = re.search(r"VHASH s=\d+ ([0-9a-fA-F]{8})", "\n".join(lines))
        assert audio_match and visual_match
        mutants = [
            lines[:-1],
            [line.replace(audio_match.group(1), "00000000", 1) for line in lines],
            [line.replace(visual_match.group(1), "00000000", 1) for line in lines],
            [line.replace("repeat 0", "repeat 1") for line in lines],
            [line.replace("missed 0", "missed 1") for line in lines],
        ]
        for mutant in mutants:
            assert not audit(mutant, reference)["passed"]
            negative_controls += 1

        runs.append({
            "name": name,
            "passed": True,
            "uf2_sha256": stored["uf2_sha256"],
            "checked_audio_hashes": checked["checked_audio_hashes"],
            "checked_visual_hashes": checked["checked_visual_hashes"],
            "worst_render_ms": checked["worst_render_ms"],
            "fps_min": checked["fps_min"],
            "frames": checked["frames"],
            "audio_position": checked["audio_position"],
            "counters": checked["counters"],
            "done": checked["done"],
            "final": checked["final"],
        })

    map_text = (ROOT / "darkroom" / "build_rp2350" / "darkroom.elf.map").read_text(encoding="utf-8")
    bss_end = symbol(map_text, "__bss_end__")
    heap_limit = symbol(map_text, "__HeapLimit")
    flash_from_map = symbol(map_text, "__flash_binary_end") - 0x10000000
    static_main_sram = bss_end - 0x20000000
    heap_headroom = heap_limit - bss_end
    assert flash_payload == flash_from_map
    assert flash_payload <= 4 * 1024 * 1024
    assert heap_headroom >= 64 * 1024, "less than 64 KiB before scanvideo runtime allocations"

    media = ROOT / "media" / "darkroom.mp4"
    probe = json.loads(subprocess.run(
        ["ffprobe", "-v", "error", "-show_format", "-show_streams", "-of", "json", str(media)],
        capture_output=True, text=True, check=True,
    ).stdout)
    video = next(stream for stream in probe["streams"] if stream["codec_type"] == "video")
    audio = next(stream for stream in probe["streams"] if stream["codec_type"] == "audio")
    assert (video["width"], video["height"], video["avg_frame_rate"], int(video["nb_frames"])) == (640, 480, "60/1", 4800)
    assert audio["channels"] == 2 and int(audio["sample_rate"]) == 24000
    assert abs(float(probe["format"]["duration"]) - 80.0) < 0.01

    source_paths = [
        path for path in sorted((ROOT / "darkroom").rglob("*"))
        if path.is_file()
        and not any(part.startswith("build") or part == "__pycache__" for part in path.relative_to(ROOT / "darkroom").parts)
        and path.suffix in (".c", ".h", ".py", ".txt")
    ] + [ROOT / "build.ps1", ROOT / "run_darkroom.bat"]
    source_hashes = {
        str(path.relative_to(ROOT)): sha256(path)
        for path in source_paths
    }

    result = {
        "uf2_sha256": firmware_sha,
        "flash_image_bytes": flash_payload,
        "uf2_bytes": len(raw),
        "static_main_sram_bytes": static_main_sram,
        "heap_before_scanvideo_bytes": heap_headroom,
        "separate_stack_bytes": 8192,
        "movie_bytes": media.stat().st_size,
        "movie_sha256": sha256(media),
        "movie_frames": 4800,
        "movie_duration_seconds": 80.0,
        "host_exe_sha256": sha256(host),
        "host_pcm_sha256": reference["pcm_sha256"],
        "host_final_hash": reference["final_hash"],
        "negative_controls_passed": negative_controls,
        "runs": runs,
        "source_sha256": source_hashes,
    }
    (ROOT / "validation" / "release.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps({key: value for key, value in result.items() if key not in ("runs", "source_sha256")}, indent=2))


if __name__ == "__main__":
    main()
