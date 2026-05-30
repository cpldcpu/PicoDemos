#!/usr/bin/env python3
"""make_lens_lut.py — bake the gravitational-lensing remap LUT for SINGULARITY.

For every one of the 160x120 screen pixels we trace a photon BACKWARD from
the camera through the curved Schwarzschild spacetime and record where it
ends up:

  * captured by the event horizon  -> shadow sentinel (0xFFFF)
  * escapes to infinity            -> the asymptotic direction, mapped to
                                      equirectangular panorama (u,v)

The device then only does a table lookup + one panorama tap per pixel — no
runtime geodesic math. The Einstein ring and the black shadow disk emerge
automatically from the physics (the photon-sphere critical impact parameter
b = 3*sqrt(3)*M), nothing is hand-placed.

Integration: the equatorial null geodesic in u = 1/r,
    u'' = -u + 3*M*u^2            (RK4, vectorised over all pixels)
with the camera at radius r0 looking at the black hole. Output is packed as
uint16 per pixel:  value == 0xFFFF -> shadow; else (u<<7)|v with the 256x128
panorama (u: 8 bits, v: 7 bits).

Outputs (all checked in so a clone builds without Python):
    ../assets/_packed/lens_lut.bin   raw uint16 LUT, little-endian
    ../assets/_packed/lens_lut.h     extern + dims + sentinel
    ../assets/_packed/lens_lut.S     .incbin wrapper for the build
    lens_preview.png                 sanity render of the LUT over the pano
"""

import struct
from pathlib import Path
import numpy as np

HERE   = Path(__file__).resolve().parent
OUT    = HERE.parent / "assets" / "_packed"
PANO   = HERE.parent / "assets" / "star_pano_256x128.png"

# Screen / camera ---------------------------------------------------------
FB_W, FB_H = 320, 240        # MODE_HIRES — full native resolution
FOCAL      = FB_H * 0.72      # vertical FOV; tuned with R0 so the shadow disk
                             # fills ~the central third and a bright Einstein
                             # ring of lensed sky wraps around it.
M    = 1.0                   # black-hole mass (G=c=1) -> r_s = 2M = 2
R_S  = 2.0 * M
R0   = 14.0                  # camera radius (7 r_s out) — strong cinematic bend
                             # while keeping plenty of lensed sky on screen

# Panorama dims (must match pack_assets.py star_pano) ---------------------
PANO_W, PANO_H = 256, 128
SHADOW = 0xFFFF

# Integration params ------------------------------------------------------
DPHI   = 0.01
PHIMAX = 8.0 * np.pi
U_CAP  = 1.0 / R_S           # u >= this  <=>  r <= r_s  -> captured


