/****************************************************************************
 *  Genesis Plus GX ULTIMATE
 *
 *  Sega 32X games, emulated by PicoDrive (picodrive/, its libretro core,
 *  linked as picodrive_core.o, see picodrive/Makefile.gx). Genesis Plus GX keeps
 *  the menus, the controllers, the video output (bezels, filters, auto crop)
 *  and the sound output; PicoDrive runs the game, with its SH-2 recompiler.
 *
 *  Copyright (c) 2026 VrepliroidV
 *  Same license as Genesis Plus GX and PicoDrive: non-commercial use, the
 *  source code with any modified version.
 ****************************************************************************/

#include <stdarg.h>
#include <strings.h>
#include "shared.h"
#include "../picodrive/platform/libretro/libretro-common/include/libretro.h"
#include "pico32x.h"
#include "file_load.h"
#include "gx_audio.h"
#include <ogc/lwp_watchdog.h>

/* PicoDrive: the 32X BIOS images, NULL for its built-in replacement */
extern void *p32x_bios_g, *p32x_bios_m, *p32x_bios_s;

/* PicoDrive (picodrive/pico/32x/32x.c): SH-2 clocks, and the time each SH-2
   ran and the 68k cycle count, for the automatic overclock */
extern void Pico32xSetClocks(int msh2_hz, int ssh2_hz);
extern unsigned int gx_sh2_run_cycles[2];
extern unsigned int gx_32x_cycles(void);

/* PicoDrive (picodrive/pico/32x/32x.c, platform/libretro/libretro.c): the
   68k cycles run and waited and its overclock, and the cheats */
extern unsigned int gx_m68k_cycles, gx_m68k_idle;
extern unsigned int gx_m68k_vint_cycles, gx_m68k_vint_idle;
extern void gx_m68k_set_overclock(int percent);
extern int gx_cheat_rom(unsigned int addr, unsigned int data);
extern void gx_cheat_code_changed(void);
extern void gx_cheat_ram(unsigned int addr, unsigned int data, int word);

int pico32x_active = 0;

static int core_ready = 0;
static int vars_changed = 1;
static struct retro_system_av_info av;
static char game_path[MAXPATHLEN];
static char system_dir[MAXPATHLEN];
static char save_dir[MAXPATHLEN];
static char sound_rate_text[16] = "48000";
static char last_error[128];
static u32 pad_bits[2];

/* the 32X BIOS files (68000, master SH-2, slave SH-2) */
static u8 bios_g[0x100] ATTRIBUTE_ALIGN(32);
static u8 bios_m[0x800] ATTRIBUTE_ALIGN(32);
static u8 bios_s[0x400] ATTRIBUTE_ALIGN(32);

/* backup RAM of the game, mirrored in sram.sram for file_slot.c */
static u8 *sram_data = NULL;
static int sram_size = 0;

/* the samples of the last frames, waiting for gx_audio_Update */
#define FIFO_FRAMES 4096
static int16 fifo[FIFO_FRAMES * 2];
static int fifo_len = 0;
static double out_frac = 0.0;   /* samples owed to the next frame */
static double read_pos = 0.0;   /* position in the FIFO, between two samples */

/* Core options: the 32X settings of the menu (config.p32x_*, also per game),
   the sound rate the video needs and the region of the settings */
static char opt_text[4][16];

