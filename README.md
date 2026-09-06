# PicoDemos: Demoscene Demos for [RP2040](https://en.wikipedia.org/wiki/RP2040) & [RP2350](https://en.wikipedia.org/wiki/RP2350) using agentic AI

This repo contains some experiments to use agentic AI to design and implement demoscene demos on the **Raspberry Pi Pico ([RP2040](https://en.wikipedia.org/wiki/RP2040))** and the **Raspberry Pi Pico 2 ([RP2350](https://en.wikipedia.org/wiki/RP2350))**. Both of these are microcontrollers that cost around a dollar each.

The RP2040 is equipped with a dual-core ARM Cortex-M0+ processor clocked at 133 MHz, 264 KB of SRAM, and 8 Programmable I/O (PIO) state machines. Its successor, the RP2350, features dual-core ARM Cortex-M33 or RISC-V cores at 150 MHz, an expanded 520 KB SRAM, and 12 PIO state machines to handle more complex graphic routines. Both of these can be easily overclocked to 300MHz.

To enable them to run demos, I use the Pimorini Pico VGA demo based that basically adds resistors and connectors to allow bit-banged VGA and audio. The board is shown below.

Even though these are microcontrollers, they are very compelling targets for graphical effects and demoscene demos. The dual-core architecture allows one core focus on audio/video processing while the other core runs the demo itself.

I felt this platform is seriously underexplored. But as I learned while during my experiments in this repo, it is maybe too powerful to be a well-defined target with interesting constraints.

I started this as an innocuous experiment in using GenAI to port existing demos to the platform. This created a suitable substrate (context) for the agents to build on. The last two demos are completely Gen AI designed and created; I mostly provided critical feedback like "this looks lame, do better".

I was especially surprised by demo 11, made by Gemini 3.5 Flash, which single-shot an almost passable implementation in less than 15 minutes. The Opus demo took 2-3 evenings of back and forth and it was also strangely adamant on inserting the coral logo everywhere. GPT 5.5 interestingly only created a very boring demo, so it is not included here.

After the release of Opus 4.8 I asked it to come up with novel demos ideas, it came up with two interesting concepts, which you can see in 13 "Singularity" and 14 "Origami".

Demo 16 "Sustain" set itself a rule instead of a technique: it never cuts. Four minutes and forty-nine seconds in one unbroken camera move, where each effect has to *become* the next one rather than end. That claim is checked by a script that audits all 17,340 frames and fails the build if any of them is a discontinuity — which turned out to catch a lot of real bugs that no one would have spotted by eye.

Demo 15 "Quicksilver" is an attempt to push the hardware more. It makes use of the SIO interpolator, a custom hardware block in the RP2350 that can do pixel blending and affine address generation to render effects that would be impossible otherwise, like a beam-raced full-VGA rotozoom in truecolor. This demos was much more "hands-on" than the others.



Everything below the line is AI generated.

*Azure*

---

![PicoDemos Showcase](pico-demo.jpg)


---

##  Overview

| # | Production | Type | Primary Chip | Primary Display Output | Assistive LLM Creator |
|---|---|---|---|---|---|
| **01** | **[State Of The Art (SOTA)](01_SOTA_DEMO)** | Classic Port | [RP2040](https://en.wikipedia.org/wiki/RP2040) / [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (15-bit), ST7789 SPI, or Composite PAL | **Claude Opus 4.7** |
| **02** | **[Dawn](02_Dawn)** | Classic Port | [RP2040](https://en.wikipedia.org/wiki/RP2040) | VGA (16-bit RGB-565) | **Claude Opus 4.7** *(Web: 4.1)* |
| **10** | **[SLOP (TheDemo)](10_TheDemo)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (Multi-mode raster splits) | **Claude Opus 4.7** |
| **11** | **[VOLTAGE (FlashDemo)](11_FlashDemo)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (Multi-mode & Beam-raced) | **Gemini 3.5 Flash** *(Antigravity)* |
| **13** | **[SINGULARITY](13_Singularity)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (320×240 truecolor) | **Claude Opus 4.8** |
| **14** | **[ORIGAMI](14_Origami)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (320×240 truecolor, antialiased filled polygons) | **Claude Opus 4.8** |
| **15** | **[QUICKSILVER](15_Quicksilver)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (beam-raced full-VGA rotozoom + SIO interpolator: Mode-7/env-mapped chrome/tunnel) | **Claude Opus 4.8** |
| **16** | **[SUSTAIN](16_Sustain)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (320×240 truecolor ray-marched world — **4:49 with no cuts anywhere**) | **Claude Opus 5** |
| **17** | **[HYSTERESIS](17_Hysteresis)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (320×240 palette feedback field — **no pixel is a function of *t***, and the synth soundtrack is generated too) | **Claude Opus 5** |
| **18** | **[VESPER](18_Vesper)** | Original Demo | RP2350 | VGA (320×240 solid 3D, metallic lighting, bloom and reflections), Canticle stereo synth score — **two minutes in a 60.2 KiB flash image** | **GPT-6 Astra** *(Phase)* |
| **19** | **[PERSISTENCE](19_Persistence)** | Original Demo | [RP2350](https://en.wikipedia.org/wiki/RP2350) | VGA (**native 640×480, no framebuffer anywhere** — every scanline generated live for the beam, 31,500 a second) + a tracker score on the other core | **Claude Fable 5.1** *(Phosphor)* |

> QUICKSILVER, SUSTAIN, HYSTERESIS, VESPER and PERSISTENCE are productions of **[LATENT](LATENT.md)** — a demoscene group for machine-made productions on bare-metal silicon. Code & direction by **Beam** / Claude Opus 4.8, **Overscan** / Claude Opus 5, **Phase** / GPT-6 Astra, and **Phosphor** / Claude Fable 5.1; music by **Suno** on QUICKSILVER and SUSTAIN, **Overscan** on HYSTERESIS, **Phase** on VESPER, and **Phosphor** on PERSISTENCE. HYSTERESIS, VESPER and PERSISTENCE synthesize their soundtracks on the device. Visuals also contributed by **Antigravity** / Gemini and **GPT Image 2**; human critic: **Azure**.

---

##  Repository Structure

```
PicoDemos/
├── .gitignore                   # Workspace-wide unified Git exclude rules
├── README.md                    # Root collection directory map and overview (this file)
├── pico-demo.jpg                # Showcase banner image
│
├── 01_SOTA_DEMO/                # C-reimplementation of the 1992 Amiga demo by Spaceballs
│   ├── sota/                    # Upstream nfd/sota submodule
│   ├── native/                  # Windows desktop simulator port
│   ├── pico/                    # ST7789 SPI, Composite PAL, and VGA Pico/Pico2 backends
│   └── *.uf2                    # Checked-in release firmware images
│
├── 02_Dawn/                     # Port of the 1995 Amiga 1200 4K intro by AZURE/ARTWORK
│   ├── pico/                    # RP2040 hardware engine (scanvideo-based VGA output)
│   ├── web_port/                # TypeScript reference & browser preview harness
│   └── dawn_vga_rp2040.uf2      # Checked-in release firmware image
│
├── 10_TheDemo/                  # SLOP: An original 4-minute beat-synced RP2350 VGA demo
│   ├── thedemo/                 # Engine sources, tools, custom split raster drivers
│   ├── slop_vga_rp2350.uf2      # Checked-in release firmware image
│   └── README.md                # Memory budgets, librosa beat-mapping, Suno 4.5 audio
│
├── 11_FlashDemo/                # VOLTAGE: An original cyber-neon high-voltage RP2350 VGA demo
│   ├── thedemo/                 # Fluid solvers, Cortex-M33 FPU raymarching, Julia set fractal
│   ├── PLANNING.md              # Technical architecture, dual-core sync, storyboard
│   ├── voltage_vga_rp2350.uf2   # Checked-in release firmware image
│   └── README.md                # Render mode details and credits
│
├── 13_Singularity/              # SINGULARITY: relativistic black-hole journey, full 320×240 truecolor
    ├── singularity/             # Engine sources, 8 scene effects, tools (geodesic LUT baker), SDL host
    ├── media/                   # Per-scene screenshots + 60fps demo video
    ├── IMPLEMENTATION.md        # Full design doc: memory budget, asset + Suno 4.5 prompts
    ├── singularity_vga_rp2350.uf2  # Checked-in release firmware image
    └── README.md                # Arc, MODE_HIRES truecolor engine, build, credits
│
├── 14_Origami/                  # ORIGAMI: warm folded-paper flat-shaded polygon demo
│   ├── origami/                 # Engine sources, polygon renderer, six scene effects, SDL host
│   ├── assets/                  # Paper backdrops, texture tile, and source MP3
│   ├── media/                   # Host-preview screenshots of the six scenes
│   ├── IMPLEMENTATION.md        # Design/build notes and beat-synced timeline
│   └── origami_vga_rp2350.uf2   # Checked-in release firmware image
│
├── 15_Quicksilver/              # QUICKSILVER: liquid-chrome demo on the RP2350 SIO interpolator
    ├── quicksilver/             # Engine + interpolator emulator, 10 scene effects, tools, SDL host
    │   └── IMPLEMENTATION.md     # Design doc: interpolator datapath, memory budget, Suno 5.5 prompts
    ├── media/                   # Per-scene screenshots + 60 fps demo video
│   ├── quicksilver_vga_rp2350.uf2  # Checked-in release firmware image
│   └── README.md                # Arc, interpolator hero, raycast tunnels, build, credits
│
├── 16_Sustain/                  # SUSTAIN: a demo with no cuts — one 4:49 unbroken shot
│   ├── sustain/                 # One renderer, one camera spline, one world function
│   │   ├── fields/              # terrain / cave / monolith families; morphs are parameter lerps
│   │   └── tools/               # cut_detect.py (the no-cut audit), pair + tiling + music checks
│   ├── media/                   # Twelve moments from the single shot + 60 fps video
│   ├── PLANNING.md              # The rule, the referee, and the architecture it forced
│   ├── sustain_vga_rp2350.uf2   # Checked-in release firmware image
│   └── README.md                # Arc, the audit, on-device performance work, credits
│
├── 17_Hysteresis/               # HYSTERESIS: a demo with memory — no pixel is a function of t
│   ├── hysteresis/              # The operator, the arc, and a synth that shares the score
│   │   ├── field.c              # convolve → advect → react → persist; one pass, no branches
│   │   ├── score.c              # ONE event table, read by both the field and the synth
│   │   ├── synth.c              # integer synth; the soundtrack is generated, not played back
│   │   ├── host/                # SDL build: watch it, listen to it, capture it
│   │   └── tools/               # no_keyframes.py (the memory referee), capture, audio checks
│   ├── assets/                  # wordmark + endcard, injected as forcing rather than blitted
│   ├── media/                   # Video, stills, and why this content barely encodes
│   ├── PLANNING.md              # The rule, the exemption, and where the theory was wrong
│   ├── hysteresis_vga_rp2350.uf2 # Checked-in release firmware image
│   └── README.md                # Arc, the referee, the shared score, credits
│
├── 18_Vesper/                   # VESPER: illuminated machinery and the Canticle stereo score
    ├── vesper/                  # Solid 3D renderer, synth, Pico backend and SDL player
    ├── media/                   # Scene gallery, full video and release validation
    ├── music_review/            # Original/Canticle comparison and approval record
    ├── build.ps1               # Host, firmware, checks and video capture
    ├── Run Vesper.cmd          # Desktop launcher
    ├── vesper_vga_rp2350.uf2    # Release firmware image
│   └── README.md               # Direction, architecture, build and Phase / GPT-6 Astra credits
│
└── 19_Persistence/              # PERSISTENCE: a demo with no framebuffer — native 640×480
    ├── persistence/             # Ten scanline kernels; core 1 owns space, core 0 owns time
    │   ├── beam.c/.h            # the line contract, and the runner that dispatches it
    │   ├── fx_*.c               # title, plasma, kefrens, twister, tunnel, plane, split, credits
    │   ├── s3d.c                # solid 3D as per-row visible-boundary lists (an S-buffer)
    │   ├── song.c / synth.c     # the tracker tune and the stereo synth, on core 0
    │   ├── host/                # SDL player — the one place a whole frame is assembled
    │   └── tools/               # no_framebuffer.py, audit.exe, capture, gallery, serial
    ├── media/                   # Video, per-scene stills, piano roll, audit + device logs
    ├── PLANNING.md              # The rule, the budget, and what the referees have to prove
    ├── persistence_vga_rp2350.uf2  # Checked-in release firmware image
    └── README.md                # Arc, measurements, the three referees, what went wrong
```

---

##  The Demos

### 1. 01_SOTA_DEMO (State Of The Art)
* A brilliant port based on the modern C-reimplementation [nfd/sota](https://github.com/nfd/sota) of the legendarily smooth **1992 Amiga demo by Spaceballs** (original release on [pouët.net](https://www.pouet.net/prod.php?which=99)).
* **Assistive Assistant:** **Claude Opus 4.7** compiled the C port and helped adapt it to Windows, RP2040, and RP2350.
* **Target Outputs:**
  - **VGA Output:** [sota_vga_rp2040.uf2](01_SOTA_DEMO/sota_vga_rp2040.uf2) / [sota_vga_rp2350.uf2](01_SOTA_DEMO/sota_vga_rp2350.uf2), 320×240 @ 60 Hz 32K colors with 11 kHz mono QOA-encoded audio, running on the Pimoroni VGA board.
  - **ST7789 SPI Display:** [sota_tft_rp2040.uf2](01_SOTA_DEMO/sota_tft_rp2040.uf2), 240×240 full-color (RGB-565) SPI panel.
  - **Composite PAL / S-Video:** [sota_composite_rp2040.uf2](01_SOTA_DEMO/sota_composite_rp2040.uf2), 320×256 @ 50 Hz PAL video output utilizing a custom 3-bit binary-weighted resistor DAC (8 grey levels with 4×4 Bayer ordered dithering to produce ~128 perceived grey shades).
* **Core Technical Milestone:** Pad widths to multiples of 64 pixels to bypass Cortex-M0+ unaligned 32-bit hardware-fault limits, custom fast DMA-PWM mixing to play QOA audio.
* **Screenshots & Captures Showcase:**
  <table>
    <tr>
      <td align="center" width="50%">
        <img src="01_SOTA_DEMO/SOTA_composite.jpg" width="100%" alt="SOTA Composite PAL Output"/><br/>
        <sub><b>📺 Composite PAL Output (Dithered Grey)</b></sub>
      </td>
      <td align="center" width="50%">
        <video src="01_SOTA_DEMO/SOTA_on_tft_screen.mp4" width="100%" controls></video><br/>
        <sub><b>📱 SPI TFT Vector Rotoscoping Video</b></sub>
      </td>
    </tr>
  </table>

### 2. 02_Dawn
* Port of the  **1995 Amiga 1200 4K intro by AZURE/ARTWORK** (original release on [pouët.net](https://www.pouet.net/prod.php?which=9213)).
* **Assistive Assistant:** **Claude Opus 4.7** (the initial web-harness TypeScript port in `web_port/` was completed using **Opus 4.1**).
* **Target Outputs:** Pimoroni VGA Demo Base running at 320×240@60Hz (line-doubled from a 160×128 6bpp chunky framebuffer).
* **Web Preview:** [Run the Dawn browser port on GitHub Pages](https://cpldcpu.github.io/PicoDemos/).
* **Prebuilt Firmware:** [dawn_vga_rp2040.uf2](02_Dawn/dawn_vga_rp2040.uf2).
* **Core Technical Milestone:** Squeezed into a tight 264 KB SRAM budget by dynamically regenerating torus geometries, voxel shading lists, and division tables inline on the ARM cores rather than allocating expensive look-up tables (LUTs) like the original 2 MB Amiga version did.
* **Screenshots Showcase (3D Raycaster & Environmental Effects):**
  <table>
    <tr>
      <td><img src="02_Dawn/media/voxel_dawn.png" width="220" alt="Voxel Terrain"/></td>
      <td><img src="02_Dawn/media/torus_golden.png" width="220" alt="Golden Torus"/></td>
      <td><img src="02_Dawn/media/torus_swirl.png" width="220" alt="Torus Swirl"/></td>
    </tr>
    <tr>
      <td><img src="02_Dawn/media/torus_red.png" width="220" alt="Crimson Torus"/></td>
      <td><img src="02_Dawn/media/torus_finale.png" width="220" alt="Torus Finale"/></td>
      <td><img src="02_Dawn/media/torus_blur.png" width="220" alt="Torus Motion Blur"/></td>
    </tr>
  </table>

### 3. 10_TheDemo (SLOP)
* A completely original, breathtaking 4-minute demoscene production designed specifically for the **Pico 2 (RP2350)**.
* **Generator:** **Claude Opus 4.7**.
* **Target Outputs:** Waveshare RP2350-Plus + Pimoroni VGA Demo Base. Runs a beat-synced layout mixed at runtime with a high-fidelity 22050 Hz Mono QOA soundtrack.
* **Prebuilt Firmware:** [slop_vga_rp2350.uf2](10_TheDemo/slop_vga_rp2350.uf2).
* **Aesthetic:** "Bio-organic" (wet, iridescent, alien-vegetal).
* **Visual Highlights:** Shimmering copper logo intro, voxel landscapes, dynamic advected fluid simulators hosting AI-generated portraits, reflective raytraced metaballs in truecolor, Gray-Scott reaction-diffusion grids, and particle-storm tunnels.
* **Screenshots Showcase (Bio-Organic Synth Engine):**
  <table>
    <tr>
      <td><img src="10_TheDemo/media/title_copper.png" width="220" alt="Copper Intro"/></td>
      <td><img src="10_TheDemo/media/raytraced_spheres.png" width="220" alt="Raytraced Metaballs"/></td>
      <td><img src="10_TheDemo/media/fluid_portraits.png" width="220" alt="Fluid Portraits"/></td>
    </tr>
    <tr>
      <td><img src="10_TheDemo/media/reaction_diffuse.png" width="220" alt="Reaction Diffusion"/></td>
      <td><img src="10_TheDemo/media/voxel_landscape.png" width="220" alt="Voxel Landscape"/></td>
      <td><img src="10_TheDemo/media/tunnel_particles.png" width="220" alt="Particle Storm Tunnel"/></td>
    </tr>
  </table>

### 4. 11_FlashDemo (VOLTAGE)
* An original, incredibly fast cyber-neon, high-voltage production designed specifically for the **Pico 2 (RP2350)**.
* **Generator:** **Gemini 3.5 Flash** *(Antigravity)*.
* **Target Outputs:** Waveshare RP2350-Plus + Pimoroni VGA Demo Base.
* **Prebuilt Firmware:** [voltage_vga_rp2350.uf2](11_FlashDemo/voltage_vga_rp2350.uf2).
* **Aesthetic:** "Kinetic Neon, High-Voltage Plasma, and Cybernetic Glitches".
* **Visual Highlights:** Cyber spark-gap grids, unstable fluid reactor cores, real-time raymarched reflective cylinders (utilizing M33 FPU instructions), morphing 3D wireframe vector stars, polar-coordinate lightning spark-generators, real-time zooming Julia fractal shockwaves, and sub-pixel rotozooming outro credits.
* **Screenshots Showcase (Cyber-Neon Shader Engine):**
  <table>
    <tr>
      <td><img src="11_FlashDemo/media/voltage_arc.png" width="220" alt="Voltage Arc"/></td>
      <td><img src="11_FlashDemo/media/vector_strike.png" width="220" alt="Vector Strike"/></td>
      <td><img src="11_FlashDemo/media/julia_shockwave.png" width="220" alt="Julia Shockwave"/></td>
    </tr>
    <tr>
      <td><img src="11_FlashDemo/media/plasma_core.png" width="220" alt="Plasma Reactor Core"/></td>
      <td><img src="11_FlashDemo/media/spark_generator.png" width="220" alt="Spark Generator"/></td>
      <td><img src="11_FlashDemo/media/spark_gap.png" width="220" alt="Spark Gap Grid"/></td>
    </tr>
  </table>

### 5. 13_Singularity (SINGULARITY)
* An original, physics-grounded production designed specifically for the **Pico 2 (RP2350)** — a relativistic journey into a black hole, rendered in **full 320×240 truecolor** end-to-end.
* **Generator:** **Claude Opus 4.8**.
* **Target Outputs:** Waveshare RP2350-Plus + Pimoroni VGA Demo Base, beat-synced to a 5:30 Suno 4.5 soundtrack (*"Graviton Choir"*).
* **Prebuilt Firmware:** [singularity_vga_rp2350.uf2](13_Singularity/singularity_vga_rp2350.uf2).
* **Aesthetic:** "Cosmic & relativistic" — deep indigo/violet voids, accretion gold, Doppler blue→red.
* **Visual Highlights:** curl-noise dust collapsing onto a forming star, a relativistic aberration/Doppler starfield warp, a Mode-7 accretion disk with Doppler beaming + orbiting hot-spots, a real **Schwarzschild gravitational-lensing** Einstein ring (geodesics integrated offline into a remap LUT), and a glowing wireframe **spacetime gravity-well** plunging to the singularity.
* **Core Technical Milestone:** debuts a full **320×240 RGB565 truecolor mode** (`MODE_HIRES`) sharing the framebuffer arena with the palette modes, and bakes Schwarzschild null-geodesic photon paths into a flash LUT so the lensing climax is a ~1-pixel-cheap gather on-device.
* **Screenshots Showcase (Relativistic Black-Hole Engine):**
  <table>
    <tr>
      <td><img src="13_Singularity/media/lensing.png" width="220" alt="Gravitational Lensing"/></td>
      <td><img src="13_Singularity/media/accretion_disk.png" width="220" alt="Doppler Accretion Disk"/></td>
      <td><img src="13_Singularity/media/spacetime.png" width="220" alt="Spacetime Gravity Well"/></td>
    </tr>
    <tr>
      <td><img src="13_Singularity/media/nebula.png" width="220" alt="Curl-noise Nebula"/></td>
      <td><img src="13_Singularity/media/starfield.png" width="220" alt="Relativistic Warp"/></td>
      <td><img src="13_Singularity/media/rebirth.png" width="220" alt="Rebirth Endcard"/></td>
    </tr>
  </table>

### 6. 14_Origami (ORIGAMI)
* A warm, tactile folded-paper production designed specifically for the **Pico 2 (RP2350)** — the deliberate opposite of SINGULARITY.
* **Generator:** **Claude Opus 4.8**.
* **Target Outputs:** Waveshare RP2350-Plus + Pimoroni VGA Demo Base, beat-synced to *"Marimba Seedbox"* (Suno 4.5, prompted by Claude; 117.5 BPM, 2:15).
* **Prebuilt Firmware:** [origami_vga_rp2350.uf2](14_Origami/origami_vga_rp2350.uf2).
* **Demo Video:** 📺 [14_Origami/media/origami.mp4](14_Origami/media/origami.mp4) (full 2:15 @ 60 fps with soundtrack).
* **Aesthetic:** soft paper whites, cream, sky blue, coral, sage, and mustard, with hand-made soft drop shadows.
* **Visual Highlights:** a folded title sheet, paper-plane flight, crane fold/unfold morph, pop-up book skyline, Miura-ori tessellation wave, and confetti credits.
* **Core Technical Milestone:** this repo's first real-time **flat-shaded filled-polygon 3D engine** ([poly3d.c](14_Origami/origami/effects/poly3d.c)) — Rodrigues crease folding, perspective projection, back-face cull, painter's-algorithm depth sorting, per-face Lambert shading, **antialiased** coverage span fill, and soft alpha-darkened paper shadows, all in 320×240 RGB565 truecolor at 60 fps.
* **Screenshots Showcase (Flat-Shaded Paper Engine):**
  <table>
    <tr>
      <td><img src="14_Origami/media/title.png" width="220" alt="Title"/></td>
      <td><img src="14_Origami/media/plane.png" width="220" alt="Paper Plane"/></td>
      <td><img src="14_Origami/media/crane.png" width="220" alt="Crane Fold"/></td>
    </tr>
    <tr>
      <td><img src="14_Origami/media/city.png" width="220" alt="Pop-up Paper City"/></td>
      <td><img src="14_Origami/media/miura.png" width="220" alt="Miura Wave"/></td>
      <td><img src="14_Origami/media/credits.png" width="220" alt="Confetti Credits"/></td>
    </tr>
  </table>

---

### 7. 15_Quicksilver (QUICKSILVER)
* A liquid-chrome production and the first to make this repo's **RP2350 SIO interpolator** the hardware hero (demo 13 reserved it but never wired it up).
* **Generator:** **Claude Opus 4.8** (scene handle **Beam**) — the first production of the **[LATENT](LATENT.md)** group.
* **Target Outputs:** Pico 2 (RP2350) + Pimoroni VGA Demo Base, beat-synced to a **Suno 5.5** track (*"Second Key Change"*, ~92 BPM, 3:04).
* **Prebuilt Firmware:** [quicksilver_vga_rp2350.uf2](15_Quicksilver/quicksilver_vga_rp2350.uf2).
* **Demo Video:** 📺 [15_Quicksilver/media/quicksilver.mp4](15_Quicksilver/media/quicksilver.mp4) (full 3:04 @ 60 fps with soundtrack).
* **Core Technical Milestone:** a **bit-exact software emulator** of the SIO interpolator lets the identical effect code preview on the SDL host and run as raw silicon on the RP2350. The interpolator's affine address-gen, **BLEND** (hardware bilinear lerp) and **CLAMP** units drive the Mode-7 mercury plain, the liquid-metal plasma, the chrome env-mapping and the **beam-raced native-640 rotozoom** (no framebuffer). The climax conduit is a **relief raymarcher** (real 3D wall displacement) that marches a coarse column grid and interpolates the hit coordinates to hold 60 fps at full QVGA.
* **Visual Highlights:** chrome wordmark title, reflective Mode-7 mercury plain, an iridescent flowing liquid-metal plasma, a rubber rotozoomer, **2D bump-mapped mercury under a moving light**, high-poly env-mapped chrome solids (neutral/violet/gold matcaps) with beat-synced object swaps, a **voxel/relief chrome tunnel** at the climax, and a readable credits scroller into the LATENT sting.
* **Screenshots Showcase (in running order):**
  <table>
    <tr>
      <td><img src="15_Quicksilver/media/title.png" width="220" alt="Title"/></td>
      <td><img src="15_Quicksilver/media/mode7.png" width="220" alt="Mercury Plain"/></td>
      <td><img src="15_Quicksilver/media/plasma.png" width="220" alt="Liquid Plasma"/></td>
    </tr>
    <tr>
      <td><img src="15_Quicksilver/media/rotozoom.png" width="220" alt="Rubber Rotozoomer"/></td>
      <td><img src="15_Quicksilver/media/liquid.png" width="220" alt="Bump-Mapped Mercury"/></td>
      <td><img src="15_Quicksilver/media/chrome.png" width="220" alt="Chrome Drop"/></td>
    </tr>
    <tr>
      <td><img src="15_Quicksilver/media/tunnel.png" width="220" alt="Voxel Tunnel (climax)"/></td>
      <td><img src="15_Quicksilver/media/chrome_gold.png" width="220" alt="Chrome Climax"/></td>
      <td><img src="15_Quicksilver/media/credits.png" width="220" alt="Credits"/></td>
    </tr>
  </table>

---

### 8. 16_Sustain (SUSTAIN)
* **A demo with no cuts.** Four minutes and forty-nine seconds in one unbroken camera move — no fades, no crossfades, no dissolves, no scene boundaries, and the screen never goes black until the final frame.
* **Generator:** **Claude Opus 5** (scene handle **Overscan**) — a **[LATENT](LATENT.md)** production.
* **Target Outputs:** Pico 2 (RP2350) + Pimoroni VGA Demo Base, 320×240 RGB565 line-doubled to 640×480 @ 60 Hz, scored to a **Suno** track built around a sustained bass pedal that never stops — which is where the demo gets its name.
* **Prebuilt Firmware:** [sustain_vga_rp2350.uf2](16_Sustain/sustain_vga_rp2350.uf2).
* **Demo Video:** 📺 [16_Sustain/media/sustain.mp4](16_Sustain/media/sustain.mp4) (full 4:49 @ 60 fps with soundtrack).
* **Core Technical Milestone:** the no-cut rule is a *constraint on the architecture*, not a stylistic claim — there is **one** ray-marched world function, and enclosure, cross-section and material are all **parameters** of it, so the world *becomes* rather than cuts. Fourteen morphs are parameter lerps along a single camera spline. The claim is enforced mechanically by `cut_detect.py`, which audits all **17,340 frames** and fails the build on any discontinuity — which caught a number of real bugs no one would have spotted by eye.
* **Visual Highlights:** sea → canyon → chasm → slot → tunnel → cave → chamber → monoliths → cooling → collapse → and back to the opening sea, as one continuous transit.
* **Honest caveat:** ~10 fps enclosed, ~15 fps open, at 300 MHz — not yet 60. The README documents where it started and what moved it.
* **Screenshots Showcase (in running order):**
  <table>
    <tr>
      <td><img src="16_Sustain/media/sea.png" width="220" alt="Sea"/></td>
      <td><img src="16_Sustain/media/canyon.png" width="220" alt="Canyon"/></td>
      <td><img src="16_Sustain/media/chasm.png" width="220" alt="Chasm"/></td>
    </tr>
    <tr>
      <td><img src="16_Sustain/media/slot.png" width="220" alt="Slot"/></td>
      <td><img src="16_Sustain/media/tunnel.png" width="220" alt="Tunnel"/></td>
      <td><img src="16_Sustain/media/cave.png" width="220" alt="Cave"/></td>
    </tr>
    <tr>
      <td><img src="16_Sustain/media/monolith.png" width="220" alt="Monoliths"/></td>
      <td><img src="16_Sustain/media/collapse.png" width="220" alt="Collapse"/></td>
      <td><img src="16_Sustain/media/return.png" width="220" alt="Return to the sea"/></td>
    </tr>
  </table>

---

### 9. 17_Hysteresis (HYSTERESIS)
* **A demo with memory.** Where every other production here draws frame *n* from the clock, this one may not: **no pixel is a function of *t***, and every one of the 12,600 frames is computed from the frame before it. The demo is a single dynamical system being stepped, not a timeline being drawn.
* **Generator:** **Claude Opus 5** (scene handle **Overscan**) — a **[LATENT](LATENT.md)** production.
* **Target Outputs:** Pico 2 (RP2350) + Pimoroni VGA Demo Base, 320×240 8-bit palette-indexed pixel-doubled to 640×480 @ **59.8 fps**, 300 MHz @ 1.20 V.
* **Prebuilt Firmware:** [hysteresis_vga_rp2350.uf2](17_Hysteresis/hysteresis_vga_rp2350.uf2).
* **Demo Video:** 📺 [17_Hysteresis/media/hysteresis.mp4](17_Hysteresis/media/hysteresis.mp4) (full 3:30 @ 60 fps with soundtrack).
* **Core Technical Milestone:** the byte **is** the simulation state and **is** the displayed pixel, so the existing double buffer is the feedback ping-pong. Each step convolves the previous frame, advects it through a flow field of three vortices, then pushes it through a non-monotone reaction curve — ~5 reads and 1 write per cell at **63.4 cycles/cell**, no branches in the inner loop, no division, no floating point. Advection is computed once per **16×16 block** in the Amiga blitter-feedback manner, and the resulting quantisation error *is* the fractal structure. The rule has teeth: there is no seek anywhere, and a dropped frame does not stutter, it **diverges** — so the frame budget is a correctness property the referee enforces.
* **The soundtrack is generated too:** an integer synth on **core 1** (bass pedal, twelve-oscillator pad, noise bed, six tuned resonators, reverb) playing from `score.c` — **the same table that injects energy into the picture**. The event that is seen *is* the event that is heard, so there is no alignment step. 120 BPM against 60 fps against 22,050 Hz gives 1 beat = 30 frames = 11,025 samples exactly.
* **Verified:** host and device agree on every field hash **and** every audio hash across all 210 seconds; worst frame 16,482 µs against a 16,667 µs budget. `no_keyframes.py` perturbs one pixel at frame 0 and requires the divergence to *grow*, against a negative control that must forget — and it caught the project's own planning document being wrong about which map carries the memory.
* **One declared exemption:** the palette may be *f(t)*. Colour is readout, not state.
* **Screenshots Showcase (in running order):**
  <table>
    <tr>
      <td><img src="17_Hysteresis/media/opening.png" width="220" alt="The opening: one lit cell"/></td>
      <td><img src="17_Hysteresis/media/peak.png" width="220" alt="The peak"/></td>
      <td><img src="17_Hysteresis/media/endcard.png" width="220" alt="Endcard"/></td>
    </tr>
  </table>

  ![the arc](17_Hysteresis/media/arc_strip.png)

---

### 10. 18_Vesper (VESPER)

* **A machine for the blue hour.** A two-minute **[LATENT](LATENT.md)** production of illuminated architecture, warm metal and mechanical flowers, with graphics and music fitting into a **60.2 KiB flash image** (61,640 bytes).
* **Generator:** **GPT-6 Astra** (scene handle **Phase**) — code, direction and music. **Azure** — critic and producer.
* **Target Outputs:** Pico 2 (RP2350, Cortex-M33) + Pimoroni VGA Demo Base, 320×240 15-bit color doubled to 640×480 VGA scanout, with 24 kHz stereo PWM audio. Configured for 300 MHz at 1.20 V.
* **Prebuilt Firmware:** [vesper_vga_rp2350.uf2](18_Vesper/vesper_vga_rp2350.uf2).
* **Demo Video:** [18_Vesper/media/vesper.mp4](18_Vesper/media/vesper.mp4) — the complete 2:00 host capture at 60 fps with the approved **Canticle** soundtrack.
* **Desktop Player & Build Instructions:** [18_Vesper/README.md](18_Vesper/README.md); [Windows launcher](18_Vesper/Run%20Vesper.cmd).
* **Visual Highlights:** a geometric wordmark and metal ring, a flight through an illuminated nave, a rotating trefoil with traveling cyan bands, a 28-blade mechanical iris, a disassembling shard sphere, and 169 solid columns moving with the score. The final knot unwinds into the opening ring.
* **Core Technical Milestone:** procedural solid 3D geometry with near-plane clipping, reciprocal-depth occlusion and Gouraud metallic lighting, complemented by screen-space reflections and an 80×60 bloom field. Core 0 renders while core 1 supplies scanlines and synthesizes audio. A scanline-zero handshake transfers framebuffer ownership; the picture follows samples consumed by DMA.
* **The Soundtrack:** **Canticle**, composed and synthesized by **Phase** (GPT-6 Astra) — 64 bars at 128 BPM, with a recurring D-minor melody, rests, close chord voicings, a soft FM bell, bass, drums and stereo delay. Revised after Azure's music review and integrated only after approval. There are no recorded audio samples or external visual assets.
* **Validation:** all 7,200 frames plus the black endpoint pass the host checks; the complete stereo output is identical across different synthesis block sizes and matches the approved music preview sample for sample. The RP2350 UF2 builds and passes the release audit. **Physical playback and device frame rate remain untested; the video is a host capture.**
* **Screenshots Showcase (in running order):**

  <table>
    <tr>
      <td><img src="18_Vesper/media/opening.png" width="220" alt="VESPER title and metal ring"/></td>
      <td><img src="18_Vesper/media/nave.png" width="220" alt="Flight through the illuminated nave"/></td>
      <td><img src="18_Vesper/media/reliquary.png" width="220" alt="Metal trefoil with cyan bands"/></td>
    </tr>
    <tr>
      <td><img src="18_Vesper/media/iris.png" width="220" alt="Mechanical iris with 28 curling blades"/></td>
      <td><img src="18_Vesper/media/swarm.png" width="220" alt="Disassembling sphere of metallic shards"/></td>
      <td><img src="18_Vesper/media/organ.png" width="220" alt="Field of 169 animated columns"/></td>
    </tr>
  </table>

---

### 11. 19_Persistence (PERSISTENCE)

* **A demo with no framebuffer.** Two and a half minutes at **native 640×480**, and at no point does a frame of the picture exist anywhere in the machine. Every other production here draws 320×240 and lets the scanout double it, for a good reason: a 640×480 RGB565 frame is 614,400 bytes and the RP2350 has 524,288, so the framebuffer for the native mode *cannot exist* — by 90 KB, before the demo takes up any of it.
* **Generator:** **Claude Fable 5.1** (scene handle **Phosphor**) — code, direction **and music**. A **[LATENT](LATENT.md)** production; critic **Azure**.
* **Target Outputs:** Pico 2 (RP2350) + Pimoroni VGA Demo Base, 640×480 @ 59.75 Hz with 24 kHz stereo PWM audio, 300 MHz @ 1.20 V.
* **Prebuilt Firmware:** [persistence_vga_rp2350.uf2](19_Persistence/persistence_vga_rp2350.uf2).
* **Demo Video:** 📺 [19_Persistence/media/persistence.mp4](19_Persistence/media/persistence.mp4) (the full 2:30 @ 60 fps with soundtrack).
* **Core Technical Milestone:** core 1 writes each of the 480 lines straight into the scanline buffer as the beam arrives — **31,500 deadlines a second**, ~9,600 cycles each — while core 0 is permitted only to prepare per-row *tables* and to synthesise the music. Solid 3D is done as per-row **visible-boundary lists** (an S-buffer, the technique invented for machines that could not afford a z-buffer, which turns out to be exactly right for one that cannot afford a framebuffer). The tunnel computes angle and depth *exactly* every 24 pixels and lets the SIO interpolator walk between, because the lookup table it would otherwise need is 614 KB. Every gradient in the demo is ordered-dithered, because the DAC is five bits a channel and a smooth ramp across 640 pixels bands badly; it costs nothing, since a flat row is still one fill and the plasma dithers by choosing between four pre-built palettes with a pointer.
* **Measured on hardware, over all 9,000 frames and 4,320,000 scanlines: zero scanlines were shown to the beam unwritten.** The device detects this directly — `scanvideo` skips scanline ids when the beam has already passed, so a non-consecutive id *is* a missed line. Three referees gate the build: `no_framebuffer.py` proves from the linker map that nothing in the image is big enough or shaped like a frame; the device slip counter; and `audit.exe` over every frame and every sample.
* **Visual Highlights:** a beam that sweeps down and burns the title into the phosphor behind it, plasma at native width, Kefrens bars from one line buffer that is never cleared, twisting prisms, a live-computed tunnel, a Mode-7 plane with one large solid object turning over it, a **raster split running five different programs at once**, and an ending where the deflection fails and the picture collapses to a line, a dot, and out.
* **The Soundtrack:** a tracker tune in A minor at 144 BPM, ninety bars, up a tone for the last chorus, written as note tables one at a time and played by an integer stereo synth on core 0. 144 BPM against 59.75 Hz gives **1 beat = 25 frames = 10,000 samples** exactly, and the 3D objects bounce on the same table the synth reads.
* **Screenshots Showcase (in running order):**
  <table>
    <tr>
      <td><img src="19_Persistence/media/f00560.png" width="220" alt="The beam burns the title in"/></td>
      <td><img src="19_Persistence/media/f01900.png" width="220" alt="Kefrens bars"/></td>
      <td><img src="19_Persistence/media/f02700.png" width="220" alt="Twisters over the copper"/></td>
    </tr>
    <tr>
      <td><img src="19_Persistence/media/f03500.png" width="220" alt="The tunnel"/></td>
      <td><img src="19_Persistence/media/f04600.png" width="220" alt="The plane and a solid object"/></td>
      <td><img src="19_Persistence/media/f06100.png" width="220" alt="The raster split"/></td>
    </tr>
    <tr>
      <td><img src="19_Persistence/media/f06900.png" width="220" alt="The finale"/></td>
      <td><img src="19_Persistence/media/f08380.png" width="220" alt="Credits"/></td>
      <td><img src="19_Persistence/media/f08880.png" width="220" alt="Endcard"/></td>
    </tr>
  </table>

---

## Global Build & Environment Prerequisites

To compile any of the microcontroller binaries in this workspace, ensure your development machine matches the following environmental setup:

1. **Toolchain (Windows MSYS2 UCRT64):**
   - Install MSYS2 and use the UCRT64 environment.
   - Run `pacman -S` to install: `mingw-w64-ucrt-x86_64-arm-none-eabi-gcc`, `mingw-w64-ucrt-x86_64-arm-none-eabi-newlib`, `mingw-w64-ucrt-x86_64-cmake`, `mingw-w64-ucrt-x86_64-make`, `mingw-w64-ucrt-x86_64-picotool`, and `mingw-w64-ucrt-x86_64-ffmpeg`.
2. **Pico SDK 2.x:**
   - Git clone [raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk) and set the `$env:PICO_SDK_PATH` environment variable.
   - Ensure the submodules are fully updated (especially `lib/tinyusb`).
3. **Pico Extras:**
   - Git clone [raspberrypi/pico-extras](https://github.com/raspberrypi/pico-extras) (required for VGA scanvideo) and set `$env:PICO_EXTRAS_PATH`.
4. **Desktop Preview (Optional):**
   - For rapid iteration on desktop without flashing hardware, the demos provide MSYS2 SDL2 host shims. Make sure `mingw-w64-ucrt-x86_64-SDL2`, `mingw-w64-ucrt-x86_64-SDL2_mixer`, and `mingw-w64-ucrt-x86_64-libmikmod` are installed.

---

## Credits & Acknowledgements

* **Original Retro Hardware & Software Creators:**
  - **Spaceballs** (Original Amiga release of *State Of The Art*, 1992 - [pouët.net](https://www.pouet.net/prod.php?which=99)).
  - **nfd** (Modern C desktop/SDL reimplementation of SOTA - [nfd/sota](https://github.com/nfd/sota)).
  - **AZURE/ARTWORK** (Original Amiga 4k intro *Dawn*, 1995 - [pouët.net](https://www.pouet.net/prod.php?which=9213)).
* **Human Engineering & Direction:** Azure (Critic, coordinator)
* **LLM Engineering Squad:**
  - **Claude Opus 4.7** (Concept design, storyboard design, vector ports, and engine architecture for SLOP / Project 10).
  - **Gemini 3.5 Flash / Antigravity** (Storyboard implementation, raymarching, Gray-Scott solvers, CRT transitions, and assembly/VGA timing optimizations for VOLTAGE / Project 11).
  - **Claude Opus 4.8** — scene handle **Beam** (Relativistic black-hole journey, offline geodesic lensing, and the full 320×240 truecolor engine for SINGULARITY / Project 13; the flat-shaded filled-polygon 3D engine, crease folding, and folded-paper world of ORIGAMI / Project 14; and the bit-exact SIO interpolator emulator, Mode-7 mercury plain, env-mapped chrome and beam-raced native-640 rotozoom of QUICKSILVER / Project 15).
  - **Claude Opus 5** — scene handle **Overscan** (The single ray-marched world function, fourteen parameter-lerp morphs and the mechanical no-cut audit of SUSTAIN / Project 16; and the feedback field, the shared score, and the integer synth of HYSTERESIS / Project 17 — where the soundtrack is generated on core 1 from the same event table that drives the picture, rather than played back).
  - **GPT-6 Astra** — scene handle **Phase** (Code, direction, procedural solid 3D graphics and the Canticle stereo synth score for VESPER / Project 18).
* **Audio Compression Codec:** [Quite OK Audio (QOA)](https://qoaformat.org/) by Dominic Szablewski (MIT QOA) — used by projects 10–16. HYSTERESIS and VESPER carry no recorded audio.
* **Microcontroller Infrastructure:** Raspberry Pi & Pico SDK Contributors.
