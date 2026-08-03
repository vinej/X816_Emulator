// X816 Emulator
// VERA2 -- the SDRAM bitmap layer, $9F60-$9F6F.  See X816_core/doc/VERA2.md,
// which is the contract this and the RTL (vera2_regs.sv + vera2_engine.sv)
// both implement.
//
// Copyright (c) 2026 Jean-Yves Vinet.  BSD-2-Clause (same terms as the emulator).

#ifndef VERA2_H
#define VERA2_H

#include <stdint.h>
#include <stdbool.h>

void     vera2_reset(void);
uint8_t  vera2_read(uint8_t reg, bool debugOn);
void     vera2_write(uint8_t reg, uint8_t value);

// scanout side (video.c)
void     vera2_frame_start(void);              // vsync: latch the display base
bool     vera2_active(void);
bool     vera2_passthru(void);
uint32_t vera2_color_at(uint16_t x, uint16_t y);

#endif
