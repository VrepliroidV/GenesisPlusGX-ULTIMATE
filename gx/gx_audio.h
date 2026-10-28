/****************************************************************************
 *  gx_audio.c
 *
 *  Genesis Plus GX audio support
 *
 *  Copyright Eke-Eke (2007-2015), based on original work from Softdev (2006)
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

#ifndef _GC_AUDIO_H_
#define _GC_AUDIO_H_

extern u32 audioSync;

extern void gx_audio_Init(void);
extern void gx_audio_Shutdown(void);
extern void gx_audio_Start(void);
extern void gx_audio_Stop(void);
extern int gx_audio_Update(int status);
extern int gx_audio_Queued(void); /* microseconds of sound left for the audio DMA, -1 before it starts */

/* Menu music (config.menu_music): the OGG files of /genplusultimate/music */
#define MAX_MENU_MUSIC 512
enum { MUSIC_STOPPED, MUSIC_PLAYING, MUSIC_PAUSED };
extern int gx_audio_MusicList(char (*names)[128], int max); /* names of BEZEL_NAME_MAX */
extern void gx_audio_MusicRestart(void); /* the setting changed: plays its music from the start */
/* The music plays in the menu before a game is loaded, not in the in-game
   menu; the Load Game menu plays it again, from the beginning. */
extern void gx_audio_MusicEnable(int on);
extern int gx_audio_MusicEnabled(void);  /* the music and its player are shown */

/* the mini player of the menu */
extern void gx_audio_MusicPlay(void);
extern void gx_audio_MusicPause(void);
extern void gx_audio_MusicStop(void);
extern void gx_audio_MusicSkip(int dir); /* next (1) or previous (-1) file of the folder */
extern int gx_audio_MusicCount(void);    /* files in the music folder (known once the menu opened) */
extern int gx_audio_MusicState(void);
extern void gx_audio_MusicName(char *out, int len); /* the file without ".ogg", "" for none */

#endif
