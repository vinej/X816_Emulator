// X816 Emulator -- flat 16 MB memory system
//
// Derived from the Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD
//
// The X16's banked map is replaced wholesale. There are no $0000/$0001 bank
// latches, no $A000-$BFFF window and no banked ROM: the CPU's 24-bit address
// {bank, offset} indexes one flat 16 MB array.
//
//     $00:0000-$00:9EFF   RAM
//     $00:9F00-$00:9FFF   I/O
//     $00:A000-$00:FEFF   RAM
//     $00:FF00-$00:FFFF   boot ROM overlay for READS while SYSCTL[0]=1,
//                         RAM underneath for writes and once the bit is cleared
//     $01:0000-$FF:FFFF   RAM
//
// This must agree with the RTL core exactly -- see the core's
// doc/MEMORY_MAP.md, which is the authority.

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include "glue.h"
#include "via.h"
#include "memory.h"
#include "sdblock.h"
#include "vera2.h"
#include "video.h"
#include "ymglue.h"
#include "cpu/fake6502.h"
#include "wav_recorder.h"
#include "audio.h"
#include "iso_8859_15.h"

// Retained only so the X16 leftovers still link; unused by the flat map.
uint8_t ram_bank;
uint8_t rom_bank;
uint8_t *BRAM;
uint8_t ROM[ROM_SIZE];

uint8_t *RAM;                       // the flat 16 MB
static uint8_t boot_rom[X816_BOOT_SIZE];
static bool    boot_rom_loaded = false;

// SYSCTL bit 0. Powers up SET, so the boot ROM shadows $00:FF00-$00:FFFF for
// reads until software clears it -- see boot/boot.s in the core repo.
static bool sysctl_overlay = true;

// The $9F90 millisecond timer's state. Declared up here so memory_reset can
// clear it: in the RTL it is reset by cpu_reset_n like everything else, and a
// clock that survived reset in one implementation but not the other would be
// exactly the kind of quiet divergence this file's header warns about.
static uint32_t timer_ms    = 0;
static uint32_t timer_div   = 0;
static uint32_t timer_latch = 0;

// Floating-bus emulation. The RTL keeps the last byte transferred on the bus
// and returns it for unmapped reads:
//     always @(posedge cpu_clk) if (cpu_rdy) open_bus_r <= cpu_rwn ? cpu_di : cpu_do;
// Returning $00 instead would make device-probing code false-positive on
// "something present answering 0", so this is modelled rather than faked.
static uint8_t open_bus = 0;

static uint8_t addr_ym = 0;

bool randomizeRAM = false;
bool reportUninitializedAccess = false;
const char *reportUsageStatisticsFilename = NULL;

static uint32_t clock_snap = 0UL;
static uint32_t clock_base = 0UL;

void
memory_init()
{
	RAM = calloc(X816_RAM_SIZE, sizeof(uint8_t));
	if (!RAM) {
		fprintf(stderr, "X816: cannot allocate %u bytes of guest RAM\n", X816_RAM_SIZE);
		exit(1);
	}

	// X816 has no banked RAM. BRAM is allocated as inert scratch only because
	// two X16 leftovers still index it -- the debugger's bank editor
	// (debugger.c) and BASIC paste poking the KERNAL keyboard buffer
	// (main.c). Neither is reachable on X816; this keeps them from
	// dereferencing NULL until they are stripped out.
	BRAM = calloc(BANK_SIZE, sizeof(uint8_t));

	// Bank 0 is M10K on hardware and comes up zeroed by FPGA configuration, so
	// it is deterministic. Banks $01+ are SDRAM and come up as noise; randomize
	// them under -randram so software that relies on zeroed SDRAM fails here
	// rather than on the board.
	if (randomizeRAM) {
		time_t t;
		srand((unsigned)time(&t));
		for (uint32_t i = 0x10000; i < X816_RAM_SIZE; i++) {
			RAM[i] = rand();
		}
	}

	memory_reset();
}

void
memory_reset()
{
	sysctl_overlay = true;
	open_bus = 0;
	timer_ms = timer_div = timer_latch = 0;
}

bool
memory_load_boot_rom(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "X816: cannot open boot ROM '%s'\n", path);
		return false;
	}
	size_t n = fread(boot_rom, 1, X816_BOOT_SIZE, f);
	fclose(f);
	if (n != X816_BOOT_SIZE) {
		fprintf(stderr, "X816: boot ROM '%s' is %zu bytes, expected %u\n",
		        path, n, X816_BOOT_SIZE);
		return false;
	}
	boot_rom_loaded = true;
	return true;
}

