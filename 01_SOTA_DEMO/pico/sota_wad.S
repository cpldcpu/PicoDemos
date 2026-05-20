/* Bake sota.wad into the firmware so it lives in flash (XIP). Accessible
 * at runtime as &_sota_wad_start with size _sota_wad_end - _sota_wad_start.
 * Used by the ST7789 and composite builds, which don't play audio (their
 * sound_stub leaves the audio wad entries unread). The VGA build uses
 * sota_wad_qoa.S to incbin sota_pico.wad instead.
 *
 * We rely on the SDK's normal .rodata placement to send this section into
 * XIP flash; no custom linker script needed for a wad of this size. */

    .section .rodata.sota_wad, "a"
    .balign 4

    .global _sota_wad_start
_sota_wad_start:
    .incbin "sota.wad"
    .global _sota_wad_end
_sota_wad_end:

    .global _sota_wad_size
    .balign 4
_sota_wad_size:
    .word _sota_wad_end - _sota_wad_start
