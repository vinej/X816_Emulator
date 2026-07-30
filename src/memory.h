// X816 Emulator -- flat 16 MB memory system
//
// Derived from the Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD
//
// The X16's banked map is gone. X816 is flat: the CPU's 24-bit address
// {bank, offset} indexes one 16 MB array, with a 256-byte I/O page and a
// 256-byte boot ROM overlay carved out of bank $00.
//
// This MUST agree with the RTL core exactly -- see the core's
// doc/MEMORY_MAP.md. A divergence here is worse than having no emulator,
// because software developed against one silently breaks on the other.

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL.h>

#define BANK_SIZE 65536

// ---- X816 flat map ---------------------------------------------------------
#define X816_RAM_SIZE   0x1000000u   // 16 MB, $00:0000-$FF:FFFF
#define X816_IO_PAGE    0x9F00u      // bank $00 only
#define X816_BOOT_BASE  0xFF00u      // bank $00 only
#define X816_BOOT_SIZE  0x100u

// SYSCTL, bank $00
#define X816_SYSCTL         0x9F80u
#define X816_SYSCTL_LAST    0x9F8Fu
#define X816_SYSCTL_OVERLAY 0x01     // bit 0: boot ROM overlay enable (1 at reset)
#define X816_SYSCTL_EMU     0x02     // bit 1: CPU E flag, read-only

// x16emu's debug device. $9F90-$9FFF is open bus in the X816 map, so this
// range is free; kept at the same address so existing tooling still works.
#define DEVICE_EMULATOR (0x9fb0)

#define USE_CURRENT_X16_BANK (-1)
#define debug_read6502(a, b, x) real_read6502((a), (b), true, (x))

// Retained so ieee.c still compiles. X816 has no KERNAL and therefore no
// r0-r15 pseudo-registers; nothing in the X816 build should use these.
#define X16_REG_R0L (direct_page_add(2))
#define X16_REG_R0H (direct_page_add(3))
#define X16_REG_R1L (direct_page_add(4))
#define X16_REG_R1H (direct_page_add(5))
#define X16_REG_R2L (direct_page_add(6))
#define X16_REG_R2H (direct_page_add(7))
#define X16_REG_R3L (direct_page_add(8))
#define X16_REG_R3H (direct_page_add(9))
#define X16_REG_R4L (direct_page_add(10))
#define X16_REG_R4H (direct_page_add(11))
#define X16_REG_R5L (direct_page_add(12))
#define X16_REG_R5H (direct_page_add(13))
#define X16_REG_R6L (direct_page_add(14))
#define X16_REG_R6H (direct_page_add(15))
#define X16_REG_R7L (direct_page_add(16))
#define X16_REG_R7H (direct_page_add(17))
#define X16_REG_R8L (direct_page_add(18))
#define X16_REG_R8H (direct_page_add(19))
#define X16_REG_R9L (direct_page_add(20))
#define X16_REG_R9H (direct_page_add(21))
#define X16_REG_R10L (direct_page_add(22))
#define X16_REG_R10H (direct_page_add(23))
#define X16_REG_R11L (direct_page_add(24))
#define X16_REG_R11H (direct_page_add(25))
#define X16_REG_R12L (direct_page_add(26))
#define X16_REG_R12H (direct_page_add(27))
#define X16_REG_R13L (direct_page_add(28))
#define X16_REG_R13H (direct_page_add(29))
#define X16_REG_R14L (direct_page_add(30))
#define X16_REG_R14H (direct_page_add(31))
#define X16_REG_R15L (direct_page_add(32))
#define X16_REG_R15H (direct_page_add(33))

uint8_t read6502(uint16_t address, uint8_t bank);
uint8_t real_read6502(uint16_t address, uint8_t bank, bool debugOn, int16_t x16Bank);
void write6502(uint16_t address, uint8_t bank, uint8_t value);
void vp6502();

void memory_init();
void memory_reset();
void memory_report_uninitialized_access(bool);
void memory_report_usage_statistics(const char *filename);
void memory_randomize_ram(bool);

void memory_save(SDL_RWops *f, bool dump_ram, bool dump_bank);
void memory_dump_usage_counts();

// ---- X816 image loading ----------------------------------------------------
// Mirrors the core's HPS ioctl path: a file's byte offset is its flat address.
bool memory_load_boot_rom(const char *path);
bool memory_load_flat(const char *path, uint32_t addr);

// ---- Retained for the debugger / disassembler ------------------------------
// X816 has no bank latches. These are stubs so debugger.c, disasm.c, main.c
// and testbench.c keep compiling; the setters do nothing and the getters
// always report 0.
void memory_set_ram_bank(uint8_t bank);
void memory_set_rom_bank(uint8_t bank);
uint8_t memory_get_ram_bank();
uint8_t memory_get_rom_bank();

uint8_t emu_read(uint8_t reg, bool debugOn);
void emu_write(uint8_t reg, uint8_t value);

#endif