static const char *option_value(const char *key)
{
  static const char *renderer[] = { "good", "accurate", "fast" };

  if (!strcmp(key, "picodrive_drc"))
    return config.p32x_drc ? "enabled" : "disabled";
  if (!strcmp(key, "picodrive_renderer"))
    return renderer[(config.p32x_renderer < 3) ? config.p32x_renderer : 0];
  if (!strcmp(key, "picodrive_sprlim"))
    return config.p32x_sprlim ? "enabled" : "disabled";
  if (!strcmp(key, "picodrive_overclk68k"))
  {
    /* the level in use with the automatic overclock */
    int oc = pico32x_m68k_overclock();
    if (oc <= 0)
      return "disabled";
    snprintf(opt_text[0], sizeof(opt_text[0]), "+%d%%", oc);
    return opt_text[0];
  }
  if (!strcmp(key, "picodrive_fmchip"))
    return config.p32x_ym2612 ? "ym2612" : "ym3438";
  if (!strcmp(key, "picodrive_fm_filter"))
    return config.p32x_fm_filter ? "on" : "off";
  if (!strcmp(key, "picodrive_audio_filter"))
    return config.p32x_lowpass ? "low-pass" : "disabled";
  if (!strcmp(key, "picodrive_lowpass_range"))
  {
    snprintf(opt_text[1], sizeof(opt_text[1]), "%d", config.p32x_lowpass ? config.p32x_lowpass : 60);
    return opt_text[1];
  }
  if (!strcmp(key, "picodrive_gx_pwm"))
    return config.p32x_pwm ? "enabled" : "disabled";
  if (!strcmp(key, "picodrive_gx_pwm_irq_opt"))
    return config.p32x_pwm_irq_opt ? "enabled" : "disabled";
  if (!strcmp(key, "picodrive_gx_idle_det"))
    return config.p32x_idle ? "enabled" : "disabled";
  if (!strcmp(key, "picodrive_gx_msh2_khz"))
  {
    snprintf(opt_text[2], sizeof(opt_text[2]), "%d", PICO32X_SH2_KHZ * pico32x_sh2_speed(0) / 100);
    return opt_text[2];
  }
  if (!strcmp(key, "picodrive_gx_ssh2_khz"))
  {
    snprintf(opt_text[3], sizeof(opt_text[3]), "%d", PICO32X_SH2_KHZ * pico32x_sh2_speed(1) / 100);
    return opt_text[3];
  }
  if (!strcmp(key, "picodrive_sound_rate"))
    return sound_rate_text;
  if (!strcmp(key, "picodrive_frameskip"))
    /* the frames to skip are chosen here (see Frame skip) */
    return config.p32x_frameskip ? "auto" : "disabled";
  if (!strcmp(key, "picodrive_region"))
  {
    switch (config.region_detect)
    {
      case 1: return "US";
      case 2: return "Europe";
      case 3: return "Japan NTSC";
      case 4: return "Japan PAL";
      default: return "Auto";
    }
  }
  return NULL;
}

void pico32x_config_default(void)
{
  config.p32x_drc = 1;              /* as tested on the Wii: full speed */
  config.p32x_renderer = PICO32X_RENDERER_GOOD;
  config.p32x_msh2 = 100;
  config.p32x_ssh2 = 100;
  config.p32x_m68k_overclock = 0;
  config.p32x_idle = 1;
  config.p32x_pwm = 1;
  config.p32x_pwm_irq_opt = 0;
  config.p32x_sprlim = 0;
  config.p32x_ym2612 = 0;           /* YM3438, as PicoDrive's libretro core */
  config.p32x_fm_filter = 0;
  config.p32x_lowpass = 0;
  config.p32x_auto_oc = 0;
  config.p32x_frameskip = PICO32X_FRAMESKIP_OFF;
  config.p32x_m68k_auto_oc = 0;
}

void pico32x_config_check(void)
{
  if ((config.p32x_drc > 1) || (config.p32x_renderer > PICO32X_RENDERER_FAST) ||
      (config.p32x_msh2 < 25) || (config.p32x_msh2 > 300) ||
      (config.p32x_ssh2 < 25) || (config.p32x_ssh2 > 300) ||
      (config.p32x_m68k_overclock > 400) || (config.p32x_idle > 1) || (config.p32x_pwm > 1) ||
      (config.p32x_pwm_irq_opt > 1) || (config.p32x_sprlim > 1) || (config.p32x_ym2612 > 1) ||
      (config.p32x_fm_filter > 1) || (config.p32x_lowpass > 95) || (config.p32x_auto_oc > 1))
  {
    pico32x_config_default();
  }
  if (config.p32x_frameskip > PICO32X_FRAMESKIP_MAX)
  {
    config.p32x_frameskip = PICO32X_FRAMESKIP_OFF;
  }
  if (config.p32x_m68k_auto_oc > 1)
  {
    config.p32x_m68k_auto_oc = 0;
  }
}

void pico32x_options_changed(void)
{
  vars_changed = 1;
}

/****************************************************************************
 * Automatic overclock of the SH-2s and of the 68k
 *
 * The slowdowns of 32X games come from the two SH-2s, or from the 68k when
 * the game runs on it. Each frame PicoDrive counts the time each SH-2 ran
 * and the time the 68k ran and waited: the rest of the frame they waited
 * (PicoDrive recognises an SH-2 polling a register, the VDP or memory and
 * puts it to sleep; the 68k waits stopped, polling the 32X registers, or on
 * an idle loop found by the idle loop detection). When the busier SH-2 did
 * not wait at all in a frame, or the 68k did not wait before the vertical
 * interrupt (the game's next frame was due), the game is running late, and
 * when that happens again its speed goes up one level; when the frames would
 * still have time left at the level below, it goes down again. The levels go
 * from no overclock to the speeds of the settings, as the automatic 68k
 * overclock of Genesis Plus GX (core/autooc.c), with the same thresholds. A
 * level is never taken when the Wii itself would not make the frame in time
 * (the time each frame took to emulate is measured), and the overclock
 * starts only once the CPU was seen waiting: a 68k whose waiting is not
 * recognised (idle loop detection off, a loop it does not know) is left
 * alone.
 ****************************************************************************/
