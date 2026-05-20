import struct
import math
import subprocess
import numpy as np

def analyze():
    mp3_path = r"d:\Toyprojects\21_FlashDemo\thedemo\assets\VOLTAGE CONNECTING.mp3"
    sample_rate = 22050
    
    # Run ffmpeg to decode MP3 to raw s16le mono PCM on stdout
    cmd = [
        "ffmpeg", "-y", "-i", mp3_path,
        "-f", "s16le", "-ac", "1", "-ar", str(sample_rate), "-"
    ]
    
    print(f"Decoding and analyzing {mp3_path}...")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    raw_data, _ = proc.communicate()
    
    bytes_per_sample = 2
    num_samples = len(raw_data) // bytes_per_sample
    print(f"Total samples: {num_samples} ({num_samples / sample_rate:.2f} seconds)")
    
    # Unpack 16-bit signed integers
    samples = struct.unpack(f"<{num_samples}h", raw_data)
    
    # Convert to float array normalized to [-1.0, 1.0]
    samples = np.array(samples, dtype=np.float32) / 32768.0
    
    # Analyze in 1-second windows
    window_sec = 1.0
    window_size = int(sample_rate * window_sec)
    
    print("\n--- Audio Energy Profile (RMS) by Second ---")
    print(f"{'Time':<8} | {'Energy (RMS)':<12} | {'Visual Peak Indicator'}")
    print("-" * 50)
    
    max_rms = 0.0
    results = []
    
    for i in range(0, num_samples, window_size):
        chunk = samples[i:i+window_size]
        if len(chunk) == 0:
            break
        rms = math.sqrt(np.mean(chunk**2))
        max_rms = max(max_rms, rms)
        results.append((i / sample_rate, rms))
        
    for t, rms in results:
        bar_len = int((rms / max_rms) * 35)
        bar = "#" * bar_len
        print(f"{t:05.1f}s    | {rms:0.4f}       | {bar}")

if __name__ == "__main__":
    analyze()
