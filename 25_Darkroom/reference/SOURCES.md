# Reference sources and hashes

Hashes are SHA-256 over the exact local bytes. Download dates were not recorded,
so this manifest does not invent them.

## Downloaded source material

| Local file | Bytes | Source | SHA-256 |
|---|---:|---|---|
| `STELLAR-DarkRoom` | 31,480 | [amigascne original executable](https://ftp.amigascne.org/pub/amiga/Groups/S/Stellar/STELLAR-DarkRoom) | `dae19d8e3660b11de27963bf7e7fb24d8a3d0c099be6366a798cb7663b9d10e1` |
| `darkroom.mod` | 57,862 | [Modland mirror at amigascne](https://ftp.amigascne.org/mirrors/ftp.modland.com/pub/modules/Protracker/Strobo/darkroom.mod) | `fdf3ed2fb0184ff66b6647c0dbebc5efa236d901e4603aa036fad5b2018105ce` |
| `darkr.zip` | 1,220,499 | [scene.org mirror package](https://archive.scene.org/pub/mirrors/amidemos/darkr.zip) | `ebf393083d73dea0ed5b0ead2598a373f0cb6edbcbfc71cb6cff5d09369a14be` |
| `reference_50hz.mp4` | 28,927,560 | [Amiga Demos YouTube reference](https://www.youtube.com/watch?v=fzOpicgCU8E) | `6ed33c5e3f5423c28f3b87cac01d5f3d76b216114ce62ff0a3734ad99a85e101` |

The MP4 reports 954×720 H.264 video at 50 fps and 44.1 kHz stereo AAC. Its
124.900-second container duration includes material outside the reconstructed
80-second presentation, so container duration is not used as the demo length.

## Archive members and extracted program data

| Local file | Bytes | Provenance | SHA-256 |
|---|---:|---|---|
| `file_id.diz` | 1,159 | Member of `darkr.zip` | `930fce73767e4d82c49a7bf4de54c40ef051eb4b1d4c8b72a4e33865107c35da` |
| `darkr.avi` | 1,292,288 | Member of `darkr.zip`; low-quality 352×288 capture | `1677742b8f87442ea5c2ea6d00fbc13ac14ffbe4fd1e63e53828f85617cad9bf` |
| `unpacked.bin` | 41,116 | Offline PowerPacker unpacking of `STELLAR-DarkRoom` | `0c0018c363273238b6641087f466b9b470af14d2004e337c96cb0a10c2ed2483` |
| `hunk0.bin` | 38,952 | Code hunk extracted from `unpacked.bin` | `ac01a55fb384196ad02202334d52998d9fcb7da6c95b9cb5cfe839dab3c891f5` |
| `hunk3.bin` | 1,884 | Copper/data hunk extracted from `unpacked.bin` | `f9122e11becedbd5f6d82ac5164e3229aae49afaebf7ad723d5a0c72905c0f1a` |
| `disassembly.txt` | 454,159 | Disassembly derived from the unpacked executable | `0b7a5c027813155d93111eef91ad0c4352bb7902e73e0a0a4d490c99424fb68d` |
| `relocations.json` | 1,773 | Relocation map derived from the unpacked executable | `728944015f4fdc45a36884a050d78820de09e8c4553013ca1158737905e8d930` |

## Review derivatives

The remaining PNG and contact-sheet files are working analysis outputs. They
help compare timing and visuals but are not original Stellar assets.

| Local file | Bytes | SHA-256 |
|---|---:|---|
| `screen.png` | 38,302 | `9cae5bbb89a5df91adb8399a9ceba7c87459f50751b7ef30c26999abf767c5d0` |
| `frame_036.png` | 184,400 | `29bdefbc296321ac69bc2e4d52d601529b4a3546afc37cb5fdcd1a64cc77e9e4` |
| `frame_068.png` | 158,481 | `e1aeb46af135a16582ecf5a9d575c4b310c82a0a70d6745adf682a8a54c36f6f` |
| `early_1.png` | 6,280 | `106f0200defd226686179cf8c1870b63b13d0d2a86a48c41e0edf67d03c3b7fe` |
| `early_2.png` | 5,209 | `b39692636a54f171688a4826e531253266c3970cbe3125999c82e87370c162d6` |
| `early_3.png` | 8,051 | `22d1f8191526d26a9de978508d8323cd7de0cc2acfce4c8fb125fb34bc62c27c` |
| `early_4.png` | 11,783 | `f468bcc2da1a48bbefcf6d780c704fb6a9f4413b60ba036e39968a20dee5a35a` |
| `early_5.png` | 18,839 | `f9033191b3ea5ad5acb0e62ce8594c0517fb09f4912eb9a8a178c6d2e0175efb` |
| `early_6.png` | 31,577 | `2037400f7ef6502ea5ebff44196287bf1d829cbbc43dc220e6d9970c2e4fa0d0` |
| `early_8.png` | 84,259 | `238884305336cc1a2ec6797b04b5af89868c3f7aaf60ed4f1bcad4a5a0f9579c` |
| `contact.png` | 593,375 | `b07896b5d628a72a5d2f9b088016513e25f0740bacc93e47721f1980d113812d` |
| `contact_hq.png` | 1,379,066 | `e30d1da9d60d3f1c3f4253f59092d38f076c8e38c9977a2f987a60946c69a34b` |
| `compare_055.png` | 35,564 | `b8f1823887a83b258e19be114d0745dfcd711a39fe3c5722274b28066b15b9da` |
| `music_audit.md` | 4,192 | `fc35416f4386100550884c15eed62928cc18192bb78b6652baceba05ebad1d2d` |
| `pt2_tables.c` | 17,447 | `a3e39e77a8f8288ba8374aa8dd41faf90ac81ccbc9f18c984e6e5976a3fbbb20` |
| `pt2_LICENSE` | 1,527 | `a17d758d5cf35c30bc53ba93de3e71df6a3e6e76652d31b761e61b4028b518cc` |

`pt2_tables.c` and `pt2_LICENSE` preserve the source and license record for the
standard ProTracker finetune period table transcribed into `periods.h`; they are
supporting implementation references, not material from Darkroom.

The original production and music remain credited to their authors. Inclusion
here records the exact evidence used for study and reproducibility and does not
assert ownership or a new license over the 1994 material.
