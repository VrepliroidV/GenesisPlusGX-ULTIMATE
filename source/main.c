/*
 * 32X Test - a small Wii front end for the PicoDrive libretro core, made to
 * see how fast the Sega 32X runs on the Wii.
 *
 * Copyright (c) 2026 VrepliroidV
 * PicoDrive: notaz, irixxxx, fDave and others (see COPYING and AUTHORS in
 * the PicoDrive sources). Same license: non-commercial use, source included.
 *
 * Menu on the libogc text console, game on GX, sound through the audio
 * interface at 32 kHz, Wii remote / Classic / GameCube / XInput USB pads.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <dirent.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/stat.h>

#include "libretro.h"
#include "xinput_usb.h"

#define APP_NAME    "32X Test"
#define APP_VERSION "0.1"
#define APP_FOLDER  "32xtest"

/* PicoDrive: the 32X BIOS images, NULL for its built-in replacement */
extern void *p32x_bios_g, *p32x_bios_m, *p32x_bios_s;

/* ------------------------------------------------------------------------ */
/* Settings                                                                 */
/* ------------------------------------------------------------------------ */

enum { SH2_DRC, SH2_INTERPRETER };
enum { BIOS_BUILTIN, BIOS_FILES };
enum { REND_ACCURATE, REND_GOOD, REND_FAST };

static struct
{
	int sh2;
	int bios;
	int renderer;
	int frameskip;   /* 0 off, 1 auto */
	int sound;
	int limit;       /* speed limit */
	int fps;         /* show the speed */
	int pad6;        /* 6-button pad */
	int widescreen;  /* the TV is 16:9: keep 4:3 */
} cfg = { SH2_DRC, BIOS_BUILTIN, REND_GOOD, 0, 1, 1, 1, 1, 0 };

static char base_dir[64] = "sd:/" APP_FOLDER;
static char bios_dir[80], save_dir[80];

static void cfg_path(char *out, int len)
{
	snprintf(out, len, "%s/settings.cfg", base_dir);
}

static void cfg_save(void)
{
	char path[96];
	cfg_path(path, sizeof(path));
	FILE *f = fopen(path, "w");
	if (!f)
		return;
	fprintf(f, "sh2=%d\nbios=%d\nrenderer=%d\nframeskip=%d\nsound=%d\nlimit=%d\nfps=%d\npad6=%d\nwidescreen=%d\n",
		cfg.sh2, cfg.bios, cfg.renderer, cfg.frameskip, cfg.sound, cfg.limit, cfg.fps, cfg.pad6, cfg.widescreen);
	fclose(f);
}

static void cfg_load(void)
{
	char path[96], line[64];
	cfg_path(path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
	{
		char *eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = 0;
		int v = atoi(eq + 1);
		if (!strcmp(line, "sh2")) cfg.sh2 = v & 1;
		else if (!strcmp(line, "bios")) cfg.bios = v & 1;
		else if (!strcmp(line, "renderer")) cfg.renderer = (v >= 0 && v <= 2) ? v : REND_GOOD;
		else if (!strcmp(line, "frameskip")) cfg.frameskip = v & 1;
		else if (!strcmp(line, "sound")) cfg.sound = v & 1;
		else if (!strcmp(line, "limit")) cfg.limit = v & 1;
		else if (!strcmp(line, "fps")) cfg.fps = v & 1;
		else if (!strcmp(line, "pad6")) cfg.pad6 = v & 1;
		else if (!strcmp(line, "widescreen")) cfg.widescreen = v & 1;
	}
	fclose(f);
}

/* ------------------------------------------------------------------------ */
/* Log: the core's messages, kept in memory and written to log.txt          */
/* ------------------------------------------------------------------------ */

#define LOG_SIZE 16384
static char log_text[LOG_SIZE];
static int log_len = 0;
static char last_error[160];

static void log_add(const char *text)
{
	int n = strlen(text);
	if (n >= LOG_SIZE / 2)
		n = LOG_SIZE / 2 - 1;
	if (log_len + n >= LOG_SIZE)
	{
		/* keep the second half */
		memmove(log_text, log_text + LOG_SIZE / 2, log_len - LOG_SIZE / 2);
		log_len -= LOG_SIZE / 2;
	}
	memcpy(log_text + log_len, text, n);
	log_len += n;
	log_text[log_len] = 0;
}

static void log_printf(const char *fmt, ...)
{
	char line[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	log_add(line);
}

static void log_write(void)
{
	char path[96];
	snprintf(path, sizeof(path), "%s/log.txt", base_dir);
	FILE *f = fopen(path, "w");
	if (!f)
		return;
	fwrite(log_text, 1, log_len, f);
	fclose(f);
}

static void core_log(enum retro_log_level level, const char *fmt, ...)
{
	static const char *names[] = { "debug", "info", "warn", "error" };
	char line[256];
	int n = snprintf(line, sizeof(line), "[%s] ", names[level & 3]);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line + n, sizeof(line) - n, fmt, ap);
	va_end(ap);
	log_add(line);
	if (level == RETRO_LOG_ERROR)
	{
		snprintf(last_error, sizeof(last_error), "%s", line + n);
		char *nl = strchr(last_error, '\n');
		if (nl)
			*nl = 0;
	}
}

/* ------------------------------------------------------------------------ */
/* Video                                                                    */
/* ------------------------------------------------------------------------ */

#define FIFO_SIZE (256 * 1024)
#define TEX_W 320
#define TEX_H 240

static GXRModeObj *rmode;
static u32 *game_xfb[2];
static u32 *menu_xfb;
static int game_fb = 0;
static void *gp_fifo;
static u16 texbuf[TEX_W * TEX_H] ATTRIBUTE_ALIGN(32);
static GXTexObj texobj;
static volatile u32 retraces = 0;

static void retrace_cb(u32 count)
{
	(void) count;
	retraces++;
}

static void gx_setup(void)
{
	GXColor black = { 0, 0, 0, 0xff };
	Mtx44 proj;
	Mtx view;

	gp_fifo = memalign(32, FIFO_SIZE);
	memset(gp_fifo, 0, FIFO_SIZE);
	GX_Init(gp_fifo, FIFO_SIZE);
	GX_SetCopyClear(black, GX_MAX_Z24);

	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
	u32 xfb_height = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfb_height);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumChans(0);
	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);

	GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_FALSE);
	GX_SetColorUpdate(GX_TRUE);

	guOrtho(proj, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
	guMtxIdentity(view);
	GX_LoadPosMtxImm(view, GX_PNMTX0);
	GX_Flush();
}

