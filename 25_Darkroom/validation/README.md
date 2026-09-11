# Validation evidence

This directory contains the records needed to support and reproduce DARKROOM's
hardware-validation claims.

- `release.json` is the checked release manifest.
- `release_01` and `release_02` retain the two complete device runs. Each run
  has its raw serial log, parsed JSON result, device/programming transcript and
  immutable copy of the UF2 that was flashed. The two snapshots intentionally
  match the distributed `darkroom_vga_rp2350.uf2` byte for byte.
- `development_platform_01`, `development_optimized_01` and `development_03`
  record the renderer performance problem and the intermediate fixes described
  in [HARDWARE_VALIDATION.md](../HARDWARE_VALIDATION.md). The last profile includes its unique programmed
  snapshot because it was the build immediately before the final palette fix.
- `host_checks.txt` records the whole-production host test result.

Compiler output, generated host binaries and visual-development scratch files
are reproducible working material and are not retained here.