#define AOC_LEVELS        4
#define AOC_LAG_BUSY      985   /* per mille of the frame the CPU ran: late */
#define AOC_WAIT_BUSY     950   /* at most this: the game waited */
#define AOC_DOWN_BUSY     850   /* at most this at the level below: go down */
#define AOC_CALIBRATE     30    /* frames the CPU must be seen waiting */
#define AOC_UP_WAIT       10    /* frames between two steps up */
#define AOC_DOWN_FRAMES   120   /* frames with time left before a step down */
#define AOC_DOWN_HOLD     180   /* frames without a step down after a step up */
#define AOC_DOWN_HOLD_MAX 1800
#define AOC_BOUNCE        300
#define AOC_HOLD_START    180   /* frames off after a load, a reset or a state */
#define AOC_HOST_MAX      900   /* per mille of the frame time the Wii may use */
#define AOC_HOST_FRAMES   30    /* frames over it before a step down */

typedef struct
{
  int level;
  int pct[AOC_LEVELS][2];        /* speeds of each level in percent: master
                                    and slave SH-2, or the 68k (twice) */
  int busy;                      /* per mille of the last frame the CPU ran */
  int hold, up_wait, down_wait, down_hold, since_down, roomy, waited, host_late;
  unsigned int lag_history;
} t_aoc;

static t_aoc aoc_sh2, aoc_68k;
static int aoc_host;             /* per mille of the frame time the last frame took */
static unsigned int aoc_last_cycles, aoc_last_m68k, aoc_last_m68k_idle;
static int aoc_have_cycles;

static int aoc_pct(float max, int i)
{
  if (max < 1.0)
    max = 1.0;
  return (int)(powf(max, (float)i / (AOC_LEVELS - 1)) * 100.0 + 0.5);
}

static void aoc_levels(void)
{
  int i;
  for (i = 0; i < AOC_LEVELS; i++)
  {
    aoc_sh2.pct[i][0] = aoc_pct(config.p32x_msh2 / 100.0, i);
    aoc_sh2.pct[i][1] = aoc_pct(config.p32x_ssh2 / 100.0, i);
    aoc_68k.pct[i][0] = aoc_68k.pct[i][1] = aoc_pct((100 + config.p32x_m68k_overclock) / 100.0, i);
  }
}

static int aoc_level(const t_aoc *oc)
{
  return (oc->level < AOC_LEVELS) ? oc->level : 0;
}

int pico32x_sh2_speed(int slave)
{
  if (config.p32x_auto_oc)
  {
    aoc_levels();
    return aoc_sh2.pct[aoc_level(&aoc_sh2)][slave ? 1 : 0];
  }
  return slave ? config.p32x_ssh2 : config.p32x_msh2;
}

int pico32x_m68k_overclock(void)
{
  if (config.p32x_m68k_auto_oc)
  {
    aoc_levels();
    return aoc_68k.pct[aoc_level(&aoc_68k)][0] - 100;
  }
  return config.p32x_m68k_overclock;
}

static void aoc_set_level(t_aoc *oc, int level)
{
  oc->level = level;
  oc->lag_history = 0;
  oc->roomy = 0;
  oc->host_late = 0;
  if (!core_ready)
    return;
  if (oc == &aoc_sh2)
    Pico32xSetClocks(PICO32X_SH2_KHZ * pico32x_sh2_speed(0) * 10, PICO32X_SH2_KHZ * pico32x_sh2_speed(1) * 10);
  else
    gx_m68k_set_overclock(pico32x_m68k_overclock());
}

static void aoc_reset(t_aoc *oc)
{
  oc->hold = AOC_HOLD_START;
  oc->up_wait = oc->down_wait = 0;
  oc->down_hold = AOC_DOWN_HOLD;
  oc->since_down = AOC_BOUNCE;
  oc->waited = 0;
  oc->busy = 0;
  aoc_set_level(oc, 0);
}