def bake():
    # Per-pixel primary ray directions in world space (camera at (0,0,-R0)
    # looking +z toward the BH at the origin).
    px = (np.arange(FB_W) - (FB_W - 1) / 2.0)
    py = (np.arange(FB_H) - (FB_H - 1) / 2.0)
    gx, gy = np.meshgrid(px, py)            # shape (H, W)
    dx = gx / FOCAL
    dy = gy / FOCAL
    dz = np.ones_like(dx)
    inv = 1.0 / np.sqrt(dx * dx + dy * dy + dz * dz)
    dx, dy, dz = dx * inv, dy * inv, dz * inv

    # e_r (radial outward) = (0,0,-1); cos(psi) = d.z; in-plane perp u2 is the
    # normalised screen-radial direction (dx,dy,0).
    cpsi = np.clip(dz, -1.0, 1.0)
    psi  = np.arccos(cpsi)
    hyp  = np.sqrt(dx * dx + dy * dy)
    safe = hyp > 1e-6
    ux = np.where(safe, dx / np.where(safe, hyp, 1.0), 0.0)
    uy = np.where(safe, dy / np.where(safe, hyp, 1.0), 0.0)

    # Initial state: u0 = 1/R0, w0 = du/dphi = u0 / tan(psi) (incoming).
    u  = np.full((FB_H, FB_W), 1.0 / R0)
    tan = np.tan(psi)
    w  = np.where(np.abs(tan) > 1e-6, u / tan, 1e9)   # psi~0 -> radial plunge

    active   = np.ones((FB_H, FB_W), dtype=bool)
    captured = np.zeros((FB_H, FB_W), dtype=bool)
    phi_esc  = np.zeros((FB_H, FB_W))           # phi at escape (u crosses 0)
    escaped  = np.zeros((FB_H, FB_W), dtype=bool)

    def accel(uu):
        return -uu + 3.0 * M * uu * uu

    phi = 0.0
    while phi < PHIMAX and active.any():
        u_prev = u.copy()
        # RK4 on [u, w], w = du/dphi, u'' = -u + 3 M u^2.
        k1u = w;              k1w = accel(u)
        k2u = w + 0.5*DPHI*k1w; k2w = accel(u + 0.5*DPHI*k1u)
        k3u = w + 0.5*DPHI*k2w; k3w = accel(u + 0.5*DPHI*k2u)
        k4u = w + DPHI*k3w;     k4w = accel(u + DPHI*k3u)
        un = u + (DPHI/6.0)*(k1u + 2*k2u + 2*k3u + k4u)
        wn = w + (DPHI/6.0)*(k1w + 2*k2w + 2*k3w + k4w)
        phi_n = phi + DPHI

        # Capture: u climbed past the horizon.
        cap_now = active & (un >= U_CAP)
        captured |= cap_now
        active &= ~cap_now

        # Escape: u crossed from positive to <= 0 (photon back out to infinity).
        esc_now = active & (un <= 0.0) & (u_prev > 0.0)
        # interpolate phi where u == 0
        denom = (u_prev - un)
        frac  = np.where(np.abs(denom) > 1e-12, u_prev / denom, 0.0)
        phi_cross = phi + DPHI * frac
        phi_esc = np.where(esc_now, phi_cross, phi_esc)
        escaped |= esc_now
        active &= ~esc_now

        # Freeze finished rays at 0 so their (possibly huge) u doesn't keep
        # being integrated and overflow — results are already recorded.
        u = np.where(active, un, 0.0)
        w = np.where(active, wn, 0.0)
        phi = phi_n

    # Any ray still active at PHIMAX is treated as captured (numerically stuck
    # near the photon sphere) — reads as part of the shadow, which is correct.
    captured |= active

    # Asymptotic escape direction: dir = sin(phi)*u2 + cos(phi)*e_r, with
    # e_r=(0,0,-1).  => (sin*ux, sin*uy, -cos).
    s = np.sin(phi_esc)
    c = np.cos(phi_esc)
    Dx = s * ux
    Dy = s * uy
    Dz = -c

    # Equirectangular mapping (matches spheres.c envmap_sample_dir).
    lon = np.arctan2(Dx, Dz)
    uidx = ((lon / (2.0 * np.pi) + 0.5) * PANO_W).astype(np.int64) % PANO_W
    xz   = np.sqrt(Dx * Dx + Dz * Dz)
    lat  = np.arctan2(-Dy, xz)
    vidx = ((lat / np.pi + 0.5) * PANO_H).astype(np.int64)
    vidx = np.clip(vidx, 0, PANO_H - 1)

    lut = ((uidx.astype(np.uint16) << 7) | vidx.astype(np.uint16)).astype(np.uint16)
    lut[captured] = SHADOW

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "lens_lut.bin").write_bytes(lut.tobytes())
    n_shadow = int(captured.sum())
    print(f"  baked lens_lut.bin: {FB_W}x{FB_H} uint16 = {lut.nbytes} B, "
          f"{n_shadow} shadow px ({100.0*n_shadow/lut.size:.1f}%)")

    write_h()
    write_s()
    preview(lut)


def write_h():
    txt = f"""/* GENERATED by tools/make_lens_lut.py — do not edit by hand.
 * Gravitational-lensing remap LUT for the SINGULARITY climax. */
#ifndef SINGULARITY_LENS_LUT_H
#define SINGULARITY_LENS_LUT_H
#include <stdint.h>

#define LENS_LUT_W      {FB_W}
#define LENS_LUT_H      {FB_H}
#define LENS_PANO_W     {PANO_W}
#define LENS_PANO_H     {PANO_H}
#define LENS_SHADOW     0xFFFFu

/* {FB_W}*{FB_H} uint16: 0xFFFF == event-horizon shadow, else (u<<7)|v
 * into the {PANO_W}x{PANO_H} equirectangular star panorama. */
extern const uint16_t lens_lut[LENS_LUT_W * LENS_LUT_H];

#endif
"""
    (OUT / "lens_lut.h").write_text(txt, encoding="utf-8")


def write_s():
    txt = """/* GENERATED by tools/make_lens_lut.py — do not edit by hand. */
    .section .rodata.lens_lut, "a"
    .balign 4
    .global lens_lut
lens_lut:
    .incbin "lens_lut.bin"
"""
    (OUT / "lens_lut.S").write_text(txt, encoding="utf-8")


def preview(lut):
    """Render the LUT against the panorama so the Einstein ring is visible
    before baking into the firmware."""
    try:
        from PIL import Image
    except ImportError:
        return
    pano = np.asarray(Image.open(PANO).convert("RGB").resize((PANO_W, PANO_H)))
    out = np.zeros((FB_H, FB_W, 3), dtype=np.uint8)
    shadow = lut == SHADOW
    uidx = (lut >> 7) & 0x1FF
    vidx = lut & 0x7F
    uidx = np.clip(uidx, 0, PANO_W - 1)
    vidx = np.clip(vidx, 0, PANO_H - 1)
    out = pano[vidx, uidx]
    out[shadow] = (0, 0, 0)
    img = Image.fromarray(out).resize((FB_W * 3, FB_H * 3), Image.NEAREST)
    img.save(HERE / "lens_preview.png")
    print(f"  wrote {HERE/'lens_preview.png'} (sanity check)")


if __name__ == "__main__":
    bake()
