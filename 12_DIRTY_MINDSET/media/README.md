# DIRTY MINDSET — Production Media Assets

This subfolder contains high-quality visual captures and gameplay recordings of **DIRTY MINDSET**, an RP2350 demoscene production designed for the **Pimoroni VGA Demo Base** (or compatible custom VGA hardware).

These assets showcase the visual evolution of the demo, transitioning from retro 8-bit Amstrad CPC emulation to modern high-performance organic math simulations.

---

## 🎬 Production Screenshots

Each screenshot corresponds to one of the 8 main scenes in the demo's synchronized timeline:

| File Name | Scene / Transition | Timing (ms) | Description |
| :--- | :--- | :--- | :--- |
| [01_cpc_boot.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/01_cpc_boot.png) | Amstrad CPC Boot | `0` – `17,043` | Retro CRT booting up, typewriter basic prompt, and title backdrop. |
| [02_plasma_chip.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/02_plasma_chip.png) | Amstrad Palette Plasma | `17,043` – `38,289` | Classic software plasma constrained entirely to the CPC hardware color indices. |
| [03_text_matrix.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/03_text_matrix.png) | Text-mode Matrix Rain | `38,289` – `49,156` | Digital phosphor matrix rain featuring CPC keywords and scene tags. |
| [04_dirty_logo.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/04_dirty_logo.png) | 3D Rotating Hypercube | `49,156` – `72,306` | Neon synthwave arena, perspective grid, starfield, and rotating 3D Tesseract. |
| [05_reaction_mind.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/05_reaction_mind.png) | Stabilized Reaction Mind | `72,306` – `93,576` | Gray-Scott biological petri-dish cellular growth over a swirling plasma backdrop. |
| [06_fractal_zoom.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/06_fractal_zoom.png) | Seahorse Mandelbrot Zoom | `93,576` – `115,310` | Real-time seahorse valley zoom-dive, transitioning on the heavy music beat. |
| [07_greetings.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/07_greetings.png) | Greetings Split Screen | `115,310` – `143,197` | Bouncing liquid metaballs split over horizontally scrolling greeting copper bars. |
| [08_outro.png](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/08_outro.png) | Outro Credits & Rotozoom | `143,197` – `181,520` | Centered 3D Rotozoom backdrop, rising space dust, and credits upscroller. |

---

## 🎥 Gameplay Video Recording

*   **File Name**: [dirty_mindset_60fps.mp4](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/media/dirty_mindset_60fps.mp4)
*   **Resolution**: `1280x960`
    *   *Note: This is a perfect 4x integer upscale of the raw 320x240 frame size. It uses nearest-neighbor interpolation to ensure the pixel borders remain crisp and authentic on modern high-DPI displays.*
*   **Framerate**: Solid `60.0 fps` offline render.
*   **Audio**: Stereo AAC (`192 kb/s`), synced with the soundtrack `Green-Phosphor Prayer.mp3` (synthwave/chiptune hybrid composed using Suno 4.5).

---

## 🛠️ How to Reproduce Assets

The host simulator executable (`thedemo.exe` inside `thedemo/host`) has dedicated frame-accurate flags designed for media rendering.

### A. Snapshotting a Specific Timestamp
To capture a single scene visual immediately without playing through the demo in real-time, combine `--start-ms` (shifts clock instantly) and `--screenshot-at`:
```powershell
# Captures frame exactly at t = 25000 ms and exits immediately
.\thedemo.exe --start-ms 25000 --screenshot-at 25000
```
This writes a bitmap to `screenshots/screenshot_000.bmp`. Convert to PNG using `ffmpeg`:
```powershell
ffmpeg -y -i screenshots/screenshot_000.bmp ../../media/02_plasma_chip.png
```

### B. Compiling the 60fps Upscaled Video
1. Clear the simulator screenshots folder:
   ```powershell
   Remove-Item -Path "screenshots/*" -Force
   ```
2. Run the offline renderer to dump exactly 60 frames per second directly from the visual clock up to `181,520` ms:
   ```powershell
   .\thedemo.exe --offline
   ```
   This generates `10,892` raw frames (`screenshots/frame_00000.bmp` to `screenshots/frame_10891.bmp`).
3. Compile, upscale (4x nearest-neighbor), and sync the soundtrack using `ffmpeg`:
   ```powershell
   ffmpeg -y -r 60 -f image2 -i screenshots/frame_%05d.bmp -i "../assets/Green-Phosphor Prayer.mp3" -vf "scale=1280:960:flags=neighbor" -c:v libx264 -pix_fmt yuv420p -c:a aac -b:a 192k ../../media/dirty_mindset_60fps.mp4
   ```
4. Clean up raw frame bitmaps:
   ```powershell
   Remove-Item -Path "screenshots/*" -Force
   ```