static void video_init(void)
{
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	VIDEO_Configure(rmode);
	game_xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	game_xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	menu_xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_ClearFrameBuffer(rmode, game_xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, game_xfb[1], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, menu_xfb, COLOR_BLACK);
	CON_Init(menu_xfb, 24, 24, rmode->fbWidth - 48, rmode->xfbHeight - 48, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_SetNextFramebuffer(menu_xfb);
	VIDEO_SetPostRetraceCallback(retrace_cb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	gx_setup();
}

/* 3x5 font for the speed line */
static const struct { char c; u8 rows[5]; } glyphs[] =
{
	{ '0', { 7, 5, 5, 5, 7 } }, { '1', { 2, 6, 2, 2, 7 } }, { '2', { 7, 1, 7, 4, 7 } },
	{ '3', { 7, 1, 7, 1, 7 } }, { '4', { 5, 5, 7, 1, 1 } }, { '5', { 7, 4, 7, 1, 7 } },
	{ '6', { 7, 4, 7, 5, 7 } }, { '7', { 7, 1, 1, 1, 1 } }, { '8', { 7, 5, 7, 5, 7 } },
	{ '9', { 7, 5, 7, 1, 7 } }, { '%', { 5, 1, 2, 4, 5 } }, { 'F', { 7, 4, 6, 4, 4 } },
	{ 'P', { 6, 5, 6, 4, 4 } }, { 'S', { 7, 4, 7, 1, 7 } }, { 'D', { 6, 5, 5, 5, 6 } },
	{ 'R', { 6, 5, 6, 5, 5 } }, { 'C', { 7, 4, 4, 4, 7 } }, { 'I', { 7, 2, 2, 2, 7 } },
	{ 'N', { 6, 5, 5, 5, 5 } }, { 'T', { 7, 2, 2, 2, 2 } }, { ' ', { 0, 0, 0, 0, 0 } },
};

static inline void tex_put(int tw, int x, int y, u16 c)
{
	texbuf[(((y >> 2) * (tw >> 2) + (x >> 2)) << 4) + ((y & 3) << 2) + (x & 3)] = c;
}

static void tex_text(int tw, int th, int x, int y, const char *s)
{
	int len = strlen(s), i, r, b;
	/* black box behind the text */
	for (r = y - 1; r < y + 6 && r < th; r++)
		for (i = x - 1; i < x + len * 4 && i < tw; i++)
			if (r >= 0 && i >= 0)
				tex_put(tw, i, r, 0x0000);
	for (; *s; s++, x += 4)
	{
		const u8 *rows = NULL;
		for (i = 0; i < (int) (sizeof(glyphs) / sizeof(glyphs[0])); i++)
			if (glyphs[i].c == *s)
				rows = glyphs[i].rows;
		if (!rows)
			continue;
		for (r = 0; r < 5; r++)
			for (b = 0; b < 3; b++)
				if ((rows[r] >> (2 - b)) & 1 && x + b < tw && y + r < th)
					tex_put(tw, x + b, y + r, 0xffff);
	}
}

static char speed_text[32] = "";
static u32 presented_at = 0;   /* retrace count of the last frame shown */
static unsigned last_w = 320, last_h = 224;

static void present(const u16 *src, unsigned w, unsigned h, size_t pitch)
{
	unsigned tw = (w + 3) & ~3u, th = (h + 3) & ~3u;
	unsigned x, y, r;
	size_t pp = pitch / 2;

	if (tw > TEX_W) tw = TEX_W;
	if (th > TEX_H) th = TEX_H;

	if (src)
	{
		/* RGB565 into 4x4 tiles */
		u16 *d = texbuf;
		for (y = 0; y < th; y += 4)
			for (x = 0; x < tw; x += 4)
				for (r = 0; r < 4; r++)
				{
					unsigned sy = y + r;
					if (sy < h && x + 3 < w)
					{
						const u16 *s = src + sy * pp + x;
						d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
					}
					else
					{
						unsigned k;
						for (k = 0; k < 4; k++)
							d[k] = (sy < h && x + k < w) ? src[sy * pp + x + k] : 0;
					}
					d += 4;
				}
		last_w = tw;
		last_h = th;
	}
	else
	{
		/* frame skipped: show the last one again */
		tw = last_w;
		th = last_h;
	}

	if (cfg.fps && speed_text[0])
		tex_text(tw, th, 3, 3, speed_text);

	DCFlushRange(texbuf, tw * th * 2);
	GX_InvalidateTexAll();
	GX_InitTexObj(&texobj, texbuf, tw, th, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&texobj, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	f32 x0 = 0, x1 = rmode->fbWidth, y0 = 0, y1 = rmode->efbHeight;
	if (cfg.widescreen)
	{
		f32 width = rmode->fbWidth * 3.0f / 4.0f;
		x0 = (rmode->fbWidth - width) / 2;
		x1 = x0 + width;
	}
	f32 s1 = (f32) w / tw, t1 = (f32) h / th;
	if (!src)
		s1 = t1 = 1.0f;

	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2f32(x0, y0); GX_TexCoord2f32(0, 0);
	GX_Position2f32(x1, y0); GX_TexCoord2f32(s1, 0);
	GX_Position2f32(x1, y1); GX_TexCoord2f32(s1, t1);
	GX_Position2f32(x0, y1); GX_TexCoord2f32(0, t1);
	GX_End();
	GX_DrawDone();

	game_fb ^= 1;
	GX_CopyDisp(game_xfb[game_fb], GX_TRUE);
	GX_Flush();
	VIDEO_SetNextFramebuffer(game_xfb[game_fb]);
	VIDEO_Flush();
	presented_at = retraces;
}

/* ------------------------------------------------------------------------ */
/* Audio: 32 kHz stereo through the audio interface                         */
/* ------------------------------------------------------------------------ */

#define DMA_FRAMES  512
#define RING_FRAMES 8192

static s16 dma_buf[2][DMA_FRAMES * 2] ATTRIBUTE_ALIGN(32);
static int dma_cur = 0;
static s16 ring[RING_FRAMES * 2];
static volatile u32 ring_r = 0, ring_w = 0;
static int audio_running = 0;

static void dma_cb(void)
{
	s16 *b;
	u32 avail, i, n;

	dma_cur ^= 1;
	b = dma_buf[dma_cur];
	avail = ring_w - ring_r;
	n = avail < DMA_FRAMES ? avail : DMA_FRAMES;
	for (i = 0; i < n; i++)
	{
		u32 k = (ring_r + i) % RING_FRAMES;
		b[i * 2] = ring[k * 2];
		b[i * 2 + 1] = ring[k * 2 + 1];
	}
	ring_r += n;
	if (n < DMA_FRAMES)
		memset(b + n * 2, 0, (DMA_FRAMES - n) * 4);
	DCFlushRange(b, sizeof(dma_buf[0]));
	AUDIO_InitDMA((u32) b, sizeof(dma_buf[0]));
}

static void audio_start(void)
{
	if (audio_running)
		return;
	ring_r = ring_w = 0;
	memset(dma_buf, 0, sizeof(dma_buf));
	DCFlushRange(dma_buf, sizeof(dma_buf));
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
	AUDIO_RegisterDMACallback(dma_cb);
	AUDIO_InitDMA((u32) dma_buf[0], sizeof(dma_buf[0]));
	AUDIO_StartDMA();
	audio_running = 1;
}

static void audio_stop(void)
{
	if (!audio_running)
		return;
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	audio_running = 0;
}

static size_t core_audio_batch(const int16_t *data, size_t frames)
{
	u32 space, i;
	if (!cfg.sound || !audio_running)
		return frames;
	space = RING_FRAMES - (ring_w - ring_r);
	if (frames > space)
		frames = space; /* running faster than real time: drop the rest */
	for (i = 0; i < frames; i++)
	{
		u32 k = (ring_w + i) % RING_FRAMES;
		ring[k * 2] = data[i * 2];
		ring[k * 2 + 1] = data[i * 2 + 1];
	}
	ring_w += frames;
	return frames;
}

static void core_audio_sample(int16_t left, int16_t right)
{
	int16_t s[2] = { left, right };
	core_audio_batch(s, 1);
}

static retro_audio_buffer_status_callback_t buffer_status_cb = NULL;

/* ------------------------------------------------------------------------ */
/* Input                                                                    */
/* ------------------------------------------------------------------------ */

#define RB(id) (1u << RETRO_DEVICE_ID_JOYPAD_##id)

static u32 pad_bits[2];
static int menu_wanted = 0;

/* Mega Drive buttons through the core's mapping: retro B = B, A = C, Y = A,
   X = Y, L = X, R = Z, Select = Mode */
#define MD_A    RB(Y)
#define MD_B    RB(B)
#define MD_C    RB(A)
#define MD_X    RB(L)
#define MD_Y    RB(X)
#define MD_Z    RB(R)
#define MD_MODE RB(SELECT)
#define MD_START RB(START)

static u32 gc_to_md(u16 gc, s8 sx, s8 sy)
{
	u32 b = 0;
	if ((gc & PAD_BUTTON_UP) || sy > 48) b |= RB(UP);
	if ((gc & PAD_BUTTON_DOWN) || sy < -48) b |= RB(DOWN);
	if ((gc & PAD_BUTTON_LEFT) || sx < -48) b |= RB(LEFT);
	if ((gc & PAD_BUTTON_RIGHT) || sx > 48) b |= RB(RIGHT);
	if (gc & PAD_BUTTON_B) b |= MD_A;
	if (gc & PAD_BUTTON_A) b |= MD_B;
	if (gc & PAD_BUTTON_X) b |= MD_C;
	if (gc & PAD_TRIGGER_L) b |= MD_X;
	if (gc & PAD_BUTTON_Y) b |= MD_Y;
	if (gc & PAD_TRIGGER_R) b |= MD_Z;
	if (gc & PAD_TRIGGER_Z) b |= MD_MODE;
	if (gc & PAD_BUTTON_START) b |= MD_START;
	return b;
}

/* XInput pads as in Genesis Plus GX ULTIMATE: A, B, C, X, Y, Z as labelled on
   a 6-button pad (the triggers and LB give C, RB gives Z), Back is Mode */
static u32 xi_to_md(u32 x)
{
	u32 b = 0;
	if (x & XI_ANY_UP) b |= RB(UP);
	if (x & XI_ANY_DOWN) b |= RB(DOWN);
	if (x & XI_ANY_LEFT) b |= RB(LEFT);
	if (x & XI_ANY_RIGHT) b |= RB(RIGHT);
	if (x & XI_A) b |= MD_A;
	if (x & XI_B) b |= MD_B;
	if (x & (XI_LT | XI_RT | XI_LB)) b |= MD_C;
	if (x & XI_X) b |= MD_X;
	if (x & XI_Y) b |= MD_Y;
	if (x & XI_RB) b |= MD_Z;
	if (x & XI_BACK) b |= MD_MODE;
	if (x & XI_START) b |= MD_START;
	return b;
}

static u32 wpad_to_md(int chan, int *home)
{
	WPADData *wd = WPAD_Data(chan);
	u32 w, b = 0;
	if (!wd || wd->err != WPAD_ERR_NONE)
		return 0;
	w = wd->btns_h;
	if (wd->exp.type == WPAD_EXP_CLASSIC)
	{
		if (w & WPAD_CLASSIC_BUTTON_UP) b |= RB(UP);
		if (w & WPAD_CLASSIC_BUTTON_DOWN) b |= RB(DOWN);
		if (w & WPAD_CLASSIC_BUTTON_LEFT) b |= RB(LEFT);
		if (w & WPAD_CLASSIC_BUTTON_RIGHT) b |= RB(RIGHT);
		if (w & WPAD_CLASSIC_BUTTON_Y) b |= MD_A;
		if (w & WPAD_CLASSIC_BUTTON_B) b |= MD_B;
		if (w & WPAD_CLASSIC_BUTTON_A) b |= MD_C;
		if (w & (WPAD_CLASSIC_BUTTON_ZL | WPAD_CLASSIC_BUTTON_FULL_L)) b |= MD_X;
		if (w & WPAD_CLASSIC_BUTTON_X) b |= MD_Y;
		if (w & (WPAD_CLASSIC_BUTTON_ZR | WPAD_CLASSIC_BUTTON_FULL_R)) b |= MD_Z;
		if (w & WPAD_CLASSIC_BUTTON_MINUS) b |= MD_MODE;
		if (w & WPAD_CLASSIC_BUTTON_PLUS) b |= MD_START;
		if (w & WPAD_CLASSIC_BUTTON_HOME) *home = 1;
	}
	/* the Wii remote held sideways (also with a Classic plugged in) */
	if (w & WPAD_BUTTON_RIGHT) b |= RB(UP);
	if (w & WPAD_BUTTON_LEFT) b |= RB(DOWN);
	if (w & WPAD_BUTTON_UP) b |= RB(LEFT);
	if (w & WPAD_BUTTON_DOWN) b |= RB(RIGHT);
	if (w & WPAD_BUTTON_1) b |= MD_A;
	if (w & WPAD_BUTTON_2) b |= MD_B;
	if (w & WPAD_BUTTON_A) b |= MD_C;
	if (w & WPAD_BUTTON_B) b |= MD_Y;
	if (w & WPAD_BUTTON_MINUS) b |= MD_MODE;
	if (w & WPAD_BUTTON_PLUS) b |= MD_START;
	if (w & WPAD_BUTTON_HOME) *home = 1;
	return b;
}

static void input_read(void)
{
	int port, home = 0;
	WPAD_ScanPads();
	PAD_ScanPads();
	XInput_Update();
	for (port = 0; port < 2; port++)
	{
		u16 gc = PAD_ButtonsHeld(port);
		u32 xi = XInput_ButtonsHeld(port);
		u32 b = gc_to_md(gc, PAD_StickX(port), PAD_StickY(port)) | xi_to_md(xi) | wpad_to_md(port, &home);
		/* Mode + Right (GameCube Z + Right, USB Select + Right): the menu */
		if ((b & MD_MODE) && (b & RB(RIGHT)))
			home = 1;
		pad_bits[port] = b;
	}
	if (home)
		menu_wanted = 1;
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

/* Menu keys from every controller */
enum { K_UP = 1, K_DOWN = 2, K_LEFT = 4, K_RIGHT = 8, K_OK = 16, K_BACK = 32, K_HOME = 64 };

static u32 menu_keys_held(void)
{
	u32 k = 0;
	int c;
	WPAD_ScanPads();
	PAD_ScanPads();
	XInput_Update();
	for (c = 0; c < 4; c++)
	{
		u16 gc = PAD_ButtonsHeld(c);
		s8 sy = PAD_StickY(c), sx = PAD_StickX(c);
		u32 x = XInput_ButtonsHeld(c);
		WPADData *wd = WPAD_Data(c);
		u32 w = (wd && wd->err == WPAD_ERR_NONE) ? wd->btns_h : 0;

		if ((gc & PAD_BUTTON_UP) || sy > 48 || (x & XI_ANY_UP) || (w & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP))) k |= K_UP;
		if ((gc & PAD_BUTTON_DOWN) || sy < -48 || (x & XI_ANY_DOWN) || (w & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN))) k |= K_DOWN;
		if ((gc & PAD_BUTTON_LEFT) || sx < -48 || (x & XI_ANY_LEFT) || (w & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT))) k |= K_LEFT;
		if ((gc & PAD_BUTTON_RIGHT) || sx > 48 || (x & XI_ANY_RIGHT) || (w & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT))) k |= K_RIGHT;
		if ((gc & PAD_BUTTON_A) || (x & (XI_A | XI_START)) || (w & (WPAD_BUTTON_A | WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_A))) k |= K_OK;
		if ((gc & PAD_BUTTON_B) || (x & XI_B) || (w & (WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_CLASSIC_BUTTON_B))) k |= K_BACK;
		if (w & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)) k |= K_HOME;
	}
	return k;
}

