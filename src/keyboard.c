#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "glue.h"
#include "i2c.h"
#include "keyboard.h"

#define EXTENDED_FLAG 0x100
// #define ESC_IS_BREAK /* if enabled, Esc sends Break/Pause key instead of Esc */

int
keynum_from_SDL_Scancode(SDL_Scancode scancode)
{
	switch (scancode) {
		case SDL_SCANCODE_GRAVE:
			return 1;
		case SDL_SCANCODE_BACKSPACE:
			return 15;
		case SDL_SCANCODE_TAB:
			return 16;
		case SDL_SCANCODE_CLEAR:
			return 0;
		case SDL_SCANCODE_RETURN:
			return 43;
		case SDL_SCANCODE_PAUSE:
			return 126;
		case SDL_SCANCODE_ESCAPE:
#ifdef ESC_IS_BREAK
			return 126;
#else
			return 110;
#endif
		case SDL_SCANCODE_SPACE:
			return 61;
		case SDL_SCANCODE_APOSTROPHE:
			return 41;
		case SDL_SCANCODE_COMMA:
			return 53;
		case SDL_SCANCODE_MINUS:
			return 12;
		case SDL_SCANCODE_PERIOD:
			return 54;
		case SDL_SCANCODE_SLASH:
			return 55;
		case SDL_SCANCODE_0:
			return 11;
		case SDL_SCANCODE_1:
			return 2;
		case SDL_SCANCODE_2:
			return 3;
		case SDL_SCANCODE_3:
			return 4;
		case SDL_SCANCODE_4:
			return 5;
		case SDL_SCANCODE_5:
			return 6;
		case SDL_SCANCODE_6:
			return 7;
		case SDL_SCANCODE_7:
			return 8;
		case SDL_SCANCODE_8:
			return 9;
		case SDL_SCANCODE_9:
			return 10;
		case SDL_SCANCODE_SEMICOLON:
			return 40;
		case SDL_SCANCODE_EQUALS:
			return 13;
		case SDL_SCANCODE_LEFTBRACKET:
			return 27;
		case SDL_SCANCODE_BACKSLASH:
			return 29;
		case SDL_SCANCODE_RIGHTBRACKET:
			return 28;
		case SDL_SCANCODE_A:
			return 31;
		case SDL_SCANCODE_B:
			return 50;
		case SDL_SCANCODE_C:
			return 48;
		case SDL_SCANCODE_D:
			return 33;
		case SDL_SCANCODE_E:
			return 19;
		case SDL_SCANCODE_F:
			return 34;
		case SDL_SCANCODE_G:
			return 35;
		case SDL_SCANCODE_H:
			return 36;
		case SDL_SCANCODE_I:
			return 24;
		case SDL_SCANCODE_J:
			return 37;
		case SDL_SCANCODE_K:
			return 38;
		case SDL_SCANCODE_L:
			return 39;
		case SDL_SCANCODE_M:
			return 52;
		case SDL_SCANCODE_N:
			return 51;
		case SDL_SCANCODE_O:
			return 25;
		case SDL_SCANCODE_P:
			return 26;
		case SDL_SCANCODE_Q:
			return 17;
		case SDL_SCANCODE_R:
			return 20;
		case SDL_SCANCODE_S:
			return 32;
		case SDL_SCANCODE_T:
			return 21;
		case SDL_SCANCODE_U:
			return 23;
		case SDL_SCANCODE_V:
			return 49;
		case SDL_SCANCODE_W:
			return 18;
		case SDL_SCANCODE_X:
			return 47;
		case SDL_SCANCODE_Y:
			return 22;
		case SDL_SCANCODE_Z:
			return 46;
		case SDL_SCANCODE_DELETE:
			return 76;
		case SDL_SCANCODE_UP:
			return 83;
		case SDL_SCANCODE_DOWN:
			return 84;
		case SDL_SCANCODE_RIGHT:
			return 89;
		case SDL_SCANCODE_LEFT:
			return 79;
		case SDL_SCANCODE_INSERT:
			return 75;
		case SDL_SCANCODE_HOME:
			return 80;
		case SDL_SCANCODE_END:
			return 81;
		case SDL_SCANCODE_PAGEUP:
			return 85;
		case SDL_SCANCODE_PAGEDOWN:
			return 86;
		case SDL_SCANCODE_F1:
			return 112;
		case SDL_SCANCODE_F2:
			return 113;
		case SDL_SCANCODE_F3:
			return 114;
		case SDL_SCANCODE_F4:
			return 115;
		case SDL_SCANCODE_F5:
			return 116;
		case SDL_SCANCODE_F6:
			return 117;
		case SDL_SCANCODE_F7:
			return 118;
		case SDL_SCANCODE_F8:
			return 119;
		case SDL_SCANCODE_F9:
			return 120;
		case SDL_SCANCODE_F10:
			return 121;
		case SDL_SCANCODE_F11:
			return 122;
		case SDL_SCANCODE_F12:
			return 123;
		case SDL_SCANCODE_SCROLLLOCK:
			return 125;
		case SDL_SCANCODE_RSHIFT:
			return 57;
		case SDL_SCANCODE_LSHIFT:
			return 44;
		case SDL_SCANCODE_CAPSLOCK:
			return 30;
		case SDL_SCANCODE_LCTRL:
			return 58;
		case SDL_SCANCODE_RCTRL:
			return 64;
		case SDL_SCANCODE_LALT:
			return 60;
		case SDL_SCANCODE_RALT:
			return 62;
		case SDL_SCANCODE_LGUI:
			return 59;
		case SDL_SCANCODE_RGUI:
			return 63;
		case SDL_SCANCODE_APPLICATION: // Menu
			return 65;
		case SDL_SCANCODE_NONUSBACKSLASH:
			return 45;
		case SDL_SCANCODE_KP_ENTER:
			return 108;
		case SDL_SCANCODE_KP_0:
			return 99;
		case SDL_SCANCODE_KP_1:
			return 93;
		case SDL_SCANCODE_KP_2:
			return 98;
		case SDL_SCANCODE_KP_3:
			return 103;
		case SDL_SCANCODE_KP_4:
			return 92;
		case SDL_SCANCODE_KP_5:
			return 97;
		case SDL_SCANCODE_KP_6:
			return 102;
		case SDL_SCANCODE_KP_7:
			return 91;
		case SDL_SCANCODE_KP_8:
			return 96;
		case SDL_SCANCODE_KP_9:
			return 101;
		case SDL_SCANCODE_KP_PERIOD:
			return 104;
		case SDL_SCANCODE_KP_PLUS:
			return 106;
		case SDL_SCANCODE_KP_MINUS:
			return 105;
		case SDL_SCANCODE_KP_MULTIPLY:
			return 100;
		case SDL_SCANCODE_KP_DIVIDE:
			return 95;
		case SDL_SCANCODE_NUMLOCKCLEAR:
			return 90;
		case SDL_SCANCODE_INTERNATIONAL1:
			return 56;
		default:
			return 0;
	}
}

