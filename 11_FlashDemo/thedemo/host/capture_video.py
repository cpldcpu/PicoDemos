import os
import subprocess
import glob
import shutil
import sys

# Paths
ARTIFACTS_DIR = r"C:\Users\tboes\.gemini\antigravity\brain\4be338a2-9396-4223-aea3-d77672d0a91a\artifacts"

def run_cmd(cmd, cwd=None):
    print(f"Running: {cmd}")
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
    if res.returncode != 0:
        print(f"ERROR: Command failed!\nSTDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}")
        return False
    return True

def compile_effect_gif(name, start_ms, duration_ms):
    print(f"\n--- Compiling video for {name} ({start_ms} ms to {start_ms+duration_ms} ms) ---")
    
    # 1. Clean out existing screenshots
    os.makedirs("screenshots", exist_ok=True)
    for f in glob.glob("screenshots/*.bmp") + glob.glob("screenshots/*.png"):
        try:
            os.remove(f)
        except Exception:
            pass

    # 2. Run the demo in frame-dumping mode with instant fast-forward
    cmd = f"thedemo.exe --start-ms {start_ms} --dump-frames {start_ms} {duration_ms}"
    if not run_cmd(cmd):
        return False

    # Check if frames were captured
    frames = glob.glob("screenshots/screenshot_*.bmp")
    if not frames:
        print("ERROR: No screenshot frames were captured!")
        return False
    print(f"Captured {len(frames)} frames successfully.")

    # 3. Compile to premium nearest-neighbor scaled GIF using FFmpeg
    output_name = f"{name}_animation.gif"
    dest_path = os.path.join(ARTIFACTS_DIR, output_name)
    
    # We use nearest-neighbor scaling to maintain the crisp, gorgeous pixel-art look of the VGA base (upscaled to 640 width)
    ffmpeg_cmd = (
        f'ffmpeg -y -framerate 30 -i screenshots/screenshot_%03d.bmp '
        f'-vf "scale=640:-1:flags=neighbor" -c:v gif "{dest_path}"'
    )
    if not run_cmd(ffmpeg_cmd):
        return False

    print(f"SUCCESS: Compiled animated GIF saved directly to:\n -> {dest_path}")
    
    # 4. Clean up temporary BMPs
    for f in frames:
        try:
            os.remove(f)
        except Exception:
            pass
            
    return True

def main():
    # Force process kill on any active thedemo instances using CMD standard taskkill
    subprocess.run("taskkill /F /IM thedemo.exe >nul 2>&1", shell=True)

    # 1. Rebuild the harness
    print("Rebuilding host preview harness...")
    if not run_cmd("make"):
        sys.exit(1)

    # 2. Capture a 2-second animated loop of our patched Spark-Gap Grid (Scene 1)
    compile_effect_gif("spark_gap", 6000, 2000)

    # 3. Capture a 2-second animated loop of our gorgeous new unstable fluid reactor core (Scene 2)
    compile_effect_gif("plasma_core", 22000, 2000)

    # 4. Capture a 2-second animated loop of our slow-flight Ray-Volt (Scene 3)
    compile_effect_gif("ray_volt", 36000, 2000)

    # 5. Capture a 2-second animated loop of our gorgeous new high-resolution full-screen Vector Strike (Scene 5)
    compile_effect_gif("vector_strike", 70000, 2000)

    # 6. Capture a 2-second animated loop of our high-speed 3D Cyber-Canyon Terrain Flight (Scene 6)
    compile_effect_gif("canyon_flight", 94000, 2000)

    # 7. Capture a 6-second animated climax loop of our grand rotozoom outro credit scroller and final convergence collapse (Scene 8)
    compile_effect_gif("voltage_arc", 150000, 6000)

    print("\nAll video compilations complete! Enjoy the gorgeous animations in your artifacts folder! [OK]")

if __name__ == "__main__":
    main()
