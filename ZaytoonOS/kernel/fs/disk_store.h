#ifndef ZAYTOONOS_DISK_STORE_H
#define ZAYTOONOS_DISK_STORE_H

#include "kernel/types.h"

bool disk_store_init(void);
bool disk_store_contains(const char *path);
const unsigned char *disk_store_read(const char *path, u32 *size_out);
bool disk_store_write_file(const char *name, const unsigned char *data, u32 size);
bool disk_store_install(const char *name, const unsigned char *data, u32 size);
void disk_store_list(void (*writer)(const char *line));

#endif
