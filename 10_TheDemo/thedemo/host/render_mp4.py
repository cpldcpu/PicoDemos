import os
import subprocess
import glob
import time
import sys
import shutil
from pathlib import Path

# Paths
HERE = Path(__file__).resolve().parent
SCREENSHOTS_DIR = HERE / "screenshots"
EXE = HERE / "thedemo.exe"
AUDIO_FILE = HERE.parent / "assets" / "music.mp3"
ARTIFACTS_DIR = Path(r"C:\Users\tboes\.gemini\antigravity\brain\4be338a2-9396-4223-aea3-d77672d0a91a\artifacts")
OUTPUT_MP4_LOCAL = HERE / "thedemo_60fps.mp4"
OUTPUT_MP4_ARTIFACT = ARTIFACTS_DIR / "thedemo_60fps.mp4"

TOTAL_FRAMES = 16441 # 274000 ms / (1000 / 60) ms per frame = 16440.00 frames

def run_cmd(cmd, cwd=None):
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
    if res.returncode != 0:
        print(f"\nERROR: Command failed!\nSTDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}")
        return False
    return True

def clean_screenshots():
    print("Clearing old frames...")
    if SCREENSHOTS_DIR.exists():
        files = glob.glob(str(SCREENSHOTS_DIR / "frame_*.bmp"))
        for f in files:
            try:
                os.remove(f)
            except Exception:
                pass

def main():
    print("=== SLOP / TheDemo MP4 Offline Renderer (60 FPS) ===")
    
    # 1. Compile the latest code
    print("\n[1/5] Rebuilding host binary...")
    if not run_cmd("make", cwd=str(HERE)):
        print("Rebuild failed!")
        sys.exit(1)
    
    # 2. Clean out old BMP frames
    clean_screenshots()
    
    # 3. Start offline rendering
    print("\n[2/5] Starting offline 60fps frame dumping (takes ~2.5 mins)...")
    cmd = [str(EXE), "--offline"]
    
    # Force taskkill on any active instances
    subprocess.run("taskkill /F /IM thedemo.exe >nul 2>&1", shell=True)
    
    process = subprocess.Popen(cmd, cwd=str(HERE), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    start_time = time.time()
    try:
        while process.poll() is None:
            # Count BMPs
            bmps = glob.glob(str(SCREENSHOTS_DIR / "frame_*.bmp"))
            count = len(bmps)
            pct = min(100.0, (count / TOTAL_FRAMES) * 100.0)
            
            # Print a neat progress bar
            bar_len = 30
            filled_len = int(bar_len * pct / 100.0)
            bar = '=' * filled_len + '>' + ' ' * (bar_len - filled_len - 1)
            
            elapsed = time.time() - start_time
            fps = count / elapsed if elapsed > 0 else 0
            eta = (TOTAL_FRAMES - count) / fps if fps > 0 else 0
            
            sys.stdout.write(f"\rRendering: [{bar[:bar_len]}] {pct:.1f}% ({count}/{TOTAL_FRAMES} frames) | Speed: {fps:.1f} fps | ETA: {eta:.0f}s  ")
            sys.stdout.flush()
            time.sleep(1.0)
    except KeyboardInterrupt:
        print("\nProcess interrupted by user!")
        process.terminate()
        sys.exit(1)
        
    print(f"\nRender completed successfully in {time.time() - start_time:.1f} seconds.")
    
    # 4. Invoke FFmpeg to assemble the video with audio
    print("\n[3/5] Compiling 60fps high-definition MP4 using FFmpeg...")
    # Scale 4x nearest-neighbor from 320x240 to 1280x960 to keep pixel-art sharp
    ffmpeg_cmd = (
        f'ffmpeg -y -framerate 60 -i "screenshots/frame_%05d.bmp" '
        f'-i "{AUDIO_FILE}" '
        f'-vf "scale=1280:960:flags=neighbor" '
        f'-c:v libx264 -pix_fmt yuv420p -b:v 8000k -c:a aac -b:a 192k -shortest '
        f'"{OUTPUT_MP4_LOCAL}"'
    )
    print(f"Running: {ffmpeg_cmd}")
    if not run_cmd(ffmpeg_cmd, cwd=str(HERE)):
        print("FFmpeg compile failed!")
        sys.exit(1)
        
    # 5. Save the output to artifacts
    print("\n[4/5] Copying video output to artifacts folder...")
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copy2(str(OUTPUT_MP4_LOCAL), str(OUTPUT_MP4_ARTIFACT))
    print(f"SUCCESS: Saved high-fidelity MP4 directly to:\n -> {OUTPUT_MP4_ARTIFACT}")
    
    # 6. Clean up BMP files to save space
    print("\n[5/5] Cleaning up temporary BMP frames...")
    clean_screenshots()
    
    print("\n=== Offline Render Done! ===")
    print("Enjoy your premium 60fps high-definition MP4 demo video!")

if __name__ == "__main__":
    main()