void pico32x_autooc_reset(void)
{
  aoc_host = 0;
  aoc_have_cycles = 0;
  gx_sh2_run_cycles[0] = gx_sh2_run_cycles[1] = 0;
  aoc_reset(&aoc_sh2);
  aoc_reset(&aoc_68k);
}

int pico32x_autooc_text(char *text, int size)
{
  int len = 0;

  if (!pico32x_active)
  {
    return 0;
  }
  if (config.p32x_auto_oc)
  {
    len += snprintf(text + len, size - len, "SH-2 %d%% %d%%", pico32x_sh2_speed(0), (aoc_sh2.busy + 5) / 10);
  }
  if (config.p32x_m68k_auto_oc && (len < size))
  {
    len += snprintf(text + len, size - len, "%s68K +%d%% %d%%", len ? "\n" : "", pico32x_m68k_overclock(), (aoc_68k.busy + 5) / 10);
  }
  return (len < size) ? len : size - 1;
}

/* one frame of a controller: busy per mille of the frame its CPU ran,
   late when it did not wait in time, waited when it was seen waiting */
static void aoc_step(t_aoc *oc, int busy, int late, int waited, int enabled)
{
  int host_room;

  oc->busy = busy;
  if (!enabled)
  {
    return;
  }
  if (oc->hold > 0)
  {
    oc->hold--;
    oc->lag_history = 0;
    oc->roomy = 0;
    return;
  }

  /* the Wii does not make it: one level down */
  if (aoc_host > AOC_HOST_MAX)
  {
    if ((oc->level > 0) && (++oc->host_late >= AOC_HOST_FRAMES))
    {
      aoc_set_level(oc, oc->level - 1);
      oc->since_down = 0;
      oc->up_wait = AOC_DOWN_HOLD;
    }
    return;
  }
  oc->host_late = 0;

  if (waited && (oc->waited < AOC_CALIBRATE))
    oc->waited++;
  oc->lag_history = (oc->lag_history << 1) | (late ? 1 : 0);
  if (oc->up_wait > 0)
    oc->up_wait--;
  if (oc->down_wait > 0)
    oc->down_wait--;
  if (oc->since_down < AOC_BOUNCE)
    oc->since_down++;

  /* the next level would still leave the Wii time (counted on the whole
     frame, to be safe) */
  host_room = (oc->level < AOC_LEVELS - 1) &&
              ((long long)aoc_host * oc->pct[oc->level + 1][0] <= (long long)AOC_HOST_MAX * oc->pct[oc->level][0]);

  /* late again: one level up */
  if (late && host_room && (oc->waited >= AOC_CALIBRATE) && (oc->up_wait == 0))
  {
    int late_frames = 0;
    unsigned int bits;
    for (bits = oc->lag_history & 0xff; bits; bits &= bits - 1)
      late_frames++;
    if (late_frames >= 2)
    {
      if (oc->since_down < AOC_BOUNCE)
        oc->down_hold = (oc->down_hold * 2 > AOC_DOWN_HOLD_MAX) ? AOC_DOWN_HOLD_MAX : oc->down_hold * 2;
      else
        oc->down_hold = AOC_DOWN_HOLD;
      aoc_set_level(oc, oc->level + 1);
      oc->up_wait = AOC_UP_WAIT;
      oc->down_wait = oc->down_hold;
      return;
    }
  }

  /* enough time left for the level below */
  if (oc->level > 0)
  {
    int slower = (int)((long long)oc->pct[oc->level][0] * 1100 / oc->pct[oc->level - 1][0]);
    if ((long long)busy * slower <= (long long)AOC_DOWN_BUSY * 1000)
      oc->roomy++;
    else
      oc->roomy = 0;
    if ((oc->roomy >= AOC_DOWN_FRAMES) && (oc->down_wait == 0))
    {
      aoc_set_level(oc, oc->level - 1);
      oc->since_down = 0;
    }
  }
}

