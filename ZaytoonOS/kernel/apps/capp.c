#include "kernel/apps/capp.h"
#include "kernel/apps/capp_format.h"
#include "kernel/drivers/console.h"
#include "kernel/drivers/keyboard.h"
#include "kernel/kernel/printk.h"
#include "kernel/kernel/sched.h"
#include "kernel/shell/string.h"

static int sysinfo_main(const capp_context_t *ctx);
static int hello_main(const capp_context_t *ctx);
static int ticks_main(const capp_context_t *ctx);
extern int shell_app_main(const capp_context_t *ctx);

static const capp_image_t native_capps[] = {
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "hello.capp",         "print a greeting",                     hello_main         },
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "sysinfo.capp",       "kernel and runtime information",       sysinfo_main       },
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "ticks.capp",         "scheduler uptime tick counter",        ticks_main         },
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "shell.capp",         "interactive command-line shell",          shell_app_main     },
};

static void capp_write(const char *msg)   { console_write(msg); }
static void capp_writeln(const char *msg) { console_write(msg); console_write("\n"); }
static void capp_write_char(char c)       { console_write_char(c); }
static u64  capp_ticks(void)              { return sched_ticks(); }

static const char *basename(const char *path)
{
    const char *name = path;
    if (!path) return NULL;
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

static bool native_valid(const capp_image_t *image)
{
    return image && image->magic == CAPP_MAGIC && image->abi_version == CAPP_ABI_VERSION &&
           image->header_size == sizeof(capp_image_t) && image->entry;
}

static const capp_image_t *native_find(const char *path)
{
    const char *name = basename(path);
    if (!name || !*name) return NULL;
    for (u32 i = 0; i < (u32)(sizeof(native_capps) / sizeof(native_capps[0])); ++i) {
        if (shell_streq_ci(name, native_capps[i].name)) return &native_capps[i];
    }
    return NULL;
}

static bool blob_header_valid(const capp_blob_t *blob, const capp_file_header_t **hdr_out)
{
    if (!blob || !blob->data || blob->size < sizeof(capp_file_header_t)) return false;
    const capp_file_header_t *h = (const capp_file_header_t *)blob->data;
    if (h->magic != CAPP_FILE_MAGIC || h->abi_version != CAPP_FILE_ABI_VERSION ||
        h->header_size != sizeof(capp_file_header_t) ||
        h->record_offset < sizeof(capp_file_header_t) || h->record_offset > blob->size)
        return false;
    *hdr_out = h;
    return true;
}

static const capp_blob_t *blob_find(const char *path, const capp_file_header_t **hdr_out)
{
    const char *name = basename(path);
    if (!name || !*name) return NULL;
    for (u32 i = 0; i < generated_capp_count; ++i) {
        const capp_file_header_t *h = NULL;
        if (!blob_header_valid(&generated_capps[i], &h)) continue;
        if (shell_streq_ci(name, h->name) || shell_streq_ci(name, generated_capps[i].filename)) {
            *hdr_out = h;
            return &generated_capps[i];
        }
    }
    return NULL;
}

static void write_args(const capp_context_t *ctx)
{
    for (int i = 1; i < ctx->argc; ++i) { ctx->write(" "); ctx->write(ctx->argv[i]); }
    ctx->writeln("");
}

static int blob_exec(const capp_blob_t *blob, const capp_file_header_t *hdr, const capp_context_t *ctx)
{
    u32 off = hdr->record_offset;
    int exit_code = 0;
    for (u32 ri = 0; ri < hdr->record_count; ++ri) {
        if (off + sizeof(capp_record_header_t) > blob->size) {
            printk("[capp] truncated record table in %s", hdr->name);
            return -1;
        }
        const capp_record_header_t *rec = (const capp_record_header_t *)(blob->data + off);
        off += sizeof(capp_record_header_t);
        if (off + rec->length > blob->size) {
            printk("[capp] truncated record payload in %s", hdr->name);
            return -1;
        }
        const char *payload = (const char *)(blob->data + off);
        switch (rec->opcode) {
        case CAPP_OP_PRINT:   ctx->write(payload);   break;
        case CAPP_OP_PRINTLN: ctx->writeln(payload);  break;
        case CAPP_OP_ARGS:    write_args(ctx);         break;
        case CAPP_OP_TICKS: {
            char buf[64];
            shell_format_u64(buf, sizeof(buf), "ticks=", ctx->uptime_ticks());
            ctx->writeln(buf);
        } break;
        case CAPP_OP_EXIT:
            if (rec->length >= sizeof(int)) exit_code = *(const int *)payload;
            return exit_code;
        default:
            printk("[capp] unsupported opcode %u in %s", rec->opcode, hdr->name);
            return -1;
        }
        off += rec->length;
    }
    return exit_code;
}

void capp_init(void)
{
    printk("[capp] %u apps registered (%u packaged, %u native)",
           capp_count(), generated_capp_count,
           (u32)(sizeof(native_capps) / sizeof(native_capps[0])));
}

u32 capp_count(void)
{
    return generated_capp_count + (u32)(sizeof(native_capps) / sizeof(native_capps[0]));
}

bool capp_exists(const char *path)
{
    const capp_file_header_t *h = NULL;
    return blob_find(path, &h) != NULL || native_find(path) != NULL;
}

void capp_list(void (*writer)(const char *line))
{
    for (u32 i = 0; i < generated_capp_count; ++i) {
        const capp_file_header_t *h = NULL;
        if (blob_header_valid(&generated_capps[i], &h)) writer(h->name);
    }
    for (u32 i = 0; i < (u32)(sizeof(native_capps) / sizeof(native_capps[0])); ++i) {
        writer(native_capps[i].name);
    }
}

int capp_exec(const char *path, int argc, const char *const *argv)
{
    capp_context_t ctx = {
        .argc         = argc,
        .argv         = argv,
        .write        = capp_write,
        .writeln      = capp_writeln,
        .write_char   = capp_write_char,
        .uptime_ticks = capp_ticks,
        .readchar     = keyboard_getchar,
    };

    const capp_file_header_t *hdr = NULL;
    const capp_blob_t *blob = blob_find(path, &hdr);
    if (blob) return blob_exec(blob, hdr, &ctx);

    const capp_image_t *image = native_find(path);
    if (!native_valid(image)) {
        printk("[capp] not found: %s", path ? path : "<null>");
        return -1;
    }
    return image->entry(&ctx);
}

/* ─── native apps ───────────────────────────────────────────────────────── */

static int hello_main(const capp_context_t *ctx)
{
    ctx->writeln("Hello from ZaytoonOS — a native .capp executable.");
    if (ctx->argc > 1) {
        ctx->write("args:");
        for (int i = 1; i < ctx->argc; ++i) { ctx->write(" "); ctx->write(ctx->argv[i]); }
        ctx->writeln("");
    }
    return 0;
}

static int sysinfo_main(const capp_context_t *ctx)
{
    char buf[96];
    ctx->writeln("─────────────── ZaytoonOS System Info ───────────────");
    ctx->writeln("  Arch    : x86 32-bit protected mode");
    ctx->writeln("  Bootload: GRUB2 multiboot2");
    ctx->writeln("  Memory  : bump heap + identity-mapped paging");
    ctx->writeln("  Sched   : preemptive round-robin (timer IRQ 0)");
    shell_format_u64(buf, sizeof(buf), "  Uptime  : ", ctx->uptime_ticks());
    ctx->write(buf);
    ctx->writeln(" ticks");
    shell_format_u64(buf, sizeof(buf), "  Apps    : ", capp_count());
    ctx->write(buf);
    ctx->writeln(" registered");
    ctx->writeln("─────────────────────────────────────────────────────");
    return 0;
}

static int ticks_main(const capp_context_t *ctx)
{
    char buf[64];
    shell_format_u64(buf, sizeof(buf), "uptime = ", ctx->uptime_ticks());
    ctx->write(buf);
    ctx->writeln(" ticks");
    return 0;
}
