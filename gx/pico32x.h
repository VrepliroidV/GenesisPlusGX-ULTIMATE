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
extern int  pico32x_frame(void);                       /* input, one frame, picture into bitmap
                                                          unless it is skipped: 1 if drawn */
extern void pico32x_frame_end(void);                   /* after the frame's sound and picture
                                                          were handed over, before waiting */
extern int  pico32x_audio(int16 *out);                 /* the frame's samples, for gx_audio_Update */
extern void pico32x_reset(void);

/* save states and backup RAM (file_slot.c) */
extern int  pico32x_state_size(void);
extern int  pico32x_state_save(uint8 *buffer, int size);
extern int  pico32x_state_load(const uint8 *buffer, int size);
extern void pico32x_sram_to_gpgx(void);   /* the game's backup RAM into sram.sram */
extern void pico32x_sram_from_gpgx(void); /* sram.sram into the game's backup RAM */

extern const char *pico32x_last_error(void); /* the core's last error message */

/* PicoDrive's settings for the 32X games (config.p32x_*, menu 32X Settings) */
#define PICO32X_RENDERER_GOOD     0
#define PICO32X_RENDERER_ACCURATE 1
#define PICO32X_RENDERER_FAST     2
#define PICO32X_SH2_KHZ           18409  /* PicoDrive's SH-2 speed (100%): 2.4x the 68k clock */
#define PICO32X_FRAMESKIP_OFF     0
#define PICO32X_FRAMESKIP_AUTO    1      /* when the frame would be late */
#define PICO32X_FRAMESKIP_MAX     5      /* 2 to 5: 1 to 4 frames skipped after each one drawn */
extern void pico32x_config_default(void);
extern void pico32x_config_check(void);      /* values out of range back to the defaults */
extern void pico32x_options_changed(void);   /* PicoDrive takes the settings again */

/* Automatic SH-2 overclock (config.p32x_auto_oc): the SH-2 speeds go up to
   the ones of the settings only while the game slows down */
#define PICO32X_AUTO_OC_MAX 200   /* the speeds when it is turned on at 100% */
extern void pico32x_autooc_reset(void);       /* no overclock until the game settles */
extern int  pico32x_autooc_text(char *text, int size); /* on-screen state, 0 when off */
extern int  pico32x_sh2_speed(int slave);     /* speed in use, percent */

#endif
