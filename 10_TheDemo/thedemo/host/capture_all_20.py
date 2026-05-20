#!/usr/bin/env python3
"""
capture_all_20.py — automates taking frame captures of all 8 scenes from
TheDemo (Project 20), converting them to descriptive PNG files in host/screenshots/.
"""

import os
import subprocess
import time
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
SCREENSHOTS_DIR = HERE / "screenshots"
EXE = HERE / "thedemo.exe"

# Scenes configuration: (name, start_time_ms)
SCENES = [
    ("title_copper", 8000),         # Scene 1: Copper Title (0 to 36000 ms)
    ("voxel_landscape", 42000),     # Scene 2: Voxel Landscape (36000 to 61000 ms)
    ("fluid_portraits", 70000),     # Scene 3: Fluid Portraits (61000 to 91000 ms)
    ("copperbar_scroll", 105000),   # Scene 4: Truecolor Greetz Split (91000 to 127000 ms)
    ("raytraced_spheres", 135000),  # Scene 5: Raytraced reflective spheres (127000 to 164000 ms)
    ("reaction_diffuse", 172000),   # Scene 6: Reaction-diffusion (164000 to 184000 ms)
    ("tunnel_particles", 200000),   # Scene 7: Tunnel twist (184000 to 219000 ms)
    ("rotozoom_credits", 240000)    # Scene 8: Credits outro scroller (219000 to 274000 ms)
]

def main():
    SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)

    print("=== SLOP / TheDemo Capture Tool ===")
    
    for name, t_ms in SCENES:
        print(f"\nCapturing {name} at {t_ms} ms...")
        
        # Clean up any existing screenshot_000.bmp to ensure fresh capture
        temp_bmp = SCREENSHOTS_DIR / "screenshot_000.bmp"
        if temp_bmp.exists():
            temp_bmp.unlink()
            
        # Run host preview harness with both start-ms and screenshot-at set to t_ms
        cmd = [
            str(EXE),
            "--start-ms", str(t_ms),
            "--screenshot-at", str(t_ms),
            "--exit-after", str(t_ms + 100)
        ]
        
        # Run the command and wait for completion (should be instant!)
        subprocess.run(cmd, cwd=str(HERE), check=True)
        
        # Check if bmp was created
        if not temp_bmp.exists():
            # Try screenshot_001.bmp or others in case it ticked multiple frames
            bmps = list(SCREENSHOTS_DIR.glob("screenshot_*.bmp"))
            if bmps:
                temp_bmp = bmps[0]
            else:
                print(f"Error: Capturing {name} failed, screenshot_000.bmp not found.")
                continue
            
        # Convert to PNG using PIL
        target_png = SCREENSHOTS_DIR / f"{name}.png"
        print(f"Converting to {target_png.name}...")
        
        with Image.open(temp_bmp) as img:
            img.save(target_png, "PNG")
            
        # Delete temporary BMP and other BMPs in folder
        for f in SCREENSHOTS_DIR.glob("*.bmp"):
            try:
                f.unlink()
            except Exception:
                pass
                
        print(f"Successfully captured {target_png.name} ({target_png.stat().st_size/1024:.1f} KB)!")

    print("\nAll captures complete! View them inside host/screenshots/.")

if __name__ == "__main__":
    main()