bool
memory_load_flat(const char *path, uint32_t addr)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "X816: cannot open image '%s'\n", path);
		return false;
	}
	if (addr >= X816_RAM_SIZE) {
		fprintf(stderr, "X816: load address $%06X is outside the 16 MB space\n", addr);
		fclose(f);
		return false;
	}
	size_t n = fread(&RAM[addr], 1, X816_RAM_SIZE - addr, f);
	fclose(f);
	printf("X816: loaded %zu bytes at $%06X\n", n, addr);
	return true;
}

void memory_report_uninitialized_access(bool value) { reportUninitializedAccess = value; }
void memory_report_usage_statistics(const char *filename) { reportUsageStatisticsFilename = filename; }
void memory_randomize_ram(bool value) { randomizeRAM = value; }

// ---------------------------------------------------------------------------
// Free-running millisecond timer, $9F90-$9F93 (little-endian).
//
// x816.sv counts cpu_clk cycles and divides by X816_TIMER_DIV, gated by
// nothing -- not cpu_rdy, not a chip select -- because both VIAs stop dead
// during an SD transfer (doc/AUDIT.md L-4) and VERA's VSYNC latch collapses a
// multi-frame freeze into one tick. This mirrors that: the divider runs off
// the same CPU-clock delta the rest of the main loop is driven by.
//
// The LATCH is normative, not an optimisation. Reading the low byte at $9F90
// captures bits 31:8, and $9F91-$9F93 return that capture -- so software must
// read $9F90 FIRST. Without it a read straddling a carry returns a value that
// was never true and can go backwards. The emulator implements it because
// software that gets the order wrong must break HERE, where it is cheap to
// find, and not only on hardware.
// ---------------------------------------------------------------------------
void
timer_step(uint32_t clocks)
{
	timer_div += clocks;
	while (timer_div >= X816_TIMER_DIV) {
		timer_div -= X816_TIMER_DIV;
		timer_ms++;
	}
}

static uint8_t
timer_read(uint8_t reg, bool debugOn)
{
	switch (reg & 3) {
		case 0:
			// The debugger's memory view must not disturb the machine it is
			// looking at -- same rule the SD block device follows.
			if (!debugOn) timer_latch = timer_ms >> 8;
			return timer_ms & 0xff;
		case 1: return timer_latch & 0xff;
		case 2: return (timer_latch >> 8) & 0xff;
		default: return (timer_latch >> 16) & 0xff;
	}
}

// ---------------------------------------------------------------------------
// I/O page, bank $00 only. Layout is deliberately identical to the Commander
// X16's so VERA/VIA/YM register offsets and drivers port over unchanged.
// ---------------------------------------------------------------------------
static uint8_t
io_read(uint16_t address, bool debugOn)
{
	if (address < 0x9f10) {                       // $9F00-$9F0F  VIA #1
		return via1_read(address & 0xf, debugOn);
	} else if (address < 0x9f20) {                // $9F10-$9F1F  VIA #2
		return via2_read(address & 0xf, debugOn);
	} else if (address < 0x9f40) {                // $9F20-$9F3F  VERA
		return video_read(address & 0x1f, debugOn);
	} else if (address < 0x9f50) {                // $9F40-$9F4F  YM2151
		if (!debugOn) audio_render();
		return YM_read_status();
	} else if (address >= 0x9f60 && address < 0x9f70) {
		// $9F60-$9F6F  VERA2 (doc/VERA2.md). Decodes UNCONDITIONALLY, like the
		// RTL: the -vera2 switch gates only the ID value and activation.
		return vera2_read(address & 0xf, debugOn);
	} else if (address >= X816_SYSCTL && address <= X816_SYSCTL_LAST) {
		if ((address & 0xf) == 0) {               // $9F80  SYSCTL
			return (sysctl_overlay ? X816_SYSCTL_OVERLAY : 0)
			     | (regs.e ? X816_SYSCTL_EMU : 0);
		}
		return sdblock_read(address & 0xf, debugOn);  // $9F81-$9F8B  SD
	} else if (address >= X816_TIMER && address <= X816_TIMER_LAST) {
		return timer_read(address & 3, debugOn);      // $9F90-$9F93  ms counter
	} else if (address >= DEVICE_EMULATOR && address < DEVICE_EMULATOR + 0x10) {
		// EMULATOR-ONLY. The RTL treats $9F90-$9FFF as open bus, so this device
		// does not exist on hardware. Guest software must not depend on it.
		return emu_read(address & 0xf, debugOn);
	}
	return open_bus;                              // unmapped: floating bus
}