void
handle_keyboard(bool down, SDL_Keycode sym, SDL_Scancode scancode)
{
	int keynum = keynum_from_SDL_Scancode(scancode);
	
	if (keynum == 0) return;

	if (down) {
		if (log_keyboard) {
			printf("DOWN 0x%02X\n", scancode);
			fflush(stdout);
		}

		if (keynum & EXTENDED_FLAG) {
			i2c_kbd_buffer_add(0x7f);
		}
		i2c_kbd_buffer_add(keynum & 0xff);
	} else {
		if (log_keyboard) {
			printf("UP   0x%02X\n", scancode);
			fflush(stdout);
		}

		keynum = keynum | 0b10000000;
		if (keynum & EXTENDED_FLAG) {
			i2c_kbd_buffer_add(0xff);
		}
		i2c_kbd_buffer_add(keynum & 0xff);
	}
}
/* ===========================================================================
 * -autokeys: type a string into the SMC key FIFO, headlessly.
 *
 * See keyboard.h for why this exists. The keycodes are IBM System/2 positions,
 * the same numbering keynum_from_SDL_Scancode() produces above and the same one
 * the X816 console's keymap decodes -- so this drives the real path rather than
 * a parallel one.
 * =========================================================================== */

/* keycode -> ASCII, unshifted. Index is the keycode; 0 means "no plain ASCII".
   Deliberately identical to X816_Calypsi runtime/font8x8.c keymap[64], because
   a test that used a DIFFERENT table would prove the table rather than the
   path. */
/* These are IBM KEY POSITION NUMBERS, matching keynum_from_SDL_Scancode
   above -- position 1 is the grave key, not ESC. Both tables run to 128
   because the keys worth typing in a test that are NOT on the typewriter
   block live above 63: ESCAPE is 110. Sized at 64 they could not express it,
   and putting ESC at index 1 to work around that made the grave key send an
   escape instead. */
static const unsigned char autokey_ascii[128] = {
	0,    '`',  '1',  '2',  '3',  '4',  '5',  '6',
	'7',  '8',  '9',  '0',  '-',  '=',  0,    0x08,
	0x09, 'q',  'w',  'e',  'r',  't',  'y',  'u',
	'i',  'o',  'p',  '[',  ']',  92,   0,    'a',
	's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',
	';',  39,   0,    0x0D, 0,    0,    'z',  'x',
	'c',  'v',  'b',  'n',  'm',  ',',  '.',  '/',
	0,    0,    0,    0,    0,    ' ',  0,    0,
	/* ---- 64..127: not on the typewriter block ---------------------- */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  64 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  72 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  80 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  88 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  96 */
	0,    0,    0,    0,    0,    0,    0x1B, 0,    /* 104; 110 = ESCAPE */
	0,    0,    0,    0,    0,    0,    0,    0,    /* 112 */
	0,    0,    0,    0,    0,    0,    0,    0     /* 120 */
};

