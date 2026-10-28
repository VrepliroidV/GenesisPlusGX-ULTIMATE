/*
 * (C) Gražvydas "notaz" Ignotas, 2012
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 *  - MAME license.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <assert.h>
#include <SDL.h>
#include "input.h"
#include "in_sdl.h"

#define IN_SDL_PREFIX "sdl:"
/* should be machine word for best performace */
typedef unsigned long keybits_t;
#define KEYBITS_WORD_BITS (sizeof(keybits_t) * 8)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

struct in_sdl_state {
	const in_drv_t *drv;
	SDL_Joystick *joy;
	int joy_id;
	int joy_numaxes;
	int joy_numbuttons;
	int joy_hat_down;
	int *joy_axis_keydown;
	unsigned int joy_axis_as_btn; // bitmask; vs axes of centered sticks
	unsigned int redraw:1;
	unsigned int abs_to_udlr:1;
	SDL_Event revent;
	SDL_Event mevent; // last mouse event
	short mousebtns;  // mouse buttons pressed mask
	short mousewheel; // mouse wheel scroll counter
	keybits_t keystate[SDLK_LAST / KEYBITS_WORD_BITS + 1];
	// emulator keys should always be processed immediately lest one is lost
	keybits_t emu_keys[SDLK_LAST / KEYBITS_WORD_BITS + 1];
	// "parent" state tracks all joy states to write to their ev_queue
	unsigned int joy_states_count;
	// event queues for dispatching events to this device
	SDL_Event *ev_queue;
	unsigned int ev_count;
	unsigned int ev_alloc;
	struct in_sdl_state *parent;
	struct in_sdl_state *mouse_state;
	struct in_sdl_state *joy_states[0];
};

static void (*ext_event_handler)(void *event);

