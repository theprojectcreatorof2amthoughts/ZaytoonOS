#ifndef ZAYTOONOS_APPCOMPILER_LIB_H
#define ZAYTOONOS_APPCOMPILER_LIB_H

#include <stdint.h>

#define CAPP_FILE_MAGIC 0x31505043u
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

void capp_begin(const char *output_path, const char *name, const char *description);
void capp_print(const char *text);
void capp_println(const char *text);
void capp_print_args(void);
void capp_print_ticks(void);
void capp_number_guess(void);
void capp_flight_sim(void);
void capp_exit(int code);
void capp_end(void);

#endif /* ZAYTOONOS_APPCOMPILER_LIB_H */
