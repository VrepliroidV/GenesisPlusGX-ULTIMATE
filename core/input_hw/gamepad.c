/***************************************************************************************
 *  Genesis Plus
 *  2-Buttons, 3-Buttons & 6-Buttons controller support
 *  with support for J-Cart, 4-Way Play & Master Tap adapters
 *
 *  Copyright (C) 2007-2019  Eke-Eke (Genesis Plus GX)
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

#include "shared.h"
#include "gamepad.h"

static struct
{
  uint8 State;
  uint8 Counter;
  uint8 Timeout;
  uint32 Latency;
} gamepad[MAX_DEVICES];

static struct
{
  uint8 Latch;
  uint8 Counter;
} flipflop[2];

static uint8 latch;

/* GPGX ULTIMATE, test build: the accesses of a Paprium game to the port A
   pad during the last frames, written to a text file by padlog_save() */
#define PADLOG_SIZE 40000
typedef struct
{
  uint32 frame;     /* frame number */
  uint32 cycles;    /* 68k cycles in the frame */
  uint32 pc;        /* 68k program counter */
  uint16 line;      /* VDP line */
  uint16 a, b;      /* pad state (accesses); held, pressed words (frame end) */
  uint8 op;         /* 'W' write, 'R' read, 'F' frame end */
  uint8 val;        /* data written or read; pad type byte (frame end) */
  uint8 mask;       /* TH/TR direction bits (write) */
  uint8 step;       /* 6-button counter | TH state, before the access */
  uint16 speed;     /* 68k speed in percent (overclock above 100) */
  uint32 raw, host; /* frame end: the USB pad's buttons, the frontend's pad buttons */
} padlog_t;
uint32 padlog_host[2];  /* set by the frontend: USB pad buttons, pad buttons */
static padlog_t *padlog;
static unsigned int padlog_pos, padlog_count, padlog_frame;
static uint32 padlog_pcs[16];
static int padlog_npcs;

static void padlog_add(int op, unsigned int val, unsigned int mask, unsigned int a, unsigned int b)
{
  padlog_t *e;

  if (!(cart.special & HW_PUNCHIUM))
    return;
  if (!padlog)
  {
    padlog = (padlog_t *)malloc(PADLOG_SIZE * sizeof(padlog_t));
    if (!padlog)
      return;
  }

  e = &padlog[padlog_pos];
  padlog_pos = (padlog_pos + 1) % PADLOG_SIZE;
  if (padlog_count < PADLOG_SIZE)
    padlog_count++;

  e->frame = padlog_frame;
  e->cycles = m68k.cycles;
  e->pc = m68k.pc;
  e->line = v_counter;
  e->a = a;
  e->b = b;
  e->raw = padlog_host[0];
  e->host = padlog_host[1];
  e->op = op;
  e->val = val;
  e->mask = mask;
  e->step = gamepad[0].Counter | (gamepad[0].State >> 6);
#ifdef M68K_OVERCLOCK_SHIFT
  e->speed = (m68k.cycle_ratio > 0) ? (uint16)(((long long)100 << M68K_OVERCLOCK_SHIFT) / m68k.cycle_ratio) : 0;
#else
  e->speed = 100;
#endif

  /* the code reading the pad (dumped with the log) */
  if (op != 'F')
  {
    int i;
    for (i = 0; i < padlog_npcs; i++)
      if ((padlog_pcs[i] >> 8) == (m68k.pc >> 8)) break;
    if ((i == padlog_npcs) && (padlog_npcs < 16))
      padlog_pcs[padlog_npcs++] = m68k.pc;
  }
}

static unsigned int padlog_byte(uint32 address)
{
  address &= 0xFFFFFF;
  if (address >= 0xE00000)
    return READ_BYTE(work_ram, address & 0xFFFF);
  if (m68k.memory_map[address >> 16].read8)
    return m68k.memory_map[address >> 16].read8(address) & 0xFF;
  if (m68k.memory_map[address >> 16].base)
    return READ_BYTE(m68k.memory_map[address >> 16].base, address & 0xFFFF);
  return 0xFF;
}

int padlog_save(const char *filename, const char *header)
{
  FILE *f;
  unsigned int i, n;

  if (!padlog || !padlog_count)
    return 0;
  f = fopen(filename, "w");
  if (!f)
    return 0;

  fprintf(f, "Paprium pad log: port A device %d (0 = 3 buttons, 1 = 6 buttons), %u entries\n", input.dev[0], padlog_count);
  if (header)
    fprintf(f, "%s", header);
  fprintf(f, "frame line cycles pc op value mask step | pad / F: type held pressed 68k-speed%% usb-buttons pad-buttons\n");
  n = (padlog_pos + PADLOG_SIZE - padlog_count) % PADLOG_SIZE;
  for (i = 0; i < padlog_count; i++)
  {
    padlog_t *e = &padlog[(n + i) % PADLOG_SIZE];
    if (e->op == 'F')
      fprintf(f, "%u %u %u F type=%02X held=%04X pressed=%04X speed=%u usb=%08X pad=%04X\n", e->frame, e->line, e->cycles, e->val, e->a, e->b, e->speed, e->raw, e->host);
    else
      fprintf(f, "%u %u %u %06X %c %02X %02X %u | %04X\n", e->frame, e->line, e->cycles, e->pc, e->op, e->val, e->mask, e->step, e->a);
  }

  /* the code around each place reading the pad */
  for (i = 0; i < (unsigned int)padlog_npcs; i++)
  {
    uint32 start = (padlog_pcs[i] & ~1) - 0x100;
    uint32 a;
    fprintf(f, "\ncode near %06X:", padlog_pcs[i]);
    for (a = start; a < start + 0x200; a += 2)
    {
      if (!((a - start) & 0x1F))
        fprintf(f, "\n%06X:", a);
      fprintf(f, " %02X%02X", padlog_byte(a), padlog_byte(a + 1));
    }
    fprintf(f, "\n");
  }

  fclose(f);
  return 1;
}