static const char * const in_sdl_keys[SDLK_LAST] = {
	[SDLK_BACKSPACE] = "backspace",
	[SDLK_TAB] = "tab",
	[SDLK_CLEAR] = "clear",
	[SDLK_RETURN] = "return",
	[SDLK_PAUSE] = "pause",
	[SDLK_ESCAPE] = "escape",
	[SDLK_SPACE] = "space",
	[SDLK_EXCLAIM]  = "!",
	[SDLK_QUOTEDBL]  = "\"",
	[SDLK_HASH]  = "#",
	[SDLK_DOLLAR]  = "$",
	[SDLK_AMPERSAND]  = "&",
	[SDLK_QUOTE] = "'",
	[SDLK_LEFTPAREN] = "(",
	[SDLK_RIGHTPAREN] = ")",
	[SDLK_ASTERISK] = "*",
	[SDLK_PLUS] = "+",
	[SDLK_COMMA] = ",",
	[SDLK_MINUS] = "-",
	[SDLK_PERIOD] = ".",
	[SDLK_SLASH] = "/",
	[SDLK_0] = "0",
	[SDLK_1] = "1",
	[SDLK_2] = "2",
	[SDLK_3] = "3",
	[SDLK_4] = "4",
	[SDLK_5] = "5",
	[SDLK_6] = "6",
	[SDLK_7] = "7",
	[SDLK_8] = "8",
	[SDLK_9] = "9",
	[SDLK_COLON] = ":",
	[SDLK_SEMICOLON] = ";",
	[SDLK_LESS] = "<",
	[SDLK_EQUALS] = "=",
	[SDLK_GREATER] = ">",
	[SDLK_QUESTION] = "?",
	[SDLK_AT] = "@",
	[SDLK_LEFTBRACKET] = "[",
	[SDLK_BACKSLASH] = "\\",
	[SDLK_RIGHTBRACKET] = "]",
	[SDLK_CARET] = "^",
	[SDLK_UNDERSCORE] = "_",
	[SDLK_BACKQUOTE] = "`",
	[SDLK_a] = "a",
	[SDLK_b] = "b",
	[SDLK_c] = "c",
	[SDLK_d] = "d",
	[SDLK_e] = "e",
	[SDLK_f] = "f",
	[SDLK_g] = "g",
	[SDLK_h] = "h",
	[SDLK_i] = "i",
	[SDLK_j] = "j",
	[SDLK_k] = "k",
	[SDLK_l] = "l",
	[SDLK_m] = "m",
	[SDLK_n] = "n",
	[SDLK_o] = "o",
	[SDLK_p] = "p",
	[SDLK_q] = "q",
	[SDLK_r] = "r",
	[SDLK_s] = "s",
	[SDLK_t] = "t",
	[SDLK_u] = "u",
	[SDLK_v] = "v",
	[SDLK_w] = "w",
	[SDLK_x] = "x",
	[SDLK_y] = "y",
	[SDLK_z] = "z",
	[SDLK_DELETE] = "delete",

	// gamepad buttons
	#define B(x) [SDLK_WORLD_##x] = "b"#x
	B(0), B(1), B(2), B(3), B(4), B(5), B(6), B(7),
	B(8), B(9), B(10), B(11), B(12), B(13), B(14), B(15),
	B(16), B(17), B(18), B(19), B(20), B(21), B(22), B(23),
	B(24), B(25), B(26), B(27), B(28), B(29), B(30), B(31),
	B(32), B(33), B(34), B(35), B(36), B(37), B(38), B(39),
	#undef B

	// mouse buttons
	[SDLK_WORLD_81] = "left button",
	[SDLK_WORLD_82] = "middle button",
	[SDLK_WORLD_83] = "right button",
	[SDLK_WORLD_84] = "wheel up",
	[SDLK_WORLD_85] = "wheel down",
	[SDLK_WORLD_86] = "x1 button",
	[SDLK_WORLD_87] = "x2 button",

	[SDLK_KP0] = "[0]",
	[SDLK_KP1] = "[1]",
	[SDLK_KP2] = "[2]",
	[SDLK_KP3] = "[3]",
	[SDLK_KP4] = "[4]",
	[SDLK_KP5] = "[5]",
	[SDLK_KP6] = "[6]",
	[SDLK_KP7] = "[7]",
	[SDLK_KP8] = "[8]",
	[SDLK_KP9] = "[9]",
	[SDLK_KP_PERIOD] = "[.]",
	[SDLK_KP_DIVIDE] = "[/]",
	[SDLK_KP_MULTIPLY] = "[*]",
	[SDLK_KP_MINUS] = "[-]",
	[SDLK_KP_PLUS] = "[+]",
	[SDLK_KP_ENTER] = "enter",
	[SDLK_KP_EQUALS] = "equals",

	[SDLK_UP] = "up",
	[SDLK_DOWN] = "down",
	[SDLK_RIGHT] = "right",
	[SDLK_LEFT] = "left",
	[SDLK_INSERT] = "insert",
	[SDLK_HOME] = "home",
	[SDLK_END] = "end",
	[SDLK_PAGEUP] = "page up",
	[SDLK_PAGEDOWN] = "page down",

	[SDLK_F1] = "f1",
	[SDLK_F2] = "f2",
	[SDLK_F3] = "f3",
	[SDLK_F4] = "f4",
	[SDLK_F5] = "f5",
	[SDLK_F6] = "f6",
	[SDLK_F7] = "f7",
	[SDLK_F8] = "f8",
	[SDLK_F9] = "f9",
	[SDLK_F10] = "f10",
	[SDLK_F11] = "f11",
	[SDLK_F12] = "f12",
	[SDLK_F13] = "f13",
	[SDLK_F14] = "f14",
	[SDLK_F15] = "f15",

	[SDLK_NUMLOCK] = "numlock",
	[SDLK_CAPSLOCK] = "caps lock",
	[SDLK_SCROLLOCK] = "scroll lock",
	[SDLK_RSHIFT] = "right shift",
	[SDLK_LSHIFT] = "left shift",
	[SDLK_RCTRL] = "right ctrl",
	[SDLK_LCTRL] = "left ctrl",
	[SDLK_RALT] = "right alt",
	[SDLK_LALT] = "left alt",
	[SDLK_RMETA] = "right meta",
	[SDLK_LMETA] = "left meta",
	[SDLK_LSUPER] = "left super",	/* "Windows" keys */
	[SDLK_RSUPER] = "right super",	
	[SDLK_MODE] = "alt gr",
	[SDLK_COMPOSE] = "compose",
};