/* Keys pressed now, with repeat for the directions when held */
static u32 menu_keys(void)
{
	static u32 prev = 0;
	static int hold = 0;
	u32 now = menu_keys_held(), down = now & ~prev;
	u32 dirs = now & (K_UP | K_DOWN | K_LEFT | K_RIGHT);
	if (dirs && dirs == (prev & (K_UP | K_DOWN | K_LEFT | K_RIGHT)))
	{
		if (++hold > 20 && (hold % 4) == 0)
			down |= dirs;
	}
	else
		hold = 0;
	prev = now;
	return down;
}

/* ------------------------------------------------------------------------ */
/* Core interface                                                           */
/* ------------------------------------------------------------------------ */

static int vars_changed = 1;
static struct retro_system_av_info av;

static const char *option_value(const char *key)
{
	if (!strcmp(key, "picodrive_drc"))
		return cfg.sh2 == SH2_DRC ? "enabled" : "disabled";
	if (!strcmp(key, "picodrive_renderer"))
		return cfg.renderer == REND_ACCURATE ? "accurate" : cfg.renderer == REND_FAST ? "fast" : "good";
	if (!strcmp(key, "picodrive_sound_rate"))
		return "32000";
	if (!strcmp(key, "picodrive_frameskip"))
		return cfg.frameskip ? "auto" : "disabled";
	return NULL;
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
		*(const char **) data = bios_dir;
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
		buffer_status_cb = data ? ((const struct retro_audio_buffer_status_callback *) data)->callback : NULL;
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

static void core_video(const void *data, unsigned w, unsigned h, size_t pitch)
{
	/* speed limit: no faster than the game's frame rate */
	static u64 next = 0;
	if (cfg.limit)
	{
		u64 now = gettime();
		u64 step = secs_to_ticks(1) / (av.timing.fps > 1 ? (u64) av.timing.fps : 60);
		if (next == 0 || now > next + secs_to_ticks(1) / 10)
			next = now;
		while (gettime() < next)
			;
		next += step;
	}
	present((const u16 *) data, w, h, pitch);
}

/* ------------------------------------------------------------------------ */
/* Games and BIOS                                                           */
/* ------------------------------------------------------------------------ */

#define MAX_GAMES 1000
static char (*games)[256] = NULL;  /* full paths */
static int num_games = 0;
static int game_sel = 0;
static char game_path[256] = "";
static int game_loaded = 0;
static int needs_reload = 0;

static int has_ext(const char *name)
{
	static const char *exts[] = { ".32x", ".bin", ".md", ".gen", ".smd" };
	const char *dot = strrchr(name, '.');
	unsigned i;
	if (!dot)
		return 0;
	for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
		if (!strcasecmp(dot, exts[i]))
			return 1;
	return 0;
}

static const char *base_name(const char *path)
{
	const char *s = strrchr(path, '/');
	return s ? s + 1 : path;
}

static int game_cmp(const void *a, const void *b)
{
	return strcasecmp(base_name((const char *) a), base_name((const char *) b));
}

static void scan_games(void)
{
	static const char *dirs[] = { "sd:/" APP_FOLDER "/roms", "usb:/" APP_FOLDER "/roms" };
	unsigned d;
	if (!games)
		games = malloc(MAX_GAMES * 256);
	num_games = 0;
	if (!games)
		return;
	for (d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++)
	{
		DIR *dir = opendir(dirs[d]);
		struct dirent *e;
		if (!dir)
			continue;
		while (num_games < MAX_GAMES && (e = readdir(dir)) != NULL)
		{
			if (e->d_name[0] == '.' || !has_ext(e->d_name))
				continue;
			snprintf(games[num_games++], 256, "%s/%s", dirs[d], e->d_name);
		}
		closedir(dir);
	}
	qsort(games, num_games, 256, game_cmp);
	if (game_sel >= num_games)
		game_sel = 0;
}

static u8 bios_g[0x100], bios_m[0x800], bios_s[0x400];
static int bios_found[3];

static int read_bios(const char *name, u8 *buf, int size)
{
	char path[128];
	FILE *f;
	int n;
	snprintf(path, sizeof(path), "%s/%s", bios_dir, name);
	f = fopen(path, "rb");
	if (!f)
		return 0;
	n = fread(buf, 1, size, f);
	fclose(f);
	return n == size;
}

static void check_bios(void)
{
	bios_found[0] = read_bios("32X_G_BIOS.BIN", bios_g, sizeof(bios_g));
	bios_found[1] = read_bios("32X_M_BIOS.BIN", bios_m, sizeof(bios_m));
	bios_found[2] = read_bios("32X_S_BIOS.BIN", bios_s, sizeof(bios_s));
}

static void unload_game(void)
{
	if (!game_loaded)
		return;
	retro_unload_game();
	game_loaded = 0;
}

static int load_game(const char *path)
{
	struct retro_game_info info;
	unload_game();
	check_bios();
	p32x_bios_g = (cfg.bios == BIOS_FILES && bios_found[0]) ? bios_g : NULL;
	p32x_bios_m = (cfg.bios == BIOS_FILES && bios_found[1]) ? bios_m : NULL;
	p32x_bios_s = (cfg.bios == BIOS_FILES && bios_found[2]) ? bios_s : NULL;

	snprintf(game_path, sizeof(game_path), "%s", path);
	memset(&info, 0, sizeof(info));
	info.path = game_path;
	vars_changed = 1;
	last_error[0] = 0;
	log_printf("--- loading %s (SH-2 %s, BIOS %s, renderer %d)\n", path,
		cfg.sh2 == SH2_DRC ? "recompiler" : "interpreter",
		cfg.bios == BIOS_FILES ? "files" : "built-in", cfg.renderer);
	if (!retro_load_game(&info))
	{
		log_printf("--- load failed\n");
		log_write();
		return 0;
	}
	retro_get_system_av_info(&av);
	retro_set_controller_port_device(0, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, cfg.pad6 ? 1 : 0));
	retro_set_controller_port_device(1, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, cfg.pad6 ? 1 : 0));
	game_loaded = 1;
	needs_reload = 0;
	log_printf("--- loaded: %ux%u, %.2f fps, %.0f Hz\n", av.geometry.base_width, av.geometry.base_height,
		av.timing.fps, av.timing.sample_rate);
	log_write();
	return 1;
}