void gamepad_reset(int port)
{
  /* default state (Gouketsuji Ichizoku / Power Instinct, Samurai Spirits / Samurai Shodown) */
  gamepad[port].State = 0x40;
  gamepad[port].Counter = 0;
  gamepad[port].Timeout = 0;
  gamepad[port].Latency = 0;

  /* reset 4-WayPlay latch (controller #0 used by default) */
  latch = 0x00;

  /* reset Master Tap flip-flop */
  flipflop[port>>2].Latch = 0;
  flipflop[port>>2].Counter = 0;
}

void gamepad_refresh(int port)
{
  /* 6-buttons pad */
  if (gamepad[port].Timeout++ > 25)
  {
    gamepad[port].Counter = 0;
    gamepad[port].Timeout = 0;
  }
}

void gamepad_end_frame(int port, unsigned int cycles)
{
  if ((port == 0) && (cart.special & HW_PUNCHIUM))
  {
    padlog_add('F', READ_BYTE(work_ram, 0x7035), 0, READ_WORD(work_ram, 0x7028), READ_WORD(work_ram, 0x702A));
    padlog_frame++;
  }

  if (gamepad[port].Latency > cycles)
  {
    /* adjust TH direction switching latency for next frame */
    gamepad[port].Latency -= cycles;
  }
  else
  {
    /* reset TH direction switching latency */
    gamepad[port].Latency = 0;
  }
}

INLINE unsigned char gamepad_read(int port)
{
  /* D7 is not connected, D6 returns TH input state */
  unsigned int data = gamepad[port].State | 0x3F;

  /* pad state */
  unsigned int pad = input.pad[port];

  /* get current TH input pulse counter */
  unsigned int step = gamepad[port].Counter | (data >> 6);

  /* get current timestamp */
  unsigned int cycles = ((system_hw & SYSTEM_PBC) == SYSTEM_MD) ? m68k.cycles : Z80.cycles;

  /* TH direction switching latency */
  if (cycles < gamepad[port].Latency)
  {
    /* TH internal state switching has not occured yet (Decap Attack) */
    step &= ~1;
  }

  /* C/B or START/A buttons status is returned on D5-D4 (active low) */
  /* D-PAD or extra buttons status is returned on D3-D0 (active low) */
  switch (step)
  {
    /* From gen_hw.txt (*):

       A 6-button gamepad allows the extra buttons to be read based on how
       many times TH is switched from 0 to 1. Observe the following sequence:

       TH = 1 : ?1CBRLDU    3-button pad return value
       TH = 0 : ?0SA00DU    3-button pad return value
       TH = 1 : ?1CBRLDU    3-button pad return value (*)
       TH = 0 : ?0SA00DU    3-button pad return value (*)
       TH = 1 : ?1CBRLDU    3-button pad return value
       TH = 0 : ?0SA0000    D3-D0 are forced to '0'
       TH = 1 : ?1CBMXYZ    Extra buttons returned in D3-0
       TH = 0 : ?0SA1111    D3-D0 are forced to '1'

       From this point on, the standard 3-button pad values will be returned if any further TH transitions are done.

       (*) additional High-to-Low transition is necessary to access extra buttons according to official MK-1653-50 specification 
    */
    case 4: /*** Third Low ***/
    {
      /* TH = 0 : ?0SA0000    D3-D0 forced to '0' */
      data &= ~(((pad >> 2) & 0x30) | 0x0F);
      break;
    }

    case 7: /*** Fourth High ***/
    {
      /* TH = 1 : ?1CBMXYZ    Extra buttons returned in D3-D0 */
      data &= ~((pad & 0x30) | ((pad >> 8) & 0x0F));
      break;
    }

    case 6: /*** Fourth Low ***/
    {
      /* TH = 0 : ?0SA1111    D3-D0 forced to '1' */
      data &= ~((pad >> 2) & 0x30);
      break;
    }

    default: /*** 3-button mode ***/
    {
      if (step & 1)
      {
        /* TH = 1 : ?1CBRLDU */
        data &= ~(pad & 0x3F);
      }
      else
      {
        /* TH = 0 : ?0SA00DU */
        data &= ~((pad & 0x03) | ((pad >> 2) & 0x30) | 0x0C);
      }
      break;
    }
  }

  if (port == 0)
    padlog_add('R', data, 0, pad, 0);

  return data;
}

