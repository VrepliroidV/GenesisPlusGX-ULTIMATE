/****************************************************************************
 *  Genesis Plus GX ULTIMATE
 *  Paprium cartridge chip (high level emulation, punchium.c)
 *
 *  From Genesis Plus GX PUNCHiUM (pav1388, "Project Little Man"), see
 *  punchium.c. Same license as Genesis Plus GX (see LICENSE.txt).
 ****************************************************************************/

#ifndef _PUNCHIUM_H_
#define _PUNCHIUM_H_

/* VDP DMA from the 68k bus ends at once while a Paprium cartridge is used
   (vdp_ctrl.c) */
extern int fast_dma_hack;

/* settings: decoded tiles kept (1 MB), one-hit kill cheat (at reset) */
extern int punchium_tile_cache;
extern int punchium_cheat_saitama;

extern void punchium_cart_init(void);   /* md_cart_init: memory map, chip */
extern void punchium_cart_reset(void);  /* md_cart_reset */
extern void punchium_shutdown(void);    /* another game: music file, cache */
extern void punchium_audio(int samples); /* frame of sound into snd.blips[3] (48 kHz) */
extern int  punchium_context_save(uint8 *state);
extern int  punchium_context_load(uint8 *state);

#endif
