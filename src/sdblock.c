// X816 SD block device with DMA.
//
// The counterpart of rtl/sd_block.sv in X816_Core. Same registers, same
// semantics, so a conformance test binary proves both implementations.
//
// This is NOT the X16's SD path. The X16 bit-bangs VERA's SPI at $9F3E and
// src/sdcard.c emulates the SPI protocol behind it -- CMD17/CMD24, CRC7, R1
// responses, one byte per transfer. X816's kernel is written from scratch
// (doc/KERNEL.md), so it gets a block device instead: write an LBA, a 24-bit
// destination and a count, and the hardware moves the data.
//
// On hardware the CPU is frozen for the duration of a transfer, so software
// never polls -- the instruction after the command write runs once the
// transfer has completed. Here the transfer is instantaneous, which produces
// exactly the same observable behaviour: busy always reads back 0.
//
// Backed by the same image file as -sdcard, opened through the same x16file
// layer as src/sdcard.c so both paths see one file.

#include <stdio.h>
#include <string.h>
#include "sdblock.h"
#include "memory.h"
#include "files.h"

#define SD_BLOCK_SIZE 512

// Registers, as offsets within the SYSCTL page.
#define R_LBA0   0x1
#define R_LBA1   0x2
#define R_LBA2   0x3
#define R_LBA3   0x4
#define R_MEM0   0x5
#define R_MEM1   0x6
#define R_MEM2   0x7
#define R_COUNT  0x8
#define R_CMD    0x9
#define R_DATA   0xA

#define CMD_READ    1   // card -> memory, COUNT blocks, by DMA
#define CMD_WRITE   2   // buffer -> card, one block
#define CMD_READBUF 3   // card -> buffer, one block, memory untouched
#define CMD_RESET   4   // rewind the buffer window

#define ST_BUSY     0x01
#define ST_ERROR    0x02
#define ST_PRESENT  0x80

extern uint8_t *RAM;

static uint32_t reg_lba;
static uint32_t reg_mem;
static uint8_t  reg_count = 1;
static bool     reg_error;
static uint8_t  blkbuf[SD_BLOCK_SIZE];
static uint16_t bufptr;

static struct x16file *img;
static char const     *img_path;

void
sdblock_set_path(char const *path)
{
	img_path = path;
}

bool
sdblock_attach(void)
{
	if (img != NULL)
		return true;
	if (img_path == NULL)
		return false;
	img = x16open(img_path, "r+b");
	if (img == NULL)
		img = x16open(img_path, "rb");   // read-only image is still usable
	return img != NULL;
}

void
sdblock_detach(void)
{
	if (img != NULL) {
		x16close(img);
		img = NULL;
	}
}

void
sdblock_reset(void)
{
	reg_lba = 0;
	reg_mem = 0;
	reg_count = 1;
	reg_error = false;
	bufptr = 0;
	memset(blkbuf, 0, sizeof blkbuf);
}

// Returns false and leaves reg_error set if the block is off the end of the
// image. A short image is the normal case -- the guest is expected to notice.
static bool
read_block(uint32_t lba, uint8_t *dest)
{
	if (img == NULL)
		return false;
	if ((Sint64)lba * SD_BLOCK_SIZE + SD_BLOCK_SIZE > x16size(img))
		return false;
	x16seek(img, (Sint64)lba * SD_BLOCK_SIZE, XSEEK_SET);
	return x16read(img, dest, 1, SD_BLOCK_SIZE) == SD_BLOCK_SIZE;
}

static bool
write_block(uint32_t lba, const uint8_t *src)
{
	if (img == NULL)
		return false;
	if ((Sint64)lba * SD_BLOCK_SIZE + SD_BLOCK_SIZE > x16size(img))
		return false;
	x16seek(img, (Sint64)lba * SD_BLOCK_SIZE, XSEEK_SET);
	return x16write(img, src, 1, SD_BLOCK_SIZE) == SD_BLOCK_SIZE;
}

