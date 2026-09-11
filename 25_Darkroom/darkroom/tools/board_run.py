"""Flash Darkroom, capture USB telemetry, and audit it against this build.

The reference is generated from the current desktop executable on every run;
the script contains no blessed image or PCM checksums. A development capture
collects timing evidence without presenting a partial run as validation.
"""
import argparse
import datetime
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parents[2]
SAMPLE_RATE = 24_000
DURATION_SECONDS = 80
DURATION_SAMPLES = SAMPLE_RATE * DURATION_SECONDS


def host_reference(exe):
    picture = subprocess.run(
        [str(exe), "--vhash"], capture_output=True, text=True, check=True
    ).stdout
    visual = {
        sample: value.lower()
        for sample, value in re.findall(
            r"VHASH s=(\d+) ([0-9a-fA-F]{8})", picture
        )
    }
    expected_samples = {str(second * SAMPLE_RATE) for second in range(0, 81, 10)}
    if set(visual) != expected_samples:
        raise RuntimeError("host did not produce the nine 0..80 second VHASH values")

    with tempfile.TemporaryDirectory(prefix="darkroom-reference-") as folder:
        wav = Path(folder) / "darkroom.wav"
        subprocess.run([str(exe), "--wav", str(wav)], check=True)
        pcm = wav.read_bytes()[44:]
    if len(pcm) != DURATION_SAMPLES * 4:
        raise RuntimeError(f"host WAV has {len(pcm)} PCM bytes, expected {DURATION_SAMPLES * 4}")

    value = 2166136261
    audio = {}
    words = struct.iter_unpack("<H", pcm)
    for frame in range(1, DURATION_SAMPLES + 1):
        value = ((value ^ next(words)[0]) * 16777619) & 0xFFFFFFFF
        value = ((value ^ next(words)[0]) * 16777619) & 0xFFFFFFFF
        if frame % SAMPLE_RATE == 0:
            audio[str(frame)] = f"{value:08x}"
    return {
        "visual_hashes": visual,
        "audio_hashes": audio,
        "final_hash": f"{value:08x}",
        "pcm_sha256": hashlib.sha256(pcm).hexdigest(),
    }


def observed(lines):
    body = "\n".join(lines)
    counters = {
        name: max([int(value) for value in re.findall(r"\b" + name + r" (\d+)", body)] or [-1])
        for name in ("repeat", "over", "under", "missed")
    }
    fps = [float(value) for value in re.findall(r"\| fps ([\d.]+)", body)]
    renders = [float(value) for value in re.findall(r"worst render ([\d.]+) ms", body)]
    if not renders:
        renders = [float(value) for value in re.findall(r"render ms [\d.]+/[\d.]+/([\d.]+)", body)]
    return {
        "counters": counters,
        "fps_windows": len(fps),
        "fps_min": min(fps) if fps else None,
        "worst_render_ms": max(renders) if renders else None,
    }


def audit(lines, reference):
    body = "\n".join(lines)
    failures = []

    def require(condition, reason):
        if not condition:
            failures.append(reason)

    require("BOOT DARKROOM" in body, "missing BOOT")
    require("sys=300000000 Hz" in body, "firmware did not report a 300 MHz system clock")
    require("sample_rate=24000" in body, "firmware did not report 24 kHz audio")
    require("duration=1920000" in body, "firmware did not report an 80 second duration")
    require("engine=native-software-blitter" in body, "missing renderer implementation identity")

    done = [line for line in lines if line.startswith("DONE ")]
    final = [line for line in lines if line.startswith("FINAL ")]
    require(len(done) == 1 and len(final) == 1, "not exactly one complete film")

    visual = {
        sample: value.lower()
        for sample, value in re.findall(r"VHASH s=(\d+) ([0-9a-fA-F]{8})", body)
    }
    require(visual == reference["visual_hashes"], "hardware VHASH values differ from the current host build")
    audio = {
        sample: value.lower()
        for sample, value in re.findall(r"AHASH s=(\d+) ([0-9a-fA-F]{8})", body)
    }
    require(audio == reference["audio_hashes"], "missing, extra, or mismatching per-second PCM hashes")
    require(
        f"ENDHASH s={DURATION_SAMPLES} {reference['final_hash']}" in body,
        "whole-score PCM hash differs from the current host build",
    )

    metrics = observed(lines)
    for name, value in metrics["counters"].items():
        require(value == 0, f"{name}={value}")
    require(metrics["worst_render_ms"] is not None, "missing render timing")
    require(metrics["worst_render_ms"] is not None and metrics["worst_render_ms"] < 16.0,
            "render reaches or exceeds the 16 ms budget")
    require(metrics["fps_windows"] >= 78, "missing per-second timing windows")
    require(metrics["fps_min"] is not None and metrics["fps_min"] >= 59.5,
            "frame-rate window below 59.5 fps")

    frames_match = re.search(r"DONE frames (\d+)", body)
    frames = int(frames_match.group(1)) if frames_match else 0
    require(4700 <= frames <= 4900, f"unexpected whole-film frame count {frames}")
    audio_match = re.search(r"DONE .*\| audio (\d+)", body)
    played = int(audio_match.group(1)) if audio_match else 0
    # Core 0 observes the DMA clock at frame boundaries. One 60 Hz frame plus
    # the primed ring can put the final observation modestly past the score.
    require(DURATION_SAMPLES <= played <= DURATION_SAMPLES + 1024,
            f"unexpected final audio position {played}")

    return {
        "passed": not failures,
        "complete": len(done) == 1 and len(final) == 1,
        "failures": failures,
        "checked_audio_hashes": len(audio),
        "checked_visual_hashes": len(visual),
        "frames": frames,
        "audio_position": played,
        **metrics,
        "done": done,
        "final": final,
    }