/* one emulated frame: host_us microseconds to emulate */
static void aoc_end_frame(int host_us)
{
  unsigned int now = gx_32x_cycles();
  unsigned int frame = now - aoc_last_cycles;
  unsigned int m68k = gx_m68k_cycles - aoc_last_m68k;
  unsigned int m68k_idle = gx_m68k_idle - aoc_last_m68k_idle;
  unsigned int run = (gx_sh2_run_cycles[0] > gx_sh2_run_cycles[1]) ? gx_sh2_run_cycles[0] : gx_sh2_run_cycles[1];
  int fps = vdp_pal ? 50 : 60;
  int busy_sh2, busy_68k, late_68k;

  gx_sh2_run_cycles[0] = gx_sh2_run_cycles[1] = 0;
  aoc_last_cycles = now;
  aoc_last_m68k = gx_m68k_cycles;
  aoc_last_m68k_idle = gx_m68k_idle;
  if (!aoc_have_cycles || !frame || (frame > 0x1000000) || !m68k || (m68k > 0x1000000))
  {
    aoc_have_cycles = 1;
    return;
  }

  busy_sh2 = (int)((long long)run * 1000 / frame);
  if (busy_sh2 > 1000)
    busy_sh2 = 1000;
  if (m68k_idle > m68k)
    m68k_idle = m68k;
  busy_68k = 1000 - (int)((long long)m68k_idle * 1000 / m68k);
  aoc_host = host_us * fps / 1000;   /* per mille of 1/fps s */

  /* the SH-2s: late when they did not wait in the frame; the 68k: when it
     did not wait (at most 1.5% of the time) before the vertical interrupt */
  late_68k = ((long long)gx_m68k_vint_idle * 1000 < (long long)(1000 - AOC_LAG_BUSY) * gx_m68k_vint_cycles);
  aoc_step(&aoc_sh2, busy_sh2, busy_sh2 >= AOC_LAG_BUSY, busy_sh2 <= AOC_WAIT_BUSY, config.p32x_auto_oc);
  aoc_step(&aoc_68k, busy_68k, late_68k, !late_68k, config.p32x_m68k_auto_oc);
}

/****************************************************************************
 * Frame skip (config.p32x_frameskip)
 *
 * A skipped frame is emulated without drawing its picture: PicoDrive skips
 * the Mega Drive and 32X layers, the game runs and sounds the same. The last
 * picture stays on the screen, and the frame does not wait for the VSYNC,
 * only for the sound. PicoDrive skips a frame when its frontend says the
 * sound is about to run out (libretro's audio buffer status), which is
 * decided here:
 * - AUTO: when the next frame, drawn, would not be ready before the sound
 *   already handed to the audio DMA runs out (by the time the frames drawn
 *   took), so the game keeps its speed and its sound does not stutter;
 * - 1 to 4: that many frames skipped after each frame drawn.
 * AUTO skips at most FS_MAX_SKIP frames in a row, and the first frame after
 * the menu is always drawn (the audio DMA starts again with it).
 ****************************************************************************/
#define FS_MAX_SKIP  4      /* frames skipped in a row at most (AUTO) */
#define FS_MARGIN    1000   /* microseconds: the sound and the VSYNC are not exactly in step */

static retro_audio_buffer_status_callback_t audio_status_cb = NULL;
static struct
{
  int skipped;   /* frames skipped in a row */
  int draw_us;   /* time a frame drawn takes (emulation, sound and picture) */
  int drawn;     /* the frame running was drawn (core_video got a picture) */
  int draw_host; /* emulation time of the last frame drawn */
  int resized;   /* the picture changed size: its frame waits for the VSYNC */
  u64 start;     /* when the frame started */
} fs;

static int fs_skip(void)
{
  int queued;

  if (!config.p32x_frameskip || !audio_status_cb)
  {
    fs.skipped = 0;
    return 0;
  }

  queued = gx_audio_Queued();
  if (queued >= 0)
  {
    if (config.p32x_frameskip == PICO32X_FRAMESKIP_AUTO)
    {
      if ((fs.skipped < FS_MAX_SKIP) && (fs.draw_us + FS_MARGIN > queued))
      {
        fs.skipped++;
        return 1;
      }
    }
    else if (fs.skipped < config.p32x_frameskip - 1)
    {
      fs.skipped++;
      return 1;
    }
  }

  fs.skipped = 0;
  return 0;
}

void pico32x_frame_end(void)
{
  if (fs.drawn && !fs.resized)
  {
    /* up at once, down slowly: one slow frame is enough to skip the next */
    int us = diff_usec(fs.start, gettime());
    fs.draw_us = (us >= fs.draw_us) ? us : fs.draw_us - (fs.draw_us - us) / 4;
  }
}

static void core_log(enum retro_log_level level, const char *fmt, ...)
{
  if (level == RETRO_LOG_ERROR)
  {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, ap);
    va_end(ap);
    char *nl = strchr(last_error, '\n');
    if (nl) *nl = 0;
  }
}

