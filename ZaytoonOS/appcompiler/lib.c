#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct capp_file_header {
    uint32_t magic;
    uint16_t abi_version;
    uint16_t header_size;
    uint32_t record_offset;
    uint32_t record_count;
    char name[CAPP_NAME_MAX];
    char description[CAPP_DESC_MAX];
} __attribute__((packed)) capp_file_header_t;

typedef struct capp_record_header {
    uint16_t opcode;
    uint16_t flags;
    uint32_t length;
} __attribute__((packed)) capp_record_header_t;

static FILE *out;
static capp_file_header_t header;

static void checked_write(const void *data, size_t size)
{
    if (fwrite(data, 1, size, out) != size) {
        perror("capp write");
        exit(1);
    }
}

static void copy_field(char *dst, size_t dst_size, const char *src)
{
    if (!src) {
        src = "";
    }
    snprintf(dst, dst_size, "%s", src);
}

static void emit_record(capp_record_opcode_t opcode, const void *payload, uint32_t length)
{
    if (!out) {
        fprintf(stderr, "capp_begin must be called before emitting records\n");
        exit(1);
    }

    capp_record_header_t record = {
        .opcode = (uint16_t)opcode,
        .flags = 0,
        .length = length,
    };
    checked_write(&record, sizeof(record));
    if (length) {
        checked_write(payload, length);
    }
    header.record_count++;
}

void capp_begin(const char *output_path, const char *name, const char *description)
{
    out = fopen(output_path, "wb");
    if (!out) {
        perror(output_path);
        exit(1);
    }

    memset(&header, 0, sizeof(header));
    header.magic = CAPP_FILE_MAGIC;
    header.abi_version = CAPP_FILE_ABI_VERSION;
    header.header_size = sizeof(header);
    header.record_offset = sizeof(header);
    copy_field(header.name, sizeof(header.name), name);
    copy_field(header.description, sizeof(header.description), description);
    checked_write(&header, sizeof(header));
}

void capp_print(const char *text)
{
    emit_record(CAPP_OP_PRINT, text, (uint32_t)strlen(text) + 1u);
}

void capp_println(const char *text)
{
    emit_record(CAPP_OP_PRINTLN, text, (uint32_t)strlen(text) + 1u);
}

void capp_print_args(void)
{
    emit_record(CAPP_OP_ARGS, NULL, 0);
}

void capp_print_ticks(void)
{
    emit_record(CAPP_OP_TICKS, NULL, 0);
}

void capp_number_guess(void)
{
    emit_record(CAPP_OP_NUMBER_GUESS, NULL, 0);
}

void capp_flight_sim(void)
{
    emit_record(CAPP_OP_FLIGHT_SIM, NULL, 0);
}

void capp_exit(int code)
{
    emit_record(CAPP_OP_EXIT, &code, sizeof(code));
}

void capp_end(void)
{
    if (!out) {
        return;
    }

    if (fseek(out, 0, SEEK_SET) != 0) {
        perror("capp seek");
        exit(1);
    }
    checked_write(&header, sizeof(header));
    fclose(out);
    out = NULL;
}