static struct in_sdl_state *state_alloc(int joy_numaxes, int joy_state_count)
{
	struct in_sdl_state *state;

	state = calloc(1, sizeof(*state) + sizeof(state->joy_states[0]) * joy_state_count);
	if (state == NULL) {
		fprintf(stderr, "in_sdl: OOM\n");
		return NULL;
	}

	if (joy_numaxes) {
		state->abs_to_udlr = 1;
		state->joy_numaxes = joy_numaxes;
		state->joy_axis_keydown = malloc(joy_numaxes * sizeof(state->joy_axis_keydown[0]));
		if (!state->joy_axis_keydown) {
			free(state);
			return NULL;
		}
		memset(state->joy_axis_keydown, 0xff, joy_numaxes * sizeof(state->joy_axis_keydown[0]));
	}
	state->joy_states_count = joy_state_count;
	return state;
}

static void in_sdl_probe(const in_drv_t *drv)
{
	const struct in_pdata *pdata = drv->pdata;
	const char * const * key_names = in_sdl_keys;
	struct in_sdl_state *state;
	SDL_Joystick *joy;
	int i, a, joycount;
	char name[256];

	if (pdata->key_names)
		key_names = pdata->key_names;

	/* joysticks go here too */
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joycount = SDL_NumJoysticks();

	if (!(state = state_alloc(0, joycount)))
		return;
	state->drv = drv;
	in_register(IN_SDL_PREFIX "keys", -1, state, SDLK_LAST,
		key_names, 0);
	//SDL_EnableUNICODE(1);

	if (pdata->mouse_device && (state->mouse_state = state_alloc(0, 0))) {
		state->mouse_state->drv = drv;
		state->mouse_state->parent = state;
		in_register(IN_SDL_PREFIX "mouse", -1, state->mouse_state, SDLK_LAST,
			key_names, 0);
	}

	for (i = 0; i < joycount; i++) {
		struct in_sdl_state *jstate;
		joy = SDL_JoystickOpen(i);
		if (joy == NULL)
			continue;

		if (!(jstate = state_alloc(SDL_JoystickNumAxes(joy), 0)))
			break;
		jstate->joy = joy;
		jstate->joy_id = i;
		jstate->joy_numbuttons = SDL_JoystickNumButtons(joy);
		jstate->drv = drv;
		jstate->parent = state;
		state->joy_states[i] = jstate;
		for (a = 0; a < jstate->joy_numaxes; a++)
			if (SDL_JoystickGetAxis(joy, a) < -16384)
				jstate->joy_axis_as_btn |= 1u << a;

		snprintf(name, sizeof(name), IN_SDL_PREFIX "%s", SDL_JoystickName(i));
		in_register(name, -1, jstate, SDLK_LAST, key_names, 0);

		printf("  %s: %d buttons %d axes %d hat(s), "
			"guessed axis_as_btn mask: %x\n",
			name, jstate->joy_numbuttons, jstate->joy_numaxes,
			SDL_JoystickNumHats(joy), jstate->joy_axis_as_btn);
	}

	if (joycount > 0)
		SDL_JoystickEventState(SDL_ENABLE);
}