static bool core_environment(unsigned cmd, void *data)
{
  switch (cmd)
  {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      ((struct retro_log_callback *) data)->log = core_log;
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      return *(const enum retro_pixel_format *) data == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      *(const char **) data = system_dir;
      return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char **) data = save_dir;
      return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
      struct retro_variable *v = (struct retro_variable *) data;
      v->value = option_value(v->key);
      return v->value != NULL;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
      *(bool *) data = vars_changed;
      vars_changed = 0;
      return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool *) data = true;
      return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
      return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
      *(unsigned *) data = 0;
      return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      av = *(const struct retro_system_av_info *) data;
      return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
      av.geometry = *(const struct retro_game_geometry *) data;
      return true;
    case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
      /* the frame skip (picodrive_frameskip "auto"): told before each frame */
      audio_status_cb = data ? ((const struct retro_audio_buffer_status_callback *) data)->callback : NULL;
      return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_MESSAGE:
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      return true;
    default:
      return false;
  }
}

/* The picture goes into the texture of gx_video.c, in 4x4 tiles of RGB565
   pixels like the line renderer writes them (gx_video.h), without borders */
static void core_video(const void *data, unsigned w, unsigned h, size_t pitch)
{
  unsigned line, x, tiles;

  if (!data)
    return; /* the same picture again (a skipped frame) */
  fs.drawn = 1;

  if (w > 320) w = 320;
  if (h > 240) h = 240;
  w &= ~3u;

  if ((bitmap.viewport.w != (int)w) || (bitmap.viewport.h != (int)h) || bitmap.viewport.x || bitmap.viewport.y)
  {
    bitmap.viewport.w = w;
    bitmap.viewport.h = h;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.changed |= 1;
  }

  tiles = w >> 2;
  for (line = 0; line < h; line++)
  {
    u16 *out = (u16 *)(bitmap.data + ((tiles << 5) * (line >> 2)) + ((line & 3) << 3));
    const u16 *in = (const u16 *)((const u8 *)data + line * pitch);
    for (x = 0; x < tiles; x++)
    {
      out[0] = in[0];
      out[1] = in[1];
      out[2] = in[2];
      out[3] = in[3];
      in += 4;
      out += 16;
    }
  }
}

/* Samples into the FIFO; the Wii audio interface takes them right channel
   first (as BLIP_INVERT does for Genesis Plus GX) */
static size_t core_audio_batch(const int16_t *data, size_t frames)
{
  size_t i, space = FIFO_FRAMES - fifo_len;
  if (frames > space)
    frames = space;
  int16 *out = fifo + fifo_len * 2;
  for (i = 0; i < frames; i++)
  {
    out[i * 2] = data[i * 2 + 1];
    out[i * 2 + 1] = data[i * 2];
  }
  fifo_len += frames;
  return frames;
}

static void core_audio_sample(int16_t left, int16_t right)
{
  int16_t s[2] = { left, right };
  core_audio_batch(s, 1);
}