def capture(port_name, uf2_snapshot, name, timeout, development_seconds):
    subprocess.run(["picotool", "load", "-F", "-v", str(uf2_snapshot)], check=True)
    info = subprocess.run(
        ["picotool", "info", "-a"], capture_output=True, text=True, check=True
    ).stdout
    (ROOT / "validation" / f"{name}_device.txt").write_text(info, encoding="utf-8")
    subprocess.run(["picotool", "reboot"], check=True)

    connect_deadline = time.monotonic() + 60
    connection = None
    while time.monotonic() < connect_deadline:
        try:
            connection = serial.Serial(port_name, 115200, timeout=0.2)
            break
        except serial.SerialException:
            time.sleep(0.1)
    if connection is None:
        raise RuntimeError("USB CDC did not enumerate")

    lines = []
    pending = b""
    deadline = time.monotonic() + timeout
    development_deadline = None
    log_path = ROOT / "validation" / f"{name}.log"
    with connection, log_path.open("w", encoding="utf-8") as log:
        while time.monotonic() < deadline:
            pending += connection.read(4096)
            while b"\n" in pending:
                raw, pending = pending.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").strip()
                if not line:
                    continue
                lines.append(line)
                log.write(line + "\n")
                log.flush()
                print(line, flush=True)
                if development_seconds and line.startswith("T ") and development_deadline is None:
                    development_deadline = time.monotonic() + development_seconds
            if any(line.startswith("FINAL ") for line in lines[-2:]):
                break
            if development_deadline is not None and time.monotonic() >= development_deadline:
                break
    return lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM10")
    parser.add_argument("--uf2", type=Path, default=ROOT / "darkroom_vga_rp2350.uf2")
    parser.add_argument("--host", type=Path,
                        default=ROOT / "darkroom" / "build_host" / "darkroom.exe")
    parser.add_argument("--name", default="run_01")
    parser.add_argument("--replay", type=Path)
    parser.add_argument("--timeout", type=float, default=240.0)
    parser.add_argument("--development-seconds", type=float, default=0.0,
                        help="capture this many live seconds, then report development metrics only")
    args = parser.parse_args()

    if not args.uf2.is_file():
        raise FileNotFoundError(f"missing UF2: {args.uf2}")
    if not args.host.is_file():
        raise FileNotFoundError(f"missing host player: {args.host}; run build.ps1 -Target host")
    host_exe_bytes = args.host.read_bytes()
    host_exe_sha256 = hashlib.sha256(host_exe_bytes).hexdigest()
    reference = host_reference(args.host)
    if args.host.read_bytes() != host_exe_bytes:
        raise RuntimeError("host executable changed while its reference was being generated")
    stamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    uf2_bytes = args.uf2.read_bytes()
    uf2_sha256 = hashlib.sha256(uf2_bytes).hexdigest()
    programmed_snapshot = None
    if args.replay:
        lines = args.replay.read_text(encoding="utf-8").splitlines()
    else:
        # The build output can be replaced by another build during an 80-second
        # capture. Flash and retain this immutable snapshot, and bind the report
        # to its bytes rather than hashing the mutable source path afterward.
        programmed_snapshot = ROOT / "validation" / f"{args.name}_programmed.uf2"
        programmed_snapshot.write_bytes(uf2_bytes)
        lines = capture(args.port, programmed_snapshot, args.name, args.timeout,
                        args.development_seconds)

    if args.development_seconds:
        body = "\n".join(lines)
        visual = dict(re.findall(r"VHASH s=(\d+) ([0-9a-fA-F]{8})", body))
        result = {
            "passed": None,
            "complete": any(line.startswith("FINAL ") for line in lines),
            "development_capture": True,
            "note": "Development capture; this is not a validation pass.",
            "visual_hashes_seen": len(visual),
            "visual_hashes_match": {
                sample: value.lower() == reference["visual_hashes"].get(sample)
                for sample, value in visual.items()
            },
            **observed(lines),
        }
    else:
        result = audit(lines, reference)
    result.update(
        utc=stamp,
        uf2_sha256=uf2_sha256,
        programmed_snapshot=str(programmed_snapshot) if programmed_snapshot else None,
        host_exe_sha256=host_exe_sha256,
        host_pcm_sha256=reference["pcm_sha256"],
        host_final_hash=reference["final_hash"],
        port=args.port,
    )
    output = ROOT / "validation" / f"{args.name}.json"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2), flush=True)
    if result["passed"] is False:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