static void in_sdl_free(void *drv_data)
{
	struct in_sdl_state *state = drv_data;
	unsigned int i;

	if (state != NULL) {
		if (state->mouse_state)
			state->mouse_state->parent = NULL;
		if (state->joy != NULL)
			SDL_JoystickClose(state->joy);
		free(state->joy_axis_keydown);
		free(state->ev_queue);
		if (state->parent) {
			if (state->parent->mouse_state == state)
				state->parent->mouse_state = NULL;
			for (i = 0; i < state->parent->joy_states_count; i++) {
				if (state->parent->joy_states[i] == state) {
					state->parent->joy_states[i] = NULL;
					break;
				}
			}
		}
		for (i = 0; i < state->joy_states_count; i++)
			if (state->joy_states[i])
				state->joy_states[i]->parent = NULL;
		free(state);
	}
}

static const char * const *
in_sdl_get_key_names(const in_drv_t *drv, int *count)
{
	const struct in_pdata *pdata = drv->pdata;
	*count = SDLK_LAST;

	if (pdata->key_names)
		return pdata->key_names;
	return in_sdl_keys;
}

/* could use SDL_GetKeyState, but this gives better packing */
static void update_keystate(keybits_t *keystate, int sym, int is_down)
{
	keybits_t *ks_word, mask;

	mask = 1;
	mask <<= sym & (KEYBITS_WORD_BITS - 1);
	ks_word = keystate + sym / KEYBITS_WORD_BITS;
	if (is_down)
		*ks_word |= mask;
	else
		*ks_word &= ~mask;
}

static int get_keystate(keybits_t *keystate, int sym)
{
	keybits_t *ks_word, mask;

	mask = 1;
	mask <<= sym & (KEYBITS_WORD_BITS - 1);
	ks_word = keystate + sym / KEYBITS_WORD_BITS;
	return !!(*ks_word & mask);
}

static int handle_sub_event(struct in_sdl_state *state, const SDL_Event *event,
	int *kc_out, int *down_out, int *emu_out);

static int handle_input_event(struct in_sdl_state *state, const SDL_Event *event,
	int *kc_out, int *down_out, int *emu_out)
{
	struct in_sdl_state *substate = NULL;
	int emu, joy_id = -1;
	int down, key;

	switch (event->type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		key = event->key.keysym.sym;
		down = event->type == SDL_KEYDOWN;
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		substate = state->mouse_state;
		if (!substate)
			return handle_sub_event(state, event, kc_out, down_out, emu_out);
		break;
	case SDL_MOUSEMOTION:
		substate = state->mouse_state;
		if (!substate)
			return handle_sub_event(state, event, kc_out, down_out, emu_out);
		break;
	case SDL_JOYAXISMOTION:
		joy_id = event->jaxis.which;
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		joy_id = event->jbutton.which;
		break;
	case SDL_JOYHATMOTION:
		joy_id = event->jhat.which;
		break;
	default:
		return -1;
	}

	if (joy_id >= 0) {
		// send to the joy queue
		if ((unsigned int)joy_id >= state->joy_states_count ||
		    state->joy_states[joy_id] == NULL) {
			assert(0);
			return -1;
		}
		substate = state->joy_states[joy_id];
	}

	if (substate != NULL) {
		if (substate->ev_count >= substate->ev_alloc) {
			int cnt = substate->ev_count + 16;
			void *tmp = realloc(substate->ev_queue, sizeof(substate->ev_queue[0]) * cnt);
			if (tmp == NULL) {
				assert(0);
				return -1;
			}
			substate->ev_queue = tmp;
			substate->ev_alloc = cnt;
		}
		substate->ev_queue[substate->ev_count++] = *event;
		return 0;
	}

	emu = get_keystate(state->emu_keys, key);
	update_keystate(state->keystate, key, down);
	if (kc_out != NULL)
		*kc_out = key;
	if (down_out != NULL)
		*down_out = down;
	if (emu_out != NULL)
		*emu_out = emu;

	return 1;
}