INLINE void gamepad_write(int port, unsigned char data, unsigned char mask)
{
  if (port == 0)
    padlog_add('W', data, mask, input.pad[0], 0);

  /* Check TH pin direction */
  if (mask & 0x40)
  {
    /* get TH output state */
    data &= 0x40;

    /* reset TH direction switching latency */
    gamepad[port].Latency = 0;

    /* 6-Buttons controller specific */
    if ((input.dev[port] == DEVICE_PAD6B) && (gamepad[port].Counter < 8))
    {
      /* TH 0->1 transition */
      if (data && !gamepad[port].State)
      {
        gamepad[port].Counter += 2;
        gamepad[port].Timeout = 0;
      }
    }
  }
  else
  {
    /* retrieve current timestamp */
    unsigned int cycles = ((system_hw & SYSTEM_PBC) == SYSTEM_MD) ? m68k.cycles : Z80.cycles;

    /* TH is pulled high when not configured as output by I/O controller */
    data = 0x40;

    /* TH 0->1 internal switching does not occur immediately (verified on MK-1650 model) */
    if (!gamepad[port].State)
    {
      gamepad[port].Latency = cycles + 172;
    }
  }

  /* update TH input state */
  gamepad[port].State = data;
}

/*--------------------------------------------------------------------------*/
/*  Default ports handlers                                                  */
/*--------------------------------------------------------------------------*/

unsigned char gamepad_1_read(void)
{
  return gamepad_read(0);
}

unsigned char gamepad_2_read(void)
{
  return gamepad_read(4);
}

void gamepad_1_write(unsigned char data, unsigned char mask)
{
  gamepad_write(0, data, mask);
}

void gamepad_2_write(unsigned char data, unsigned char mask)
{
  gamepad_write(4, data, mask);
}

/*--------------------------------------------------------------------------*/
/*  4-WayPlay ports handler                                                 */
/*--------------------------------------------------------------------------*/

unsigned char wayplay_1_read(void)
{
  /* check if latched TH input on port B is HIGH */
  if (latch & 0x04)
  {
    /* 4-WayPlay detection : xxxxx00 */
    return 0x7C;
  }

  /* latched TR & TL input state on port B select controller # (0-3) on port A */
  return gamepad_read(latch);
}

unsigned char wayplay_2_read(void)
{
  return 0x7F;
}

void wayplay_1_write(unsigned char data, unsigned char mask)
{
  /* latched TR & TL input state on port B select controller # (0-3) on port A */
  gamepad_write(latch & 0x03, data, mask);
}

void wayplay_2_write(unsigned char data, unsigned char mask)
{
  /* pins not configured as output by I/O controller are pulled HIGH */
  data |= ~mask;

  /* check if both UP & DOWN inputs are LOW */
  if (!(data & 0x03))
  {
    /* latch TH, TR & TL input state */
    latch = (data >> 4) & 0x07;
  }
}


/*--------------------------------------------------------------------------*/
/*  J-Cart memory handlers                                                  */
/*--------------------------------------------------------------------------*/

unsigned int jcart_read(unsigned int address)
{
   /* D6 returns TH state, D14 is fixed low (fixes Micro Machines 2) */
   return (gamepad_read(5) | ((gamepad_read(6) & 0x3F) << 8));
}

void jcart_write(unsigned int address, unsigned int data)
{
  data = (data & 0x01) << 6;
  gamepad_write(5, data, 0x40);
  gamepad_write(6, data, 0x40);
}


/*--------------------------------------------------------------------------*/
/*  Master Tap ports handler (unofficial, designed by Furrtek)              */
/*  cf. http://www.smspower.org/uploads/Homebrew/BOoM-SMS-sms4p_2.png       */
/*--------------------------------------------------------------------------*/
unsigned char mastertap_1_read(void)
{
  return gamepad_read(flipflop[0].Counter);
}

unsigned char mastertap_2_read(void)
{
  return gamepad_read(flipflop[1].Counter + 4);
}

void mastertap_1_write(unsigned char data, unsigned char mask)
{
  /* update bits set as output only */
  data = (flipflop[0].Latch & ~mask) | (data & mask);
  
  /* check TH 1->0 transitions */
  if ((flipflop[0].Latch & 0x40) && !(data & 0x40))
  {
    flipflop[0].Counter = (flipflop[0].Counter + 1) & 0x03;
  }

  /* update internal state */
  flipflop[0].Latch = data;
}

void mastertap_2_write(unsigned char data, unsigned char mask)
{
  /* update bits set as output only */
  data = (flipflop[1].Latch & ~mask) | (data & mask);
  
  /* check TH 1->0 transition */
  if ((flipflop[1].Latch & 0x40) && !(data & 0x40))
  {
    flipflop[1].Counter = (flipflop[1].Counter + 1) & 0x03;
  }

  /* update internal state */
  flipflop[1].Latch = data;
}
