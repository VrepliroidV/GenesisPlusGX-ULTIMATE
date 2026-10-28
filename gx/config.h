/****************************************************************************
 *  config.c
 *
 *  Genesis Plus GX configuration file support
 *
 *  Copyright Eke-Eke (2007-2026)
 *
 *  Modified 2026 by VrepliroidV: Auto Crop settings (config.autocrop and
 *  config.autocrop_scale) and the automatic 68k overclock
 *  (config.m68k_auto_overclock, config.s68k_auto_overclock), stored at the end of the settings file; files
 *  from before them still load.
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#define CONFIG_VERSION "GPGX 1.7.5 CF-01"

/****************************************************************************
 * Config Option 
 *
 ****************************************************************************/
typedef struct 
{
  char version[16];
  uint8 hq_fm;
  uint8 filter;
  uint8 hq_psg;
  uint8 ym2612;
  uint8 ym2413;
  uint8 mono;
  int16 psg_preamp;
  int16 fm_preamp;
  int16 cdda_volume;
  int16 pcm_volume;
  int16 low_freq;
  int16 high_freq;
  int16 lg;
  int16 mg;
  int16 hg;
  uint32 lp_range;
  uint8 system;
  uint8 region_detect;
  uint8 master_clock;
  uint8 vdp_mode;
  uint8 force_dtack;
  uint8 addr_error;
  uint8 bios;
  uint8 lock_on;
  uint8 add_on;
  uint8 hot_swap;
  uint8 invert_mouse;
  uint8 gun_cursor[2];
  uint8 overscan;
  uint8 gg_extra;
  uint8 lcd;
  uint8 ntsc;
  uint8 vsync;
  uint8 render;
  uint8 enhanced_vscroll;
  uint8 enhanced_vscroll_limit;
  uint8 tv_mode;
  uint8 bilinear;
  uint8 vfilter;
  uint8 aspect;
  uint8 cd_latency;
  int16 xshift;
  int16 yshift;
  int16 xscale;
  int16 yscale;
#ifdef HW_RVL
  uint8 trap;
  float gamma;
#else
  uint8 v_prog;
#endif
  t_input_config input[MAX_INPUTS];
  uint16 pad_keymap[4][MAX_KEYS+1];
#ifdef HW_RVL
  uint32 wpad_keymap[4*3+1][MAX_KEYS+1];
  uint8 autosleep;
  int32 calx;
  int32 caly;
#endif
  uint8 autoload;
  uint8 autocheat;
  uint8 s_auto;
  uint8 s_default;
  uint8 s_device;
  uint8 l_device;
  uint8 bg_overlay;
  uint8 cd_leds;
  uint8 fps;
  uint8 analog_sensitivity;
  int16 screen_w;
  float bgm_volume;
  float sfx_volume;
  char lastdir[FILETYPE_32X][TYPE_RECENT][MAXPATHLEN];  /* the 32X one is lastdir_32x */
  float ntsc_sharpness;
  float ntsc_resolution;
  float ntsc_artifacts;
  float ntsc_fringing;
  float ntsc_bleed;
  float m68k_overclock;
  float s68k_overclock;
  float z80_overclock;
  char sys_rom[12][256];
  uint8 autocrop;  /* AUTOCROP_OFF, AUTOCROP_FIT, AUTOCROP_STRETCH (gx/utils/autocrop.h) */
  uint8 autocrop_scale;  /* AUTOCROP_SCALE_SMOOTH, AUTOCROP_SCALE_INTEGER */
  uint8 m68k_auto_overclock;  /* 1: m68k_overclock only when the game slows down (core/autooc.c) */
  uint8 s68k_auto_overclock;  /* same for the Mega CD sub 68k and s68k_overclock */
  char bezel[128];  /* "" off, "<auto>" by the game's name, or a PNG in /genplusultimate/bezels (gx/utils/bezel.h) */
  char menu_music[128];  /* MENU_MUSIC_OFF, MENU_MUSIC_RANDOM or an OGG file in /genplusultimate/music (gx_audio.c) */
  char lastdir_32x[TYPE_RECENT][MAXPATHLEN];  /* last 32X game directory of each device */
  char sys_rom_32x[3][256];  /* 32X BIOS: 68000, master SH-2, slave SH-2 (gx/pico32x.h) */
  /* settings of PicoDrive for the 32X games (gx/pico32x.h, 32X Settings) */
  uint16 p32x_msh2;            /* master SH-2 speed, percent of PicoDrive's */
  uint16 p32x_ssh2;            /* slave SH-2 speed, percent of PicoDrive's */
  uint16 p32x_m68k_overclock;  /* 68k overclock in percent, 0 off */
  uint8 p32x_drc;              /* 1: SH-2 recompilers */
  uint8 p32x_renderer;         /* PICO32X_RENDERER_* */
  uint8 p32x_idle;             /* 1: 68k idle loop detection */
  uint8 p32x_pwm;              /* 1: PWM sound */
  uint8 p32x_pwm_irq_opt;      /* 1: PWM IRQ optimization */
  uint8 p32x_sprlim;           /* 1: no sprite limit */
  uint8 p32x_ym2612;           /* FM chip: 1 YM2612, 0 YM3438 */
  uint8 p32x_fm_filter;        /* 1: FM filtering */
  uint8 p32x_lowpass;          /* low-pass filter in percent, 0 off */
  uint8 p32x_auto_oc;          /* 1: SH-2 speeds automatic, up to p32x_msh2/ssh2 */
} t_config;

/* The last directory of a file type: the 32X one was added at the end of
   the settings, so older config.ini files can still be read */
#define CONFIG_LASTDIR(type, device) (((type) == FILETYPE_32X) ? config.lastdir_32x[device] : config.lastdir[type][device])

/* System ROM i: 0-11 as in Genesis Plus GX, 12-14 the 32X BIOS */
#define SYS_ROM_COUNT 15
#define CONFIG_SYS_ROM(i) (((i) >= 12) ? config.sys_rom_32x[(i) - 12] : config.sys_rom[i])

/* Menu music (config.menu_music): none, one of the OGG files of the music
   folder picked by chance at every start, or one file name ("<" is not
   allowed in FAT file names) */
#define MENU_MUSIC_OFF    ""
#define MENU_MUSIC_RANDOM "<random>"

extern t_config config;

extern void config_save(void);
extern void config_default(void);


#endif /* _CONFIG_H_ */