static int handle_sub_event(struct in_sdl_state *state, const SDL_Event *event,
	int *kc_out, int *down_out, int *emu_out)
{
	static const int hat2uldr[4] = { SDLK_UP, SDLK_RIGHT, SDLK_DOWN, SDLK_LEFT };
	int kc = -1, kc_prev = -1, down = 0, emu = 0, ret = 0, i, val, xor;
	int kc_button_base = SDLK_WORLD_0;
	int kc_axis_base = kc_button_base + state->joy_numbuttons;
	int kc_hat_base = kc_axis_base + state->joy_numaxes;

	int bmask, xrel, yrel;

	switch (event->type) {
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		bmask = SDL_BUTTON(event->button.button);
		if (event->button.state == SDL_PRESSED) {
			// the wheel creates DOWN/UP events at the same time.
			// the batched processing between 2 readouts would lead
			// to them being cancelled out. count them instead.
			if (event->button.button == SDL_BUTTON_WHEELDOWN)
				state->mousewheel -= 1;
			else if (event->button.button == SDL_BUTTON_WHEELUP)
				state->mousewheel += 1;
			else
				state->mousebtns |= bmask;
		} else	state->mousebtns &= ~bmask;
		kc = event->button.button + kc_button_base + 80;
		down = event->type == SDL_MOUSEBUTTONDOWN;
		ret = 1;
		break;
	case SDL_MOUSEMOTION:
		xrel = state->mevent.motion.xrel;
		yrel = state->mevent.motion.yrel;
		state->mevent = *event;
		state->mevent.motion.xrel += xrel;
		state->mevent.motion.yrel += yrel;
		return 0;

	case SDL_JOYAXISMOTION:
		if ((unsigned)event->jaxis.axis >= (unsigned)state->joy_numaxes)
			return 1;
		assert(event->jaxis.which == state->joy_id);
		if (event->jaxis.value < -16384) {
			if (state->abs_to_udlr && event->jaxis.axis < 2)
				kc = event->jaxis.axis ? SDLK_UP : SDLK_LEFT;
			// some pressure sensitive buttons appear as an axis that goes
			// from -32768 (released) to 32767 (pressed) and there is no
			// way to distinguish from ones centered at 0? :(
			else if (!(state->joy_axis_as_btn & (1u << event->jaxis.axis)))
				kc = kc_axis_base + event->jaxis.axis * 2;
			down = 1;
		}
		else if (event->jaxis.value > 16384) {
			if (state->abs_to_udlr && event->jaxis.axis < 2)
				kc = event->jaxis.axis ? SDLK_DOWN : SDLK_RIGHT;
			else
				kc = kc_axis_base + event->jaxis.axis * 2 + 1;
			down = 1;
		}
		kc_prev = state->joy_axis_keydown[event->jaxis.axis];
		state->joy_axis_keydown[event->jaxis.axis] = kc;
		if (kc == kc_prev)
			return -1; // no simulated key change
		if (kc >= 0 && kc_prev >= 0) {
			// must release the old key first
			state->joy_axis_keydown[event->jaxis.axis] = -1;
			kc = kc_prev;
			down = 0;
		}
		else if (kc_prev >= 0)
			kc = kc_prev;
		ret = 1;
		break;

	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		assert(event->jbutton.which == state->joy_id);
		kc = kc_button_base + (int)event->jbutton.button;
		down = event->jbutton.state == SDL_PRESSED;
		ret = 1;
		break;
	case SDL_JOYHATMOTION:
		assert(event->jhat.which == state->joy_id);
		val = event->jhat.value;
		xor = val ^ state->joy_hat_down;
		for (i = 0; i < 4; i++, xor >>= 1, val >>= 1) {
			if (xor & 1) {
				if (event->jhat.hat)
					kc = kc_hat_base + event->jhat.hat * 4 + i;
				else
					kc = hat2uldr[i];
				down = val & 1;
				state->joy_hat_down &= ~(1u << i);
				state->joy_hat_down |= down << i;
				ret = 1;
				break;
			}
		}
		if ((state->joy_hat_down ^ event->jhat.value) & 0x0f)
			ret = 2; // repeat this event
		break;
	default:
		//printf("joy ev %d\n", event->type);
		return -1;
	}

	if (ret) {
		emu |= get_keystate(state->emu_keys, kc);
		update_keystate(state->keystate, kc, down);
	}
	if (kc_out != NULL)
		*kc_out = kc;
	if (down_out != NULL)
		*down_out = down;
	if (emu_out != 0)
		*emu_out = emu;

	return ret;
}

