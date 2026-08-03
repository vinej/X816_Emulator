// X816 Emulator
// VERA2 -- the SDRAM bitmap layer, $9F60-$9F6F.  See X816_core/doc/VERA2.md,
// which is the contract; the RTL (vera2_regs.sv + vera2_engine.sv) implements
// the same document independently, so anything that diverges between the two
// is a bug in one of them, not a matter of taste.
//
// Adapted from the X16 emulator's bitmap2.c (same author), and DELIBERATELY
// SMALLER, for the same reason the RTL register block is smaller than
// upstream's: X816's framebuffer is ORDINARY CPU MEMORY at X816_VFB_BASE
// ($E0:0000), so there is no private framebuffer array here and no ADDR/DATA
// port or blitter to model -- programs draw with plain stores, which land in
// RAM[] like every other store, and this module just reads them back at
// scanout.  The registers upstream spent on the data port carry a DISPLAY
// BASE instead (page flipping; upstream always scans offset 0).
//
// Three-valued feature detect, matching the RTL exactly:
//   - registers ALWAYS decode (the RTL block is always instantiated);
//   - ID reads $B5 only when the master switch is on (-vera2 here, the OSD
//     switch on MiSTer), $00 otherwise;
//   - the layer only ever draws when BOTH the switch and CTRL[0] agree.
// So "absent" and "present but disabled" are distinguishable, and a core or
// emulator with the switch off is bit-identical to stock apart from the
// registers reading as themselves.
//
// Copyright (c) 2026 Jean-Yves Vinet.  BSD-2-Clause (same terms as the emulator).

#include "vera2.h"
#include "glue.h"                 // RAM, has_vera2
#include "x816_contract.h"

#define VFB_MASK (X816_VFB_SIZE - 1u)   // 1 MB, power of two

static bool     v2_enable;
static uint8_t  v2_mode;          // 1 = 640x480 8bpp, 2 = 640x480 4bpp
static bool     v2_passthru;      // CTRL[3]: VERA opaque pixels over the bitmap
static uint32_t v2_disp;          // DISPBASE as written (20-bit, bit 0 forced 0)
static uint32_t v2_disp_latched;  // ...and as latched at the last vsync
static uint8_t  v2_pal_lo;        // latched {G,B} between PALLO and PALHI
static uint8_t  v2_pal_cursor;    // palette write cursor
static uint32_t v2_pal_rgb[256];  // precomputed 0x00RRGGBB (framebuffer format)

void
vera2_reset(void)
{
	v2_enable = false;
	v2_mode = 0;
	v2_passthru = false;
	v2_disp = 0;
	v2_disp_latched = 0;
	v2_pal_lo = 0;
	v2_pal_cursor = 0;
	for (int i = 0; i < 256; i++) {
		v2_pal_rgb[i] = 0;
	}
	// The framebuffer is RAM[] and is NOT cleared here: it is ordinary memory,
	// and a reset no more blanks it than it blanks the heap.  The RTL is the
	// same -- SDRAM keeps its contents across a CPU reset.
}

uint8_t
vera2_read(uint8_t reg, bool debugOn)
{
	(void)debugOn;                // no register here has read side effects
	switch (reg & 0xf) {
		case 0x0: // CTRL
			return (uint8_t)((v2_passthru ? 8 : 0) | (v2_mode << 1) | (v2_enable ? 1 : 0));
		case 0x1: // ID: $B5 only with the master switch on -- doc/VERA2.md 3.1
			return has_vera2 ? X816_VERA2_ID_VALUE : 0x00;
		case 0x2: return (uint8_t)( v2_disp        & 0xff);
		case 0x3: return (uint8_t)((v2_disp >>  8) & 0xff);
		case 0x4: return (uint8_t)((v2_disp >> 16) & 0x0f);
		default:  return 0x00;    // $9F65, $9F69-$9F6F: reserved
	}
}

void
vera2_write(uint8_t reg, uint8_t value)
{
	switch (reg & 0xf) {
		case 0x0: // CTRL
			v2_enable   = value & 1;
			v2_mode     = (value >> 1) & 3;
			v2_passthru = (value >> 3) & 1;
			break;
		// DISPBASE. Bit 0 is forced even -- the RTL fetch works in 16-bit
		// words (vera2_regs.sv does {di[7:1],1'b0}).
		case 0x2: v2_disp = (v2_disp & 0xFFF00u) |  (uint32_t)(value & 0xFE);       break;
		case 0x3: v2_disp = (v2_disp & 0xF00FFu) | ((uint32_t)value << 8);          break;
		case 0x4: v2_disp = (v2_disp & 0x0FFFFu) | ((uint32_t)(value & 0xF) << 16); break;
		case 0x6: v2_pal_cursor = value; break;
		case 0x7: v2_pal_lo = value;     break;
		case 0x8: { // PALHI commits {R,G,B} and steps the cursor
			uint32_t r = value    & 0xf; r = (r << 4) | r;   // 4-bit -> 8-bit
			uint32_t g = v2_pal_lo >> 4; g = (g << 4) | g;
			uint32_t b = v2_pal_lo & 0xf; b = (b << 4) | b;
			v2_pal_rgb[v2_pal_cursor] = (r << 16) | (g << 8) | b;
			v2_pal_cursor++;
			break;
		}
		default: break;           // ID read-only; $9F65, $9F69-$9F6F reserved
	}
}

// vsync. The RTL latches the display base at vs_rise so a mid-frame flip
// cannot tear (doc/VERA2.md 3.2); video.c calls this at frame start for the
// same reason. (The RTL's nanosecond-window torn-latch caveat has no emulator
// equivalent -- register writes and frame boundaries never interleave here.)
void
vera2_frame_start(void)
{
	v2_disp_latched = v2_disp;
}

bool
vera2_active(void)
{
	// Both the master switch and software must agree -- doc/VERA2.md 1.
	return has_vera2 && v2_enable && (v2_mode == 1 || v2_mode == 2);
}

bool
vera2_passthru(void)
{
	return v2_passthru;
}

uint32_t
vera2_color_at(uint16_t x, uint16_t y)
{
	// STRAIGHT OUT OF RAM[]. This is the whole model: the program stored
	// pixels with ordinary writes, they went where every store goes, and the
	// scanout reads them back from there. Offsets wrap mod 1 MB inside the
	// window, matching the RTL's 20-bit pointer arithmetic.
	uint8_t idx;
	if (v2_mode == 1) {                            // 8bpp: byte k = pixel k
		uint32_t off = (v2_disp_latched + (uint32_t)y * 640u + x) & VFB_MASK;
		idx = RAM[X816_VFB_BASE + off];
	} else {                                       // 4bpp: high nibble = left
		uint32_t off = (v2_disp_latched + (uint32_t)y * 320u + (x >> 1)) & VFB_MASK;
		uint8_t byte = RAM[X816_VFB_BASE + off];
		idx = (x & 1) ? (byte & 0x0f) : (byte >> 4);
	}
	return v2_pal_rgb[idx];
}