static void core_input_poll(void)
{
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
  (void) index;
  if (port > 1 || (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD)
    return 0;
  if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
    return (int16_t) pad_bits[port];
  return (pad_bits[port] >> id) & 1;
}

/* Mega Drive buttons of Genesis Plus GX (input.pad) to PicoDrive's libretro
   buttons: retro B = B, A = C, Y = A, X = Y, L = X, R = Z, Select = Mode */
#define RB(id) (1u << RETRO_DEVICE_ID_JOYPAD_##id)
static u32 md_to_retro(u16 p)
{
  u32 b = 0;
  if (p & INPUT_UP)    b |= RB(UP);
  if (p & INPUT_DOWN)  b |= RB(DOWN);
  if (p & INPUT_LEFT)  b |= RB(LEFT);
  if (p & INPUT_RIGHT) b |= RB(RIGHT);
  if (p & INPUT_A)     b |= RB(Y);
  if (p & INPUT_B)     b |= RB(B);
  if (p & INPUT_C)     b |= RB(A);
  if (p & INPUT_X)     b |= RB(L);
  if (p & INPUT_Y)     b |= RB(X);
  if (p & INPUT_Z)     b |= RB(R);
  if (p & INPUT_MODE)  b |= RB(SELECT);
  if (p & INPUT_START) b |= RB(START);
  return b;
}

static void core_init(void)
{
  if (core_ready)
    return;
  snprintf(system_dir, sizeof(system_dir), "%s/bios", DEFAULT_PATH);
  snprintf(save_dir, sizeof(save_dir), "%s/saves/32x", DEFAULT_PATH);
  retro_set_environment(core_environment);
  retro_set_video_refresh(core_video);
  retro_set_audio_sample(core_audio_sample);
  retro_set_audio_sample_batch(core_audio_batch);
  retro_set_input_poll(core_input_poll);
  retro_set_input_state(core_input_state);
  retro_init();
  core_ready = 1;
}

static int read_file(const char *path, u8 *buf, int size)
{
  FILE *f = fopen(path, "rb");
  int n;
  if (!f)
    return 0;
  n = fread(buf, 1, size, f);
  fclose(f);
  return n == size;
}

int pico32x_is_32x_file(const char *filename)
{
  const char *dot = strrchr(filename, '.');
  return dot && !strcasecmp(dot, ".32x");
}

int pico32x_load(const char *filename)
{
  struct retro_game_info info;
  char header[0x200];
  FILE *f;

  int was_active = pico32x_active;

  core_init();
  pico32x_unload();

  /* a Paprium game left: its music file and tile cache (load_rom does it
     for the other games) */
  punchium_shutdown();

  /* the 32X BIOS files, if there are any (PicoDrive has its own otherwise) */
  p32x_bios_g = read_file(config.sys_rom_32x[PICO32X_BIOS_68K], bios_g, sizeof(bios_g)) ? bios_g : NULL;
  p32x_bios_m = read_file(config.sys_rom_32x[PICO32X_BIOS_MASTER], bios_m, sizeof(bios_m)) ? bios_m : NULL;
  p32x_bios_s = read_file(config.sys_rom_32x[PICO32X_BIOS_SLAVE], bios_s, sizeof(bios_s)) ? bios_s : NULL;

  snprintf(game_path, sizeof(game_path), "%s", filename);
  memset(&info, 0, sizeof(info));
  info.path = game_path;
  vars_changed = 1;
  last_error[0] = 0;
  if (!retro_load_game(&info))
  {
    /* the 32X game that was loaded is gone too */
    if (was_active)
      system_hw = 0;
    return 0;
  }
  retro_get_system_av_info(&av);
  pico32x_active = 1;

  /* for the menus, a Mega Drive game: no cartridge patches, the header of
     the ROM for the game information, no hot swap into a 32X game */
  ggenie_shutdown();
  areplay_shutdown();
  system_hw = SYSTEM_MD;
  romtype = SYSTEM_MD;
  vdp_pal = (av.timing.fps < 55.0) ? 1 : 0;
  interlaced = 0;
  odd_frame = 0;
  config.hot_swap &= 1;
  memset(header, 0xff, sizeof(header));
  f = fopen(filename, "rb");
  if (f)
  {
    fread(header, 1, sizeof(header), f);
    fclose(f);
  }
  cart.special = 0;
  cart.romsize = sizeof(header);
  memcpy(cart.rom, header, sizeof(header));
  getrominfo(header);
  cart.romsize = 0;

  /* backup RAM (its size is known right after loading) */
  sram_data = (u8 *) retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
  sram_size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
  if (!sram_data || sram_size > 0x10000)
    sram_size = 0;
  memset(sram.sram, 0xff, sizeof(sram.sram));
  if (sram_size)
    memcpy(sram.sram, sram_data, sram_size);
  sram.on = sram_size ? 1 : 0;
  sram.crc = crc32(0, sram.sram, 0x10000);
  return 1;
}

void pico32x_unload(void)
{
  if (!pico32x_active)
    return;
  retro_unload_game();
  pico32x_active = 0;
  sram_data = NULL;
  sram_size = 0;
  /* the next Genesis Plus GX game starts from a full system init */
  config.hot_swap &= 1;
}

void pico32x_start(void)
{
  double fps = get_framerate();
  double rate = vdp_pal ? 50.0 : 60.0;
  int i;

  /* as many samples per frame as the video needs: PicoDrive makes
     rate / (50 or 60) samples per frame, gx_audio plays 48 kHz */
  if (fps > 0.0)
    snprintf(sound_rate_text, sizeof(sound_rate_text), "%d", (int)(48000.0 * rate / fps + 0.5));
  else
    snprintf(sound_rate_text, sizeof(sound_rate_text), "48000");
  vars_changed = 1;
  fifo_len = 0;
  out_frac = 0.0;
  read_pos = 0.0;

  /* the sound engine of Genesis Plus GX is not used, but gx_video.c sets its
     output rate at every VSYNC (snd.sample_rate, read by pico32x_audio) */
  audio_init(48000, fps);

  /* controllers of the settings (3 or 6 buttons) */
  input_init();
  for (i = 0; i < 2; i++)
  {
    int dev = input.dev[i * 4];
    retro_set_controller_port_device(i, (dev == DEVICE_PAD6B) ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
                                        : (dev == NO_DEVICE) ? RETRO_DEVICE_NONE : RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0));
  }
  /* the automatic SH-2 overclock starts again from 100%, the frame skip
     measures the frames again */
  pico32x_autooc_reset();
  memset(&fs, 0, sizeof(fs));
}