static int collect_events(struct in_sdl_state *state, int *one_kc, int *one_down)
{
	SDL_Event events[8];
	int count, maxcount, is_emukey = 0;
	int i = 0, ret = 0, retval = 0;
	SDL_Event *event;

	SDL_PumpEvents();

	maxcount = ARRAY_SIZE(events);
	if ((count = SDL_PeepEvents(events, maxcount, SDL_PEEKEVENT, SDL_ALLEVENTS)) > 0) {
		for (i = 0; i < count; ) {
			event = &events[i++];
			ret = handle_input_event(state, event, one_kc, one_down, &is_emukey);
			if (ret < 0) { // unhandled event
				if (event->type == SDL_VIDEORESIZE) {
					state->redraw = 1;
					state->revent = *event;
				} else if (event->type == SDL_VIDEOEXPOSE) {
					if (state->revent.type == SDL_NOEVENT) {
						state->redraw = 1;
						state->revent.type = SDL_VIDEOEXPOSE;
					}
				} else if (ext_event_handler != NULL)
					ext_event_handler(event);
				continue;
			}

			retval |= ret;
			if ((is_emukey || one_kc != NULL) && retval)
			{
				break;
			}
		}
	}
	// remove the handled events
	if (i)
		SDL_PeepEvents(events, i, SDL_GETEVENT, SDL_ALLEVENTS);

	// if the event queue has been emptied and resize/expose events were in it
	if (state->redraw && count == 0) {
		if (ext_event_handler != NULL)
			ext_event_handler(&state->revent);
		state->redraw = 0;
		state->revent.type = SDL_NOEVENT;
		// dummy key event to force returning from the key loop,
		// so the application has a chance to redraw the window
		if (one_kc != NULL) {
			*one_kc = SDLK_UNKNOWN;
			retval |= 1;
		}
		if (one_down != NULL)
			*one_down = 1;
	}
	return retval;
}

static int do_sub_events(struct in_sdl_state *state, int *one_kc, int *one_down)
{
	int is_emukey = 0, ret = 0, retval = 0;
	SDL_Event *event;
	unsigned int i;

	for (i = 0; i < state->ev_count; ) {
		event = &state->ev_queue[i];
		ret = handle_sub_event(state, event, one_kc, one_down, &is_emukey);
		if (ret != 2) // not a repeated event
			i++;
		if (ret < 0) {
			// unhandled - drop
			continue;
		}

		retval |= ret;
		if ((is_emukey || one_kc != NULL) && retval)
		{
			break;
		}
	}

	// remove the handled events
	if (i < state->ev_count)
		memmove(&state->ev_queue[0], &state->ev_queue[i],
			sizeof(state->ev_queue[0]) * (state->ev_count - i));
	state->ev_count -= i;

	return retval;
}

static int in_sdl_update(void *drv_data, const int *binds, int *result)
{
	struct in_sdl_state *state = drv_data;
	keybits_t mask;
	int i, sym, bit, b;

	if (!state->parent)
		collect_events(state, NULL, NULL);
	else
		do_sub_events(state, NULL, NULL);

	if (!binds)
		return 0;
	for (i = 0; i < SDLK_LAST / KEYBITS_WORD_BITS + 1; i++) {
		mask = state->keystate[i];
		if (mask == 0)
			continue;
		for (bit = 0; mask != 0; bit++, mask >>= 1) {
			if ((mask & 1) == 0)
				continue;
			sym = i * KEYBITS_WORD_BITS + bit;

			for (b = 0; b < IN_BINDTYPE_COUNT; b++)
				result[b] |= binds[IN_BIND_OFFS(sym, b)];
		}
	}

	return 0;
}

