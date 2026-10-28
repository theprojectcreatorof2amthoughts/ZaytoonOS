#include "kernel/apps/capp.h"
#include "kernel/fs/vfs.h"
#include "kernel/shell/string.h"

#define EDITOR_BUFFER_SIZE 4096u
#define EDITOR_LINE_SIZE 128u

static void editor_print_contents(const capp_context_t *ctx, const char *buffer, u32 length)
{
    for (u32 i = 0; i < length; ++i) ctx->write_char(buffer[i]);
    if (length && buffer[length - 1u] != '\n') ctx->writeln("");
}

static u32 editor_read_line(const capp_context_t *ctx, char *line, u32 capacity)
{
    u32 length = 0;
    for (;;) {
        char c = ctx->readchar();
        if (c == '\r' || c == '\n') {
            ctx->writeln("");
            break;
        }
        if (c == '\b' || (u8)c == 0x7Fu) {
            if (length) {
                --length;
                ctx->write("\b \b");
            }
        } else if ((u8)c >= 32u && length < capacity - 1u) {
            line[length++] = c;
            ctx->write_char(c);
        }
    }
    line[length] = '\0';
    return length;
}

int cappytxt_main(const capp_context_t *ctx)
{
    if (ctx->argc < 2) {
        ctx->writeln("Usage: cappytxt HDD:/filename.ext");
        return 1;
    }

    char buffer[EDITOR_BUFFER_SIZE];
    u32 length = 0;
    u32 file_size = 0;
    const unsigned char *file = vfs_read_file(ctx->argv[1], &file_size);
    if (file) {
        if (file_size > sizeof(buffer)) {
            ctx->writeln("File is too large for cappytxt (maximum 4096 bytes).");
            return 1;
        }
        length = file_size;
        for (u32 i = 0; i < length; ++i) buffer[i] = (char)file[i];
    } else if (vfs_exists(ctx->argv[1])) {
        ctx->writeln("Unable to read file.");
        return 1;
    }

    ctx->writeln("cappytxt - enter text; :w saves, :q quits, :wq saves and quits, :clear empties.");
    editor_print_contents(ctx, buffer, length);
    for (;;) {
        char line[EDITOR_LINE_SIZE];
        ctx->write("> ");
        u32 line_length = editor_read_line(ctx, line, sizeof(line));

        if (shell_streq_ci(line, ":w") || shell_streq_ci(line, ":wq")) {
            if (!vfs_write_file(ctx->argv[1], (const unsigned char *)buffer, length)) {
                ctx->writeln("Save failed. Use an HDD:/ filename and keep the file under 4096 bytes.");
                continue;
            }
            ctx->writeln("Saved.");
            if (shell_streq_ci(line, ":wq")) return 0;
            continue;
        }
        if (shell_streq_ci(line, ":q")) return 0;
        if (shell_streq_ci(line, ":clear")) {
            length = 0;
            ctx->writeln("Buffer cleared.");
            continue;
        }

        if (length + line_length + 1u > sizeof(buffer)) {
            ctx->writeln("Buffer full (4096 bytes). Save or clear before adding more text.");
            continue;
        }
        for (u32 i = 0; i < line_length; ++i) buffer[length++] = line[i];
        buffer[length++] = '\n';
    }
}
