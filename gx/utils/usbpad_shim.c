/*
 * usbpad_shim.c - XInput USB controllers seen as GameCube controllers
 *
 * Copyright (c) 2026 VrepliroidV
 * Released under the zlib license, see xinput_usb.h.
 */

#ifdef HW_RVL

#define USBPAD_SHIM_IMPL
#include "usbpad_shim.h"
#include "xinput_usb.h"

static u16 usb_held[XI_MAX_PORTS];
static u16 usb_down[XI_MAX_PORTS];
static u16 usb_up[XI_MAX_PORTS];
static u16 menu_held[XI_MAX_PORTS];
static u16 menu_down[XI_MAX_PORTS];
static int md_menu;

/* Picks GameCube buttons so the default Genesis Plus GX mapping
   (B->A, A->B, X->C, L->X, Y->Y, R->Z, Z->Mode) lands on the labels of a
   6-button pad in XInput mode. On most pads the triggers and LB all give C,
   so C works whichever of them a pad uses for it. On the 6-button pads whose
   triggers are buttons (the Retroflag Classic Controller-M: L is LB, R is
   LT, C is RT, Z is RB), only RT is C, and L and R are Mode, so C and Mode
   are different buttons for the key mapping too. */
static u16 to_gamecube(u32 x, bool six_button)
{
	u16 p = 0;
	if (x & XI_ANY_UP)    p |= PAD_BUTTON_UP;
	if (x & XI_ANY_DOWN)  p |= PAD_BUTTON_DOWN;
	if (x & XI_ANY_LEFT)  p |= PAD_BUTTON_LEFT;
	if (x & XI_ANY_RIGHT) p |= PAD_BUTTON_RIGHT;
	if (x & XI_A) p |= PAD_BUTTON_B;               /* A */
	if (x & XI_B) p |= PAD_BUTTON_A;               /* B */
	if (six_button)
	{
		if (x & XI_RT) p |= PAD_BUTTON_X;               /* C */
		if (x & (XI_LT | XI_LB)) p |= PAD_TRIGGER_Z;    /* L, R: Mode */
	}
	else if (x & (XI_LT | XI_RT | XI_LB)) p |= PAD_BUTTON_X; /* C */
	if (x & XI_X) p |= PAD_TRIGGER_L;              /* X */
	if (x & XI_Y) p |= PAD_BUTTON_Y;               /* Y */
	if (x & XI_RB) p |= PAD_TRIGGER_R;             /* Z */
	if (x & XI_BACK) p |= PAD_TRIGGER_Z;           /* Mode */
	if (x & XI_START) p |= PAD_BUTTON_START;
	return p;
}

/* Menu buttons: the ones of the game, except on the 6-button pads (their
   triggers are buttons: the Retroflag Classic Controller-M sends C as RT),
   which confirm with C (A of the menus) and go back with B (B of the menus),
   as on a Mega Drive; their A does nothing there. Such a pad is recognised
   once C or the right shoulder button (a trigger too) is pressed, and that
   press already counts. */
static u16 to_menu(u32 x, bool six_button)
{
	u16 p = to_gamecube(x, six_button);
	if (six_button)
	{
		p &= ~(PAD_BUTTON_A | PAD_BUTTON_B | PAD_TRIGGER_Z);
		if (x & XI_RT)   p |= PAD_BUTTON_A;   /* C */
		if (x & XI_B)    p |= PAD_BUTTON_B;   /* B */
		if (x & XI_BACK) p |= PAD_TRIGGER_Z;  /* Mode (L and R do nothing here) */
	}
	return p;
}

u32 USBPAD_ScanPads(void)
{
	u32 connected = PAD_ScanPads();
	int md = 0;

	XInput_Update();
	for (int i = 0; i < XI_MAX_PORTS; i++)
	{
		u32 x = XInput_ButtonsHeld(i);
		bool six_button = XInput_TriggersAsButtons(i);
		u16 now = to_gamecube(x, six_button);
		usb_down[i] = now & ~usb_held[i];
		usb_up[i] = usb_held[i] & ~now;
		usb_held[i] = now;

		now = to_menu(x, six_button);
		menu_down[i] = now & ~menu_held[i];
		menu_held[i] = now;
		if (six_button)
			md = 1;
	}
	md_menu = md;

	return connected | XInput_ConnectedMask();
}

u16 USBPAD_MenuButtonsHeld(int chan)
{
	u16 b = PAD_ButtonsHeld(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= menu_held[chan];
	return b;
}

u16 USBPAD_MenuButtonsDown(int chan)
{
	u16 b = PAD_ButtonsDown(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= menu_down[chan];
	return b;
}

int USBPAD_MegaDriveMenu(void)
{
	return md_menu;
}

u16 USBPAD_ButtonsHeld(int chan)
{
	u16 b = PAD_ButtonsHeld(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= usb_held[chan];
	return b;
}

u16 USBPAD_ButtonsDown(int chan)
{
	u16 b = PAD_ButtonsDown(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= usb_down[chan];
	return b;
}

u16 USBPAD_ButtonsUp(int chan)
{
	u16 b = PAD_ButtonsUp(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= usb_up[chan];
	return b;
}

#endif /* HW_RVL */
