#ifndef ZAYTOONOS_ATA_H
#define ZAYTOONOS_ATA_H

#include "kernel/types.h"

bool ata_init(void);
u32 ata_sector_count(void);
bool ata_read_sector(u32 lba, void *buffer);
bool ata_write_sector(u32 lba, const void *buffer);

#endif