static int in_sdl_update_kbd(void *drv_data, const int *binds, int *result)
{
	struct in_sdl_state *state = drv_data;
	keybits_t mask;
	int i, sym, bit, b = 0;

	if (!state->parent)
		collect_events(state, NULL, NULL);
	else
		do_sub_events(state, NULL, NULL);

	if (!binds)
		return 0;
	for (i = 0; i < SDLK_LAST / KEYBITS_WORD_BITS + 1; i++) {
		mask = state->keystate[i];
		if (mask == 0)
			continue;
		for (bit = 0; mask != 0; bit++, mask >>= 1) {
			if ((mask & 1) == 0)
				continue;
			sym = i * KEYBITS_WORD_BITS + bit;
			result[b++] = binds[sym];
		}
	}

	return b;
}

static int in_sdl_update_analog(void *drv_data, int axis_id, int *result)
{
	struct in_sdl_state *state = drv_data;
	int v;

	if (!state || !state->joy || !result)
		return -1;
	if ((unsigned)axis_id >= (unsigned)state->joy_numaxes)
		return -1;

	v = SDL_JoystickGetAxis(state->joy, axis_id);

	// -32768...32767 -> -IN_ABS_RANGE...IN_ABS_RANGE
	*result = (v + ((v >> 31) | 1)) / (32768 / IN_ABS_RANGE);
	return 0;
}

static int in_sdl_update_pointer(void *drv_data, int id, int *result)
{
	struct in_sdl_state *state = drv_data, *mouse;
	int max;

	mouse = (state->mouse_state ? state->mouse_state : state);
	state = (mouse->parent ? mouse->parent : state);
	*result = 0;

	switch (id) {
	// absolute position, clipped at the window/screen border
	case 0:	if ((max = state->revent.resize.w))
			*result = mouse->mevent.motion.x * 2*1024/max - 1024;
		break;
	case 1:	if ((max = state->revent.resize.h))
			*result = mouse->mevent.motion.y * 2*1024/max - 1024;
		break;
	// relative mouse movements since last query
	case 2:	if ((max = state->revent.resize.w))
			*result = mouse->mevent.motion.xrel * 2*1024/max;
		mouse->mevent.motion.xrel = 0;
		break;
	case 3:	if ((max = state->revent.resize.h))
			*result = mouse->mevent.motion.yrel * 2*1024/max;
		mouse->mevent.motion.yrel = 0;
		break;
	case 9: *result = mouse->mousewheel;
		mouse->mousewheel = 0;
		break;
	// buttons
	case -1: *result = mouse->mousebtns;
		break;
	default: return -1;
	}

	return 0;
}

static int in_sdl_update_keycode(void *drv_data, int *is_down)
{
	struct in_sdl_state *state = drv_data;
	int ret_kc = -1, ret_down = 0;

	if (!state->parent)
		collect_events(state, &ret_kc, &ret_down);
	else
		do_sub_events(state, &ret_kc, &ret_down);

	if (is_down != NULL)
		*is_down = ret_down;

	return ret_kc;
}

static int in_sdl_menu_translate(void *drv_data, int keycode, char *charcode)
{
	struct in_sdl_state *state = drv_data;
	const struct in_pdata *pdata = state->drv->pdata;
	const char * const * key_names = in_sdl_keys;
	const struct menu_keymap *map;
	int map_len;
	int ret = 0;
	int i;

	if (pdata->key_names)
		key_names = pdata->key_names;

	if (state->joy) {
		map = pdata->joy_map;
		map_len = pdata->jmap_size;
	} else if (state->parent && pdata->mouse_map) {
		/* mouse: separate map, so button names stay correct */
		map = pdata->mouse_map;
		map_len = pdata->mmap_size;
	} else {
		map = pdata->key_map;
		map_len = pdata->kmap_size;
	}

	if (keycode < 0)
	{
		/* menu -> kc */
		keycode = -keycode;
		for (i = 0; i < map_len; i++)
			if (map[i].pbtn == keycode)
				return map[i].key;
	}
	else
	{
		if (keycode == SDLK_UNKNOWN)
			ret = PBTN_RDRAW;
		else
		for (i = 0; i < map_len; i++) {
			if (map[i].key == keycode) {
				ret = map[i].pbtn;
				break;
			}
		}

		if (charcode != NULL && (unsigned int)keycode < SDLK_LAST &&
		    key_names[keycode] != NULL && key_names[keycode][1] == 0)
		{
			ret |= PBTN_CHAR;
			*charcode = key_names[keycode][0];
		}
	}

	return ret;
}

