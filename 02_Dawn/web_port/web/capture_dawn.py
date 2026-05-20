import os
import time
from pathlib import Path
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options

# Paths
HERE = Path(__file__).resolve().parent
MEDIA_DIR = HERE.parent.parent / "media"
MEDIA_DIR.mkdir(parents=True, exist_ok=True)

# Scene captures: (filename, delay_seconds_from_start)
SCENES = [
    ("dawn_text", 2.0),       # Scene 1: DAWN text fade-in
    ("by_text", 7.5),         # Scene 2: BY text fade-in
    ("azure_text", 13.0),     # Scene 3: AZURE text fade-in
    ("torus_red", 20.0),      # Scene 4: Warm-red torus zoom
    ("torus_swirl", 40.0),    # Scene 5: Swirling highlights torus
    ("voxel_dawn", 60.0),     # Scene 6: Voxel landscape flyover
    ("torus_golden", 85.0),   # Scene 7: Golden checkered torus
    ("torus_blur", 105.0),    # Scene 8: Torus with heavy blur trail
    ("torus_finale", 125.0),  # Scene 9: Finale (grayscale torus + DAWN text + blur)
]

def main():
    print("=== DAWN Scene Capture Tool via Selenium ===")
    
    # Configure Chrome
    chrome_options = Options()
    chrome_options.add_argument("--headless") # Run in headless mode so no window pops up
    chrome_options.add_argument("--window-size=1024,768")
    chrome_options.add_argument("--mute-audio") # We don't need audio for screenshots
    
    print("Launching headless Chrome...")
    driver = webdriver.Chrome(options=chrome_options)
    
    try:
        url = "http://localhost:8000/index.html"
        print(f"Navigating to {url}...")
        driver.get(url)
        time.sleep(2.0) # Wait for page load
        
        # Click the canvas to make sure focus and playback starts
        print("Clicking canvas to start/activate audio...")
        canvas = driver.find_element(By.ID, "canvas")
        canvas.click()
        
        # Capture each scene at their respective timeline offsets
        start_time = time.time()
        for filename, offset in SCENES:
            target_time = start_time + offset
            now = time.time()
            sleep_time = target_time - now
            
            if sleep_time > 0:
                print(f"Waiting {sleep_time:.1f}s for scene: {filename}...")
                time.sleep(sleep_time)
                
            print(f"Capturing {filename}...")
            png_path = MEDIA_DIR / f"{filename}.png"
            
            # Element screenshot is beautiful and exact (crops to just the canvas!)
            canvas.screenshot(str(png_path))
            print(f"Successfully saved: {png_path.name} ({png_path.stat().st_size/1024:.1f} KB)")
            
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        print("Closing browser...")
        driver.quit()
        
    print("\nAll DAWN screenshots captured successfully!")

if __name__ == "__main__":
    main()
