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

Changed: `PicoCartUnload()` in `pico/cart.c` undoes the idle loop patches
(`SekFinishIdleDet()`) before it frees the 32X memory, not after: the patches
of a 32X game are often in its 68k ROM bank, in that memory, and were written
back into freed memory when the game was unloaded.

Changed: `platform/libretro/libretro.c` (built with `GPGX_ULTIMATE`) also
reads the 32X settings of PicoDrive's own menu as options: PWM sound, PWM IRQ
optimization, idle loop detection and the SH-2 cycles (`picodrive_gx_*`), and
sets the SH-2 cores up again when the recompiler is turned on or off during a
game. They are in Genesis Plus GX ULTIMATE's System Settings > 32X Settings.

Changed: `retro_unload_game()` (with `GPGX_ULTIMATE`) frees the game (ROM, 32X
memory, recompiler tables, about 11 MB with a 4 MB game) at once, instead of
at the next load: Genesis Plus GX ULTIMATE goes on with its own games.

Changed: `pico/32x/32x.c` (with `GPGX_ULTIMATE`) counts the time each SH-2
runs (`gx_sh2_run_cycles`, with `gx_32x_cycles()` for the 68k cycle count),
for the automatic SH-2 overclock of `gx/pico32x.c`, which sets the speeds with
`Pico32xSetClocks()`.

Not changed, used as it is: the frame skip of the libretro core
(`picodrive_frameskip` "auto"), with `gx/pico32x.c` as the frontend's audio
buffer status: it says which frames to skip (32X Settings > Frame Skip).

Added: `Makefile.gx` and `pico32x_keep.txt`, which build the core for the Wii
and make `picodrive_core.o` of it (see `Makefile.wii`), and this file. The front end
is `gx/pico32x.c`.