static void
do_command(uint8_t cmd)
{
	uint8_t tmp[SD_BLOCK_SIZE];

	reg_error = false;
	bufptr = 0;

	switch (cmd) {
	case CMD_READ: {
		// COUNT of 0 transfers nothing rather than 256 blocks: a count that
		// silently means something other than what it says is exactly the
		// kind of thing a filesystem bug hides behind.
		uint32_t lba = reg_lba;
		uint32_t mem = reg_mem;
		for (unsigned i = 0; i < reg_count; i++) {
			if (!read_block(lba, tmp)) {
				reg_error = true;
				return;
			}
			// The DMA wraps within the 16 MB space, as the RTL's 24-bit
			// address register does.
			for (unsigned b = 0; b < SD_BLOCK_SIZE; b++)
				RAM[(mem + b) & 0xFFFFFF] = tmp[b];
			lba++;
			mem = (mem + SD_BLOCK_SIZE) & 0xFFFFFF;
		}
		break;
	}
	case CMD_READBUF:
		if (!read_block(reg_lba, blkbuf))
			reg_error = true;
		break;

	case CMD_WRITE:
		if (!write_block(reg_lba, blkbuf))
			reg_error = true;
		break;

	case CMD_RESET:
		break;

	default:
		reg_error = true;
		break;
	}
}

uint8_t
sdblock_read(uint8_t reg, bool debugOn)
{
	switch (reg) {
	case R_LBA0:  return (uint8_t)(reg_lba);
	case R_LBA1:  return (uint8_t)(reg_lba >> 8);
	case R_LBA2:  return (uint8_t)(reg_lba >> 16);
	case R_LBA3:  return (uint8_t)(reg_lba >> 24);
	case R_MEM0:  return (uint8_t)(reg_mem);
	case R_MEM1:  return (uint8_t)(reg_mem >> 8);
	case R_MEM2:  return (uint8_t)(reg_mem >> 16);
	case R_COUNT: return reg_count;
	case R_CMD:
		// busy is always 0: on hardware the CPU is stalled for the whole
		// transfer, so it can never observe itself busy either.
		return (uint8_t)((reg_error ? ST_ERROR : 0)
		               | (img != NULL ? ST_PRESENT : 0));
	case R_DATA: {
		uint8_t v = blkbuf[bufptr];
		if (!debugOn)                        // a debugger peek must not advance
			bufptr = (uint16_t)((bufptr + 1) & (SD_BLOCK_SIZE - 1));
		return v;
	}
	default:
		return 0x00;
	}
}

void
sdblock_write(uint8_t reg, uint8_t value)
{
	switch (reg) {
	case R_LBA0:  reg_lba = (reg_lba & 0xFFFFFF00u) | value;             break;
	case R_LBA1:  reg_lba = (reg_lba & 0xFFFF00FFu) | ((uint32_t)value << 8);  break;
	case R_LBA2:  reg_lba = (reg_lba & 0xFF00FFFFu) | ((uint32_t)value << 16); break;
	case R_LBA3:  reg_lba = (reg_lba & 0x00FFFFFFu) | ((uint32_t)value << 24); break;
	case R_MEM0:  reg_mem = (reg_mem & 0xFFFF00u) | value;               break;
	case R_MEM1:  reg_mem = (reg_mem & 0xFF00FFu) | ((uint32_t)value << 8);   break;
	case R_MEM2:  reg_mem = (reg_mem & 0x00FFFFu) | ((uint32_t)value << 16);  break;
	case R_COUNT: reg_count = value;                                     break;
	case R_CMD:   do_command(value & 0x07);                              break;
	case R_DATA:
		blkbuf[bufptr] = value;
		bufptr = (uint16_t)((bufptr + 1) & (SD_BLOCK_SIZE - 1));
		break;
	default:
		break;
	}
}
