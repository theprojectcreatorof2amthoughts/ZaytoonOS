#ifndef ZAYTOONOS_KERNEL_APPS_CAPP_H
#define ZAYTOONOS_KERNEL_APPS_CAPP_H

#include "kernel/types.h"

#define CAPP_MAGIC       0x50504143u
#define CAPP_ABI_VERSION 1u

typedef struct capp_context {
    int argc;
    const char *const *argv;
    void (*write)(const char *message);
    void (*writeln)(const char *message);
    void (*write_char)(char character);
    u64  (*uptime_ticks)(void);
    char (*readchar)(void);
} capp_context_t;

typedef int (*capp_entry_t)(const capp_context_t *ctx);

typedef struct capp_image {
    u32         magic;
    u16         abi_version;
    u16         header_size;
    const char *name;
    const char *description;
    capp_entry_t entry;
} capp_image_t;

typedef struct capp_blob {
    const char          *filename;
    const unsigned char *data;
    u32                  size;
} capp_blob_t;

extern const capp_blob_t generated_capps[];
extern const u32         generated_capp_count;

void capp_init(void);
bool capp_exists(const char *path);
void capp_list(void (*writer)(const char *line));
int  capp_exec(const char *path, int argc, const char *const *argv);
int  capp_install(const char *path);
u32  capp_count(void);

#endif