static void
io_write(uint16_t address, uint8_t value)
{
	if (address < 0x9f10) {                       // VIA #1
		via1_write(address & 0xf, value);
	} else if (address < 0x9f20) {                // VIA #2
		via2_write(address & 0xf, value);
	} else if (address < 0x9f40) {                // VERA
		video_write(address & 0x1f, value);
	} else if (address < 0x9f50) {                // YM2151: A0 selects reg/data
		if ((address & 1) == 0) {
			addr_ym = value;
		} else {
			audio_render();
			YM_write_reg(addr_ym, value);
		}
	} else if (address >= 0x9f60 && address < 0x9f70) {
		vera2_write(address & 0xf, value);        // $9F60-$9F6F  VERA2
	} else if (address >= X816_SYSCTL && address <= X816_SYSCTL_LAST) {
		if ((address & 0xf) == 0) {
			sysctl_overlay = (value & X816_SYSCTL_OVERLAY) != 0;
		} else {
			sdblock_write(address & 0xf, value);     // $9F81-$9F8B  SD
		}
	} else if (address >= DEVICE_EMULATOR && address < DEVICE_EMULATOR + 0x10) {
		emu_write(address & 0xf, value);          // emulator-only, see io_read
	}
	// everything else in the I/O page is unmapped; writes are discarded
}

// ---------------------------------------------------------------------------
// Bus
// ---------------------------------------------------------------------------
uint8_t
real_read6502(uint16_t address, uint8_t bank, bool debugOn, int16_t x16Bank)
{
	(void)x16Bank;                                // no bank latches on X816

	if (bank != 0) {                              // $01:0000-$FF:FFFF
		return RAM[((uint32_t)bank << 16) | address];
	}

	if (address < X816_IO_PAGE) {                 // $0000-$9EFF
		return RAM[address];
	}
	if (address < 0xa000) {                       // $9F00-$9FFF
		return io_read(address, debugOn);
	}
	if (address >= X816_BOOT_BASE && sysctl_overlay) {
		// The overlay shadows RAM for READS only; the RAM underneath is what
		// writes reach, which is what lets the boot stub copy itself down and
		// then unmap itself without disturbing the instruction stream.
		return boot_rom[address - X816_BOOT_BASE];
	}
	return RAM[address];                          // $A000-$FEFF, and $FF00+ once unmapped
}

uint8_t
read6502(uint16_t address, uint8_t bank)
{
	uint8_t v = real_read6502(address, bank, false, USE_CURRENT_X16_BANK);
	open_bus = v;
	return v;
}

void
write6502(uint16_t address, uint8_t bank, uint8_t value)
{
	open_bus = value;

	if (bank != 0) {                              // $01:0000-$FF:FFFF
		// Firmware write-protect (core doc/KERNEL.md section 3; x816.sv
		// "fw_region"): banks $F0-$FF hold the HPS-loaded kernel and CPU
		// stores there are SILENTLY DROPPED -- on hardware the region gets
		// no chip select for a store, so the write vanishes with no error.
		// Reads are unrestricted. The loader paths bypass this by
		// construction, exactly as the HPS/SD-DMA ports do in the RTL:
		// -load (memory_load_flat) and the SD DMA (sdblock.c) write RAM[]
		// directly and never come through here.
		if (bank >= X816_FW_FIRST_BANK) {
			return;
		}
		RAM[((uint32_t)bank << 16) | address] = value;
		return;
	}

	if (address < X816_IO_PAGE) {                 // $0000-$9EFF
		RAM[address] = value;
		return;
	}
	if (address < 0xa000) {                       // $9F00-$9FFF
		io_write(address, value);
		return;
	}
	// $A000-$FFFF, including the boot page: writes ALWAYS reach RAM, even
	// while the overlay is mapped for reads.
	RAM[address] = value;
}

void
vp6502()
{
	// On the X16 a vector pull forced the ROM bank latch to 0. X816 has no
	// bank latch and its vectors are ordinary bank-$00 reads, so this is a
	// deliberate no-op -- see the core's p65c816_flat_wrap.vhd, which drops
	// the VPB output for the same reason.
}

void
memory_save(SDL_RWops *f, bool dump_ram, bool dump_bank)
{
	(void)dump_bank;                              // no banked RAM on X816
	if (dump_ram) {
		SDL_RWwrite(f, &RAM[0], sizeof(uint8_t), X816_RAM_SIZE);
	}
}

void
memory_dump_usage_counts()
{
	if (reportUsageStatisticsFilename == NULL) return;
	printf("X816: per-address usage statistics are not implemented for the flat "
	       "16 MB map (the X16 version indexed by bank latch).\n");
}