static int in_sdl_clean_binds(void *drv_data, int *binds, int *def_binds)
{
	struct in_sdl_state *state = drv_data;
	int i, t, cnt = 0;

	memset(state->emu_keys, 0, sizeof(state->emu_keys));
	for (t = 0; t < IN_BINDTYPE_COUNT; t++) {
		for (i = 0; i < SDLK_LAST; i++) {
			int offs = IN_BIND_OFFS(i, t);
			if (state->joy && ((i < SDLK_UP || i > SDLK_LEFT) &&
				    (i < SDLK_WORLD_0 || i >= SDLK_WORLD_40)))
				binds[offs] = def_binds[offs] = 0;
			else if (state->parent &&
				    (i < SDLK_WORLD_80 || i > SDLK_WORLD_95))
				binds[offs] = def_binds[offs] = 0;
			else if (!state->parent &&
				    (i >= SDLK_WORLD_0 && i <= SDLK_WORLD_95))
				binds[offs] = def_binds[offs] = 0;
			if (binds[offs]) {
				if (t == IN_BINDTYPE_EMU)
					update_keystate(state->emu_keys, i, 1);
				cnt ++;
			}
		}
	}

	return cnt;
}

static int in_sdl_get_config(void *drv_data, enum in_cfg_opt what, int *val)
{
	struct in_sdl_state *state = drv_data;

	switch (what) {
	case IN_CFG_ABS_AXIS_COUNT:
		*val = state->joy_numaxes;
		break;
	case IN_CFG_ANALOG_MAP_ULDR:
		*val = state->abs_to_udlr;
		break;
	default:
		return -1;
	}

	return 0;
}

static int in_sdl_set_config(void *drv_data, enum in_cfg_opt what, int val)
{
	struct in_sdl_state *state = drv_data;
	size_t i;

	switch (what) {
	case IN_CFG_ANALOG_MAP_ULDR:
		state->abs_to_udlr = val ? 1 : 0;
		for (i = 0; !val && i < state->joy_numaxes; i++) {
			if (state->joy_axis_keydown[i] < 0)
				continue;
			update_keystate(state->keystate, state->joy_axis_keydown[i], 0);
			state->joy_axis_keydown[i] = -1;
		}
		break;
	default:
		return -1;
	}

	return 0;
}

static const in_drv_t in_sdl_drv = {
	.prefix          = IN_SDL_PREFIX,
	.probe           = in_sdl_probe,
	.free            = in_sdl_free,
	.get_key_names   = in_sdl_get_key_names,
	.update          = in_sdl_update,
	.update_kbd      = in_sdl_update_kbd,
	.update_analog   = in_sdl_update_analog,
	.update_pointer  = in_sdl_update_pointer,
	.update_keycode  = in_sdl_update_keycode,
	.menu_translate  = in_sdl_menu_translate,
	.clean_binds     = in_sdl_clean_binds,
	.get_config      = in_sdl_get_config,
	.set_config      = in_sdl_set_config,
};

int in_sdl_init(const struct in_pdata *pdata, void (*handler)(void *event))
{
	if (!pdata) {
		fprintf(stderr, "in_sdl: Missing input platform data\n");
		return -1;
	}

	in_register_driver(&in_sdl_drv, pdata->defbinds, pdata->kbd_map, pdata);
	ext_event_handler = handler;
	return 0;
}
