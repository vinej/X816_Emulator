// X816 SD block device with DMA. See sdblock.c and X816_Core doc/MEMORY_MAP.md.
#ifndef _SDBLOCK_H_
#define _SDBLOCK_H_
#include <inttypes.h>
#include <stdbool.h>

void    sdblock_set_path(char const *path);
bool    sdblock_attach(void);
void    sdblock_detach(void);
void    sdblock_reset(void);
uint8_t sdblock_read(uint8_t reg, bool debugOn);
void    sdblock_write(uint8_t reg, uint8_t value);

#endif