/* ------------------------------------------------------------------------ */
/* Running                                                                  */
/* ------------------------------------------------------------------------ */

static volatile int power_off = 0;
static void power_cb(void) { power_off = 1; }
static void wpad_power_cb(s32 chan) { (void) chan; power_off = 1; }
static volatile int reset_pressed = 0;
static void reset_cb(u32 irq, void *ctx) { (void) irq; (void) ctx; reset_pressed = 1; }

static void run_game(void)
{
	u64 t0;
	u32 frames = 0;
	double target = av.timing.fps > 1 ? av.timing.fps : 60.0;

	menu_wanted = 0;
	speed_text[0] = 0;
	VIDEO_SetNextFramebuffer(game_xfb[game_fb]);
	VIDEO_Flush();
	if (cfg.sound)
		audio_start();
	t0 = gettime();

	while (!menu_wanted && !power_off)
	{
		input_read();
		if (reset_pressed)
		{
			reset_pressed = 0;
			menu_wanted = 1;
		}
		if (buffer_status_cb)
		{
			u32 fill = audio_running ? ring_w - ring_r : RING_FRAMES / 2;
			unsigned occ = fill * 100 / RING_FRAMES;
			buffer_status_cb(audio_running, occ, occ < 6);
		}
		retro_run();
		frames++;

		u64 now = gettime();
		u32 us = ticks_to_microsecs(now - t0);
		if (us >= 1000000)
		{
			double fps = frames * 1000000.0 / us;
			int pct = (int) (fps * 100.0 / target + 0.5);
			snprintf(speed_text, sizeof(speed_text), "%d FPS %d%% %s", (int) (fps + 0.5), pct,
				cfg.sh2 == SH2_DRC ? "DRC" : "INT");
			frames = 0;
			t0 = now;
		}
	}

	audio_stop();
	log_write();
	VIDEO_SetNextFramebuffer(menu_xfb);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

/* ------------------------------------------------------------------------ */
/* Menu                                                                     */
/* ------------------------------------------------------------------------ */

#define C_NORMAL "\x1b[37m"
#define C_SEL    "\x1b[33m"
#define C_TITLE  "\x1b[36m"
#define C_DIM    "\x1b[32m"
#define C_ERR    "\x1b[31m"

static const char *library_version = "";

static void pos(int row, int col) { printf("\x1b[%d;%dH", row, col); }

static void draw_roms(int sel, int top, int rows)
{
	int i;
	printf("\x1b[2J");
	pos(1, 0);
	printf(C_TITLE " Choose a game (%d)  -  A: start   B: back" C_NORMAL "\n\n", num_games);
	if (num_games == 0)
	{
		printf(" No games found. Put .32x (or .bin / .md) files in\n");
		printf(" sd:/" APP_FOLDER "/roms or usb:/" APP_FOLDER "/roms\n");
		return;
	}
	for (i = top; i < top + rows && i < num_games; i++)
	{
		char name[70];
		snprintf(name, sizeof(name), "%s", base_name(games[i]));
		printf("%s %s %s\n", i == sel ? C_SEL : C_NORMAL, i == sel ? ">" : " ", name);
	}
	printf(C_NORMAL);
}

/* Returns 1 when a game was chosen (and loaded) */
static int menu_roms(void)
{
	int sel = game_sel, top = 0, rows = 20, redraw = 1;
	scan_games();
	if (sel >= num_games)
		sel = 0;
	while (!power_off)
	{
		if (sel < top) top = sel;
		if (sel >= top + rows) top = sel - rows + 1;
		if (redraw)
		{
			draw_roms(sel, top, rows);
			redraw = 0;
		}
		VIDEO_WaitVSync();
		u32 k = menu_keys();
		if (k & K_BACK)
			return 0;
		if (num_games == 0)
			continue;
		if (k & K_UP) { sel = (sel + num_games - 1) % num_games; redraw = 1; }
		if (k & K_DOWN) { sel = (sel + 1) % num_games; redraw = 1; }
		if (k & K_LEFT) { sel = sel > rows ? sel - rows : 0; redraw = 1; }
		if (k & K_RIGHT) { sel = sel + rows < num_games ? sel + rows : num_games - 1; redraw = 1; }
		if (k & K_OK)
		{
			game_sel = sel;
			printf("\x1b[2J");
			pos(2, 0);
			printf(" Loading %s ...\n", base_name(games[sel]));
			VIDEO_WaitVSync();
			if (load_game(games[sel]))
				return 1;
			printf(C_ERR "\n Could not load this game.\n %s\n" C_NORMAL, last_error);
			printf("\n Details in %s/log.txt. Press A.\n", base_dir);
			while (!power_off && !(menu_keys() & (K_OK | K_BACK)))
				VIDEO_WaitVSync();
			redraw = 1;
		}
	}
	return 0;
}

enum
{
	M_GAME, M_RESUME, M_RESET, M_SH2, M_BIOS, M_RENDER, M_SKIP, M_SOUND, M_LIMIT, M_FPS, M_PAD, M_WIDE, M_EXIT,
	M_COUNT
};

static void draw_menu(int sel)
{
	static const char *render_names[] = { "Accurate", "Good", "Fast" };
	char line[M_COUNT][80];
	int i;

	snprintf(line[M_GAME], 80, "Game:          %.50s", game_loaded ? base_name(game_path) : "(choose one)");
	snprintf(line[M_RESUME], 80, "%s", game_loaded ? (needs_reload ? "Start again with the new settings" : "Resume") : "-");
	snprintf(line[M_RESET], 80, "%s", game_loaded ? "Reset the game" : "-");
	snprintf(line[M_SH2], 80, "SH-2 CPUs:     %s", cfg.sh2 == SH2_DRC ? "Recompiler (DRC)" : "Interpreter");
	snprintf(line[M_BIOS], 80, "32X BIOS:      %s", cfg.bios == BIOS_FILES ? "Files from the bios folder" : "Built-in (no files needed)");
	snprintf(line[M_RENDER], 80, "Renderer:      %s", render_names[cfg.renderer]);
	snprintf(line[M_SKIP], 80, "Frameskip:     %s", cfg.frameskip ? "Auto (when the sound runs short)" : "Off");
	snprintf(line[M_SOUND], 80, "Sound:         %s", cfg.sound ? "On" : "Off");
	snprintf(line[M_LIMIT], 80, "Speed limit:   %s", cfg.limit ? "On (normal speed)" : "Off (as fast as it can)");
	snprintf(line[M_FPS], 80, "Show speed:    %s", cfg.fps ? "On" : "Off");
	snprintf(line[M_PAD], 80, "Pad:           %s", cfg.pad6 ? "6 buttons" : "3 buttons");
	snprintf(line[M_WIDE], 80, "TV:            %s", cfg.widescreen ? "16:9 (picture kept 4:3)" : "4:3");
	snprintf(line[M_EXIT], 80, "Exit to the Homebrew Channel");

	printf("\x1b[2J");
	pos(1, 0);
	printf(C_TITLE " " APP_NAME " " APP_VERSION "  -  PicoDrive %s for Wii" C_NORMAL "\n\n", library_version);
	for (i = 0; i < M_COUNT; i++)
		printf("%s %s %s\n", i == sel ? C_SEL : C_NORMAL, i == sel ? ">" : " ", line[i]);

	printf(C_DIM "\n BIOS files in %s:\n", bios_dir);
	printf("   32X_G_BIOS.BIN %s   32X_M_BIOS.BIN %s   32X_S_BIOS.BIN %s\n",
		bios_found[0] ? "ok" : "--", bios_found[1] ? "ok" : "--", bios_found[2] ? "ok" : "--");
	printf("\n Menu in game: HOME (Wii remote, Classic), Z + Right (GameCube),\n");
	printf(" Mode + Right (USB pad). Speed: frames per second, %% of full speed.\n");
	printf(" SH-2 and BIOS changes start the game again." C_NORMAL "\n");
}

/* Returns when a game should run */
static void menu(void)
{
	int sel = game_loaded ? M_RESUME : M_GAME, redraw = 1, changed = 0;
	check_bios();
	VIDEO_SetNextFramebuffer(menu_xfb);
	VIDEO_Flush();

	while (!power_off)
	{
		if (redraw)
		{
			draw_menu(sel);
			redraw = 0;
		}
		VIDEO_WaitVSync();
		u32 k = menu_keys();
		int dir = (k & K_RIGHT) ? 1 : (k & K_LEFT) ? -1 : (k & K_OK) ? 1 : 0;

		if ((k & (K_BACK | K_HOME)) && game_loaded && !needs_reload)
			break; /* back to the game */
		if (k & K_UP)
		{
			do sel = (sel + M_COUNT - 1) % M_COUNT;
			while (!game_loaded && (sel == M_RESUME || sel == M_RESET));
			redraw = 1;
		}
		if (k & K_DOWN)
		{
			do sel = (sel + 1) % M_COUNT;
			while (!game_loaded && (sel == M_RESUME || sel == M_RESET));
			redraw = 1;
		}
		if (!dir)
			continue;
		redraw = 1;
		switch (sel)
		{
		case M_GAME:
			if (k & K_OK)
			{
				if (menu_roms())
				{
					if (changed)
						cfg_save();
					return;
				}
			}
			break;
		case M_RESUME:
			if (k & K_OK)
			{
				if (changed)
					cfg_save();
				if (needs_reload)
				{
					char path[256];
					snprintf(path, sizeof(path), "%s", game_path);
					if (!load_game(path))
						break;
				}
				return;
			}
			break;
		case M_RESET:
			if (k & K_OK)
			{
				retro_reset();
				return;
			}
			break;
		case M_SH2:
			cfg.sh2 ^= 1; changed = 1; if (game_loaded) needs_reload = 1;
			break;
		case M_BIOS:
			cfg.bios ^= 1; changed = 1; if (game_loaded) needs_reload = 1;
			break;
		case M_RENDER:
			cfg.renderer = (cfg.renderer + dir + 3) % 3; changed = 1; vars_changed = 1;
			break;
		case M_SKIP:
			cfg.frameskip ^= 1; changed = 1; vars_changed = 1;
			break;
		case M_SOUND:
			cfg.sound ^= 1; changed = 1;
			break;
		case M_LIMIT:
			cfg.limit ^= 1; changed = 1;
			break;
		case M_FPS:
			cfg.fps ^= 1; changed = 1;
			break;
		case M_PAD:
			cfg.pad6 ^= 1; changed = 1;
			if (game_loaded)
			{
				retro_set_controller_port_device(0, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, cfg.pad6 ? 1 : 0));
				retro_set_controller_port_device(1, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, cfg.pad6 ? 1 : 0));
			}
			break;
		case M_WIDE:
			cfg.widescreen ^= 1; changed = 1;
			break;
		case M_EXIT:
			if (k & K_OK)
			{
				if (changed)
					cfg_save();
				power_off = 2;
				return;
			}
			break;
		}
	}
	if (changed)
		cfg_save();
}

