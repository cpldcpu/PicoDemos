#!/usr/bin/env python3
"""
capture_all.py — automates taking frame captures of all 7 scenes from
the VOLTAGE demo, converting them to descriptive PNG files in host/screenshots/.
Uses fast-forwarding to complete in seconds.
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
    ("spark_gap", 8000),          # Scene 1: Spark-Gap Grid (0 to 15000 ms)
    ("plasma_core", 22000),       # Scene 2: Plasma Core (15000 to 30000 ms)
    ("ray_volt", 37000),          # Scene 3: Ray-Volt Chorus 1 (30000 to 45000 ms)
    ("spark_generator", 52000),   # Scene 4: Spark Generator (45000 to 60000 ms)
    ("vector_strike", 72000),     # Scene 5: Vector Strike (60000 to 85000 ms)
    ("canyon_flight", 96000),     # Scene 6: Canyon Flight Climax (85000 to 108000 ms)
    ("julia_shockwave", 118000),  # Scene 7: Julia Shockwave Climax (108000 to 125000 ms)
    ("voltage_arc", 140000)       # Scene 8: Voltage Arc Credits Outro (125000 to 158200 ms)
]

def main():
    SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)

    print("=== VOLTAGE Capture Tool ===")
    
    for name, t_ms in SCENES:
        print(f"\nCapturing {name} at {t_ms} ms (fast-forwarding)...")
        
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
            print(f"Error: Capturing {name} failed, screenshot_000.bmp not found.")
            continue
            
        # Convert to PNG using PIL
        target_png = SCREENSHOTS_DIR / f"{name}.png"
        print(f"Converting to {target_png.name}...")
        
        with Image.open(temp_bmp) as img:
            img.save(target_png, "PNG")
            
        # Delete temporary BMP
        temp_bmp.unlink()
        print(f"Successfully captured {target_png.name} ({target_png.stat().st_size/1024:.1f} KB)!")

    print("\nAll captures complete! View them inside host/screenshots/.")

if __name__ == "__main__":
    main()