/* Shifted, US layout -- the same table X816_Calypsi runtime/font8x8.c decodes
   with. A character reachable only with Shift has to be typed WITH Shift, or
   the shift path is never exercised and ':' cannot be sent at all. */
static const unsigned char autokey_shift[128] = {
	0,    '~',  '!',  '@',  '#',  '$',  '%',  '^',
	'&',  '*',  '(',  ')',  '_',  '+',  0,    0x08,
	0x09, 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',
	'I',  'O',  'P',  '{',  '}',  '|',  0,    'A',
	'S',  'D',  'F',  'G',  'H',  'J',  'K',  'L',
	':',  '"',  0,    0x0D, 0,    0,    'Z',  'X',
	'C',  'V',  'B',  'N',  'M',  '<',  '>',  '?',
	0,    0,    0,    0,    0,    ' ',  0,    0,
	/* ---- 64..127: not on the typewriter block ---------------------- */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  64 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  72 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  80 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  88 */
	0,    0,    0,    0,    0,    0,    0,    0,    /*  96 */
	0,    0,    0,    0,    0,    0,    0x1B, 0,    /* 104; 110 = ESCAPE */
	0,    0,    0,    0,    0,    0,    0,    0,    /* 112 */
	0,    0,    0,    0,    0,    0,    0,    0     /* 120 */
};

#define AUTOKEY_LSHIFT 44

static const char *autokeys_text = NULL;
static uint32_t    autokeys_timer = 0;
static uint8_t     autokeys_pending = 0;
static bool        autokeys_shifted = false;
/* 0 = fetch the next character, then 1..4 walk the make/break sequence. A
   shifted character is four events, not two, and the shift key has to bracket
   the other one for the guest to see them in the right order. */
static uint8_t     autokeys_phase = 0;

/* Roughly 25 ms of an 8 MHz clock between events. Fast enough that a test
   finishes in seconds, slow enough that a program polling the SMC in a loop
   cannot miss one -- a real key is held far longer than this. */
#define AUTOKEYS_INTERVAL 200000

void
autokeys_set(const char *text)
{
	autokeys_text    = text;
	autokeys_timer   = AUTOKEYS_INTERVAL * 20;   /* let the program start up */
	autokeys_phase   = 0;
	autokeys_shifted = false;
}

void
autokeys_step(uint32_t clocks)
{
	unsigned char c;
	int i, code;

	if (!autokeys_text)
		return;
	if (autokeys_timer > clocks) {
		autokeys_timer -= clocks;
		return;
	}
	autokeys_timer = AUTOKEYS_INTERVAL;

	/* Every press is followed by its release, exactly as a real keyboard does.
	   Skipping the release would leave code that filters bit 7 untested, which
	   is precisely the kind of gap that put us here -- and it would leave Shift
	   stuck down for ever, since a modifier IS the gap between its two edges. */
	switch (autokeys_phase) {
	case 1:                                 /* key down */
		i2c_kbd_buffer_add(autokeys_pending);
		autokeys_phase = 2;
		return;
	case 2:                                 /* key up */
		i2c_kbd_buffer_add(autokeys_pending | 0x80);
		autokeys_phase = autokeys_shifted ? 3 : 0;
		return;
	case 3:                                 /* shift up, last */
		i2c_kbd_buffer_add(AUTOKEY_LSHIFT | 0x80);
		autokeys_phase = 0;
		return;
	default:
		break;
	}

	if (!*autokeys_text) {
		autokeys_text = NULL;                     /* done */
		return;
	}

	c = (unsigned char)*autokeys_text++;
	if (c == '\\' && *autokeys_text == 'n') {    /* literal backslash-n */
		autokeys_text++;
		c = 0x0D;
	} else if (c == '\n') {
		c = 0x0D;
	}
	/* No folding: the unshifted table is lower case now, so an upper-case
	   character is found on the SHIFTED layer and typed with Shift held --
	   which is what a person does, and what exercises the modifier. */

	code = -1;
	autokeys_shifted = false;
	for (i = 0; i < 128; i++) {
		if (autokey_ascii[i] == c) {
			code = i;
			break;
		}
	}
	if (code < 0) {
		/* Not on the unshifted layer -- try the shifted one. */
		for (i = 0; i < 128; i++) {
			if (autokey_shift[i] == c) {
				code = i;
				autokeys_shifted = true;
				break;
			}
		}
	}
	if (code < 0)
		return;                                   /* not typeable: skip it */

	autokeys_pending = (uint8_t)code;
	if (autokeys_shifted) {
		i2c_kbd_buffer_add(AUTOKEY_LSHIFT);       /* shift down first */
		autokeys_phase = 1;
	} else {
		i2c_kbd_buffer_add(autokeys_pending);
		autokeys_phase = 2;
	}
}