/* ------------------------------------------------------------------------ */
/* Main                                                                     */
/* ------------------------------------------------------------------------ */

static void make_dirs(void)
{
	char path[96];
	mkdir(base_dir, 0777);
	snprintf(path, sizeof(path), "%s/roms", base_dir); mkdir(path, 0777);
	mkdir(bios_dir, 0777);
	mkdir(save_dir, 0777);
}

int main(int argc, char **argv)
{
	struct retro_system_info sys;
	(void) argc; (void) argv;

	video_init();
	printf("\x1b[2J\x1b[2;0H " APP_NAME " " APP_VERSION " starting...\n");

	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	AUDIO_Init(NULL);
	PAD_Init();
	SYS_SetPowerCallback(power_cb);
	WPAD_SetPowerButtonCallback(wpad_power_cb);
	SYS_SetResetCallback(reset_cb);

	if (!fatInitDefault())
	{
		printf(C_ERR "\n No SD card or USB drive found.\n" C_NORMAL);
		sleep(5);
		return 0;
	}
	/* the folder on the SD card if there is one, else on the USB drive */
	{
		DIR *d = opendir("sd:/");
		if (d)
			closedir(d);
		else
			snprintf(base_dir, sizeof(base_dir), "usb:/" APP_FOLDER);
	}
	snprintf(bios_dir, sizeof(bios_dir), "%s/bios", base_dir);
	snprintf(save_dir, sizeof(save_dir), "%s/saves", base_dir);
	make_dirs();
	cfg_load();
	log_printf(APP_NAME " " APP_VERSION ", folder %s\n", base_dir);

	retro_set_environment(core_environment);
	retro_set_video_refresh(core_video);
	retro_set_audio_sample(core_audio_sample);
	retro_set_audio_sample_batch(core_audio_batch);
	retro_set_input_poll(core_input_poll);
	retro_set_input_state(core_input_state);
	retro_init();
	retro_get_system_info(&sys);
	library_version = sys.library_version ? sys.library_version : "";
	log_printf("PicoDrive %s\n", library_version);

	while (!power_off)
	{
		menu();
		if (power_off)
			break;
		if (game_loaded)
			run_game();
	}

	unload_game();
	retro_deinit();
	log_write();
	if (power_off == 1)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	return 0;
}
