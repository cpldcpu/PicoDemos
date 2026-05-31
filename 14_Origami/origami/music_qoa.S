/* Bake music.qoa (the QOA-encoded soundtrack) into flash.
 *
 * music.qoa is produced from assets/Marimba Seedbox.mp3 by:
 *   ffmpeg -i "assets/Marimba Seedbox.mp3" -ac 1 -ar 22050 -f s16le music.raw
 *   ./tools/qoaconv_s16.exe music.raw music.qoa 22050 1
 *
 * Decoded at runtime by audio_qoa.c. CMakeLists tracks music.qoa as a
 * dependency so re-encoding the soundtrack triggers a rebuild.
 */

    .section .rodata.music_qoa, "a"
    .balign 4

    .global _music_qoa_start
_music_qoa_start:
    .incbin "music.qoa"
    .global _music_qoa_end
_music_qoa_end:
