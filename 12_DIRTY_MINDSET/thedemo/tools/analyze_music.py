import os
import sys
import numpy as np
import librosa

def main():
    mp3_path = "assets/Green-Phosphor Prayer.mp3"
    if not os.path.exists(mp3_path):
        print(f"Error: {mp3_path} not found")
        sys.exit(1)

    print(f"Loading {mp3_path}...")
    # Load audio, resample to 22050Hz for consistency
    y, sr = librosa.load(mp3_path, sr=22050)
    duration = librosa.get_duration(y=y, sr=sr)
    print(f"Loaded {len(y)} samples at {sr} Hz. Duration: {duration:.2f} s ({int(duration * 1000)} ms)")

    # 1. Beat Tracking
    print("Tracking beats...")
    tempo, beat_frames = librosa.beat.beat_track(y=y, sr=sr)
    # tempo is an array or float depending on librosa version, let's handle both
    if isinstance(tempo, np.ndarray):
        tempo = tempo[0]
    beat_times = librosa.frames_to_time(beat_frames, sr=sr)
    beat_ms = [int(t * 1000) for t in beat_times]

    print(f"Estimated Tempo: {tempo:.2f} BPM")
    print(f"Total Beats Tracked: {len(beat_ms)}")
    if len(beat_ms) > 0:
        avg_interval = np.mean(np.diff(beat_ms))
        print(f"Average Beat Interval: {avg_interval:.1f} ms (corresponds to {60000 / avg_interval:.2f} BPM)")

    # 2. Structural/Energy Analysis to find major shifts
    # We compute root-mean-square (RMS) energy for 1-second windows to trace energy transitions
    frame_length = 2048
    hop_length = 512
    rms = librosa.feature.rms(y=y, frame_length=frame_length, hop_length=hop_length)[0]
    rms_times = librosa.frames_to_time(range(len(rms)), sr=sr, hop_length=hop_length)

    # Let's print out some major beat indices and timestamps
    print("\nFirst 20 beats:")
    for idx, ms in enumerate(beat_ms[:20]):
        print(f"  Beat {idx+1}: {ms} ms ({ms/1000:.2f} s)")

    # Let's write a file beat_times.txt with all beat times
    with open("tools/beat_times.txt", "w") as f:
        for idx, ms in enumerate(beat_ms):
            f.write(f"Beat {idx+1}: {ms} ms\n")
    print("Wrote all beat times to tools/beat_times.txt")

if __name__ == "__main__":
    main()
