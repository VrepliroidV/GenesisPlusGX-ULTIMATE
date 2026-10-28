/****************************************************************************
 *  Genesis Plus GX ULTIMATE
 *
 *  Sega 32X games, emulated by PicoDrive (picodrive/, its libretro core).
 *  Genesis Plus GX keeps the menus, the controllers, the video (bezels,
 *  filters, auto crop) and the sound output; PicoDrive runs the game.
 *
 *  Copyright (c) 2026 VrepliroidV
 *  Same license as Genesis Plus GX and PicoDrive: non-commercial use, the
 *  source code with any modified version.
 ****************************************************************************/

#ifndef _PICO32X_H_
#define _PICO32X_H_

#include "types.h"

/* the 32X BIOS files: config.sys_rom_32x[], also System ROM Paths 12-14 */
#define PICO32X_BIOS_68K    0
#define PICO32X_BIOS_MASTER 1
#define PICO32X_BIOS_SLAVE  2

extern int pico32x_active;           /* a 32X game is loaded */

extern int  pico32x_is_32x_file(const char *filename); /* .32x */
extern int  pico32x_load(const char *filename);        /* 1: loaded */
extern void pico32x_unload(void);
extern void pico32x_start(void);                       /* after the game was loaded (reloadrom) */
extern void pico32x_frame(void);                       /* input, one frame, picture into bitmap */
extern int  pico32x_audio(int16 *out);                 /* the frame's samples, for gx_audio_Update */
extern void pico32x_reset(void);

/* save states and backup RAM (file_slot.c) */
extern int  pico32x_state_size(void);
extern int  pico32x_state_save(uint8 *buffer, int size);
extern int  pico32x_state_load(const uint8 *buffer, int size);
extern void pico32x_sram_to_gpgx(void);   /* the game's backup RAM into sram.sram */
extern void pico32x_sram_from_gpgx(void); /* sram.sram into the game's backup RAM */

extern const char *pico32x_last_error(void); /* the core's last error message */

#endif