int pico32x_frame(void)
{
  int skip, host;

  osd_input_update();
  pad_bits[0] = md_to_retro(input.pad[0]);
  pad_bits[1] = md_to_retro(input.pad[4]);

  /* skip the picture of this frame? (PicoDrive asks it in retro_run) */
  skip = fs_skip();
  if (audio_status_cb)
    audio_status_cb(true, skip ? 0 : 100, skip);

  fs.drawn = 0;
  fs.start = gettime();
  retro_run();
  host = diff_usec(fs.start, gettime());
  fs.resized = bitmap.viewport.changed & 1;

  /* the automatic overclock goes by the frames drawn: it never takes more
     than the Wii can do drawing every frame */
  if (fs.drawn || !fs.draw_host)
    fs.draw_host = host;
  aoc_end_frame((host > fs.draw_host) ? host : fs.draw_host);
  return fs.drawn;
}

/* The samples of one frame for the audio DMA. Like audio_update(), it
   follows the output rate gx_video.c asks for in snd.sample_rate (a little
   above or below 48 kHz, to stay in sync with the video): PicoDrive makes
   48000 samples per second of video, read here a little faster or slower.
   A multiple of 8 samples (32 bytes), at most 1024. */
int pico32x_audio(int16 *out)
{
  double fps = (snd.frame_rate > 0.0) ? snd.frame_rate : (vdp_pal ? 50.0 : 60.0);
  double rate = snd.sample_rate ? snd.sample_rate : 48000;
  double step = 48000.0 / rate;
  double want = out_frac + rate / fps;
  int n = ((int) want) & ~7, i, used;

  if (n < 8) n = 8;
  if (n > 1024) n = 1024;
  out_frac = want - n;
  if (out_frac < -64.0 || out_frac > 64.0)
    out_frac = 0.0;

  for (i = 0; i < n; i++)
  {
    int k = (int) read_pos;
    if (k >= fifo_len)
      k = fifo_len - 1; /* running short: the last sample again */
    if (k < 0)
    {
      out[i * 2] = out[i * 2 + 1] = 0;
    }
    else
    {
      out[i * 2] = fifo[k * 2];
      out[i * 2 + 1] = fifo[k * 2 + 1];
    }
    read_pos += step;
  }

  /* drop what was read; keep at most about two frames waiting */
  used = (int) read_pos;
  if (used > fifo_len)
    used = fifo_len;
  read_pos -= used;
  if (read_pos > 1.0)
    read_pos = 0.0;
  if (fifo_len - used > 2048)
    used = fifo_len - 1024;
  fifo_len -= used;
  if (fifo_len > 0)
    memmove(fifo, fifo + used * 2, fifo_len * 4);
  return n;
}

void pico32x_reset(void)
{
  retro_reset();
  pico32x_autooc_reset();
}

int pico32x_state_size(void)
{
  return (int) retro_serialize_size();
}

int pico32x_state_save(uint8 *buffer, int size)
{
  return retro_serialize(buffer, size) ? size : 0;
}

int pico32x_state_load(const uint8 *buffer, int size)
{
  int ok = retro_unserialize(buffer, size) ? 1 : 0;
  pico32x_autooc_reset();
  return ok;
}

void pico32x_sram_to_gpgx(void)
{
  if (sram_size && sram_data)
    memcpy(sram.sram, sram_data, sram_size);
}

void pico32x_sram_from_gpgx(void)
{
  if (sram_size && sram_data)
    memcpy(sram_data, sram.sram, sram_size);
}

const char *pico32x_last_error(void)
{
  return last_error;
}

/* Cheats of the cheat menu (gx/gui/cheats.c) on the 32X game, done by
   PicoDrive (gx_cheat_* in picodrive/platform/libretro/libretro.c) */
int pico32x_cheat_rom(uint32 address, uint16 data)
{
  return pico32x_active ? gx_cheat_rom(address, data) : -1;
}

void pico32x_cheat_rom_done(void)
{
  if (pico32x_active)
    gx_cheat_code_changed();
}

void pico32x_cheat_ram(uint32 address, uint16 data)
{
  /* a word when the high byte is set, as the cheats of Genesis Plus GX */
  if (pico32x_active)
    gx_cheat_ram(address, data, (data & 0xff00) != 0);
}
