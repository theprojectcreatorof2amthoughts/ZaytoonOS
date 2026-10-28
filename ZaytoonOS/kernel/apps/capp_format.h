#ifndef ZAYTOONOS_KERNEL_APPS_CAPP_FORMAT_H
#define ZAYTOONOS_KERNEL_APPS_CAPP_FORMAT_H

#include "kernel/types.h"

#define CAPP_FILE_MAGIC 0x31505043u /* CPP1 little-endian: CAPP packaged program v1 */
#define CAPP_FILE_ABI_VERSION 1u
#define CAPP_NAME_MAX 32u
#define CAPP_DESC_MAX 96u

typedef enum capp_record_opcode {
    CAPP_OP_PRINT = 1,
    CAPP_OP_PRINTLN = 2,
    CAPP_OP_ARGS = 3,
    CAPP_OP_TICKS = 4,
    CAPP_OP_EXIT = 5,
    CAPP_OP_NUMBER_GUESS = 6,
    CAPP_OP_FLIGHT_SIM = 7,
} capp_record_opcode_t;

typedef struct capp_file_header {
    u32 magic;
    u16 abi_version;
    u16 header_size;
    u32 record_offset;
    u32 record_count;
    char name[CAPP_NAME_MAX];
    char description[CAPP_DESC_MAX];
} __attribute__((packed)) capp_file_header_t;

typedef struct capp_record_header {
    u16 opcode;
    u16 flags;
    u32 length;
} __attribute__((packed)) capp_record_header_t;

#endif /* ZAYTOONOS_KERNEL_APPS_CAPP_FORMAT_H */