// ---- Bank-latch stubs ------------------------------------------------------
// Kept so debugger.c, disasm.c, main.c and testbench.c keep compiling.
void memory_set_ram_bank(uint8_t bank) { (void)bank; }
void memory_set_rom_bank(uint8_t bank) { (void)bank; }
uint8_t memory_get_ram_bank() { return 0; }
uint8_t memory_get_rom_bank() { return 0; }

// Control the GIF recorder
void
emu_recorder_set(gif_recorder_command_t command)
{
	if (command == RECORD_GIF_PAUSE && record_gif != RECORD_GIF_DISABLED) {
		record_gif = RECORD_GIF_PAUSED;
	}
	if (command == RECORD_GIF_RESUME && record_gif != RECORD_GIF_DISABLED) {
		record_gif = RECORD_GIF_ACTIVE;
	}
	if (command == RECORD_GIF_SNAP && record_gif != RECORD_GIF_DISABLED) {
		record_gif = RECORD_GIF_SINGLE;
	}
}

//
// read/write emulator state (feature flags)
//
// 0: debugger_enabled            8: write: reset cpu clock counter
// 1: log_video                   8: read: snapshot cpu clock, bits 0-7
// 2: log_keyboard                9: write: debug byte 1 / read: clock 8-15
// 3: echo_mode                  10: write: debug byte 2 / read: clock 16-23
// 4: save_on_exit               11: write: char to STDOUT / read: clock 24-31
// 5: record_gif
// 6: record_wav
// 7: cmd key toggle           12: write: EXIT the emulator, value = status
//                                 (test harnesses; open bus on hardware)
void
emu_write(uint8_t reg, uint8_t value)
{
	bool v = value != 0;
	switch (reg) {
		case 0: debugger_enabled = v; break;
		case 1: log_video = v; break;
		case 2: log_keyboard = v; break;
		case 3: echo_mode = value; break;
		case 4: save_on_exit = v; break;
		case 5: emu_recorder_set((gif_recorder_command_t) value); break;
		case 6: wav_recorder_set((wav_recorder_command_t) value); break;
		case 7: disable_emu_cmd_keys = v; break;
		case 8: clock_base = clockticks6502; break;
		case 9: printf("User debug 1: $%02x\n", value); fflush(stdout); break;
		case 10: printf("User debug 2: $%02x\n", value); fflush(stdout); break;
		case 11: {
			if (value == 0x09 || value == 0x0a || value == 0x0d || (value >= 0x20 && value < 0x7f)) {
				printf("%c", value);
			} else if (value >= 0xa1) {
				print_iso8859_15_char((char) value);
			} else {
				printf("\xef\xbf\xbd");
			}
			fflush(stdout);
			break;
		}
		case 12:
			// Guest-requested exit, for test harnesses: a finished suite
			// should not have to wait out the host's timeout. On hardware
			// this address is open bus and the write does nothing.
			printf("Guest exit via $9FBC, status %d.\n", value);
			main_shutdown();
			exit(value);
			break;
		default: printf("WARN: Invalid register %x\n", DEVICE_EMULATOR + reg);
	}
}

uint8_t
emu_read(uint8_t reg, bool debugOn)
{
	if (reg == 0) {
		return debugger_enabled ? 1 : 0;
	} else if (reg == 1) {
		return log_video ? 1 : 0;
	} else if (reg == 2) {
		return log_keyboard ? 1 : 0;
	} else if (reg == 3) {
		return echo_mode;
	} else if (reg == 4) {
		return save_on_exit ? 1 : 0;
	} else if (reg == 5) {
		return record_gif;
	} else if (reg == 6) {
		return wav_recorder_get_state();
	} else if (reg == 7) {
		return disable_emu_cmd_keys ? 1 : 0;

	} else if (reg == 8) {
		if (!debugOn)
			clock_snap = clockticks6502 - clock_base;
		return (clock_snap >> 0) & 0xff;
	} else if (reg == 9) {
		return (clock_snap >> 8) & 0xff;
	} else if (reg == 10) {
		return (clock_snap >> 16) & 0xff;
	} else if (reg == 11) {
		return (clock_snap >> 24) & 0xff;

	} else if (reg == 13) {
		return keymap;
	} else if (reg == 14) {
		return '8'; // emulator detection: "816"
	} else if (reg == 15) {
		return '1';
	}
	if (!debugOn) printf("WARN: Invalid register %x\n", DEVICE_EMULATOR + reg);
	return -1;
}
