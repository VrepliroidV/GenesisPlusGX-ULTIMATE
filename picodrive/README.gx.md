# PicoDrive in Genesis Plus GX ULTIMATE

The Sega 32X games of Genesis Plus GX ULTIMATE run on this copy of
[PicoDrive](https://github.com/irixxxx/picodrive) (notaz, irixxxx, fDave and
others, see AUTHORS), commit `07abb00550528f7bf53087d932dd3d747cf2d0df`,
through its libretro core (`platform/libretro`). PicoDrive's license is in
COPYING: non-commercial use, modified versions with their source code.

Left out of this copy: the platform front ends other than `common`,
`libretro` and `libpicofe`, the ARM CPU cores (`cpu/cyclone`, `cpu/DrZ80`),
libchdr (CHD images, built with `use_libchdr=0`), and of `dr_libs` all but
`dr_mp3.h`.

Changed: `Byteswap()` in `pico/cart.c` copies the data on big endian CPUs
when the destination is another buffer (it did nothing there, so the 32X BIOS
files were never copied on the Wii and the games stayed on a black screen).

Changed: `platform/libretro/libretro.c` (built with `GPGX_ULTIMATE`) also
reads the 32X settings of PicoDrive's own menu as options: PWM sound, PWM IRQ
optimization, idle loop detection and the SH-2 cycles (`picodrive_gx_*`), and
sets the SH-2 cores up again when the recompiler is turned on or off during a
game. They are in Genesis Plus GX ULTIMATE's System Settings > 32X Settings.

Added: `Makefile.gx` and `pico32x_keep.txt`, which build the core for the Wii
and make `picodrive_core.o` of it (see `Makefile.wii`), and this file. The front end
is `gx/pico32x.c`.
