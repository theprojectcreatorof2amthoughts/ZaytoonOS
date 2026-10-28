#include "kernel/apps/capp.h"
#include "kernel/apps/capp_format.h"
#include "kernel/fs/vfs.h"
#include "kernel/kernel/sched.h"
#include "kernel/shell/string.h"

#define SHELL_MAX_ARGS 8
#define SHELL_LINE_MAX 128

static void sh_write(const capp_context_t *ctx, const char *text)
{
    ctx->write(text);
}

static void sh_writeln(const capp_context_t *ctx, const char *text)
{
    ctx->writeln(text);
}

static void sh_prompt(const capp_context_t *ctx)
{
    sh_write(ctx, vfs_cwd());
    sh_write(ctx, "> ");
}

static int tokenize(char *line, const char **argv)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < SHELL_MAX_ARGS) {
        while (*p == ' ' || *p == '\t') {
            *p++ = '\0';
        }
        if (!*p) {
            break;
        }
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') {
            ++p;
        }
    }
    return argc;
}

static bool has_capp_ext(const char *name)
{
    u32 len = shell_strlen(name);
    return len > 5 && shell_streq_ci(name + len - 5, ".capp");
}

static void cmd_help(const capp_context_t *ctx)
{
    sh_writeln(ctx, "  HELP / ?       show this list");
    sh_writeln(ctx, "  VER            kernel version");
    sh_writeln(ctx, "  CLS            clear screen");
    sh_writeln(ctx, "  DIR / LS       list directory");
    sh_writeln(ctx, "  CD <path>      change directory");
    sh_writeln(ctx, "  ECHO <text>    print text");
    sh_writeln(ctx, "  TYPE <file>    show file contents");
    sh_writeln(ctx, "  MEM            memory & heap info");
    sh_writeln(ctx, "  PS             running tasks");
    sh_writeln(ctx, "  UPTIME         scheduler ticks");
    sh_writeln(ctx, "  RUN <app.capp> execute a .capp program");
    sh_writeln(ctx, "  INSTALL <app.capp> copy a packaged app to HDD");
    sh_writeln(ctx, "  <app.capp> [args] run a .capp directly");
    sh_writeln(ctx, "  EXIT / QUIT    return to the caller");
    sh_writeln(ctx, "");
    sh_writeln(ctx, "EXAMPLES:");
    sh_writeln(ctx, "  hello.capp");
    sh_writeln(ctx, "  shell.capp");
    sh_writeln(ctx, "  number_guess.capp");
    sh_writeln(ctx, "  zayflight.capp");
}

static void cmd_cls(const capp_context_t *ctx)
{
    for (int i = 0; i < 25; ++i) {
        sh_write(ctx, "\n");
    }
}

static void cmd_type(const capp_context_t *ctx, const char *path)
{
    if (!path) {
        sh_writeln(ctx, "Usage: TYPE <file>");
    } else if (shell_streq_ci(path, "README.TXT")) {
        sh_writeln(ctx, "A:/ is the RAM drive; packaged .capp apps are in A:/BIN.");
        sh_writeln(ctx, "HDD:/APPS stores apps installed on the persistent data disk.");
        sh_writeln(ctx, "Use INSTALL <name.capp>, then RUN HDD:/APPS/<name.capp>.");
    } else {
        sh_writeln(ctx, "File not found.");
    }
}

static void exec_line(const capp_context_t *ctx, int argc, const char **argv)
{
    if (shell_streq_ci(argv[0], "HELP") || shell_streq_ci(argv[0], "?")) {
        cmd_help(ctx);
    } else if (shell_streq_ci(argv[0], "VER")) {
        sh_writeln(ctx, "ZaytoonOS kernel v1.0 (x86 32-bit, GRUB2 multiboot2)");
    } else if (shell_streq_ci(argv[0], "CLS") || shell_streq_ci(argv[0], "CLEAR")) {
        cmd_cls(ctx);
    } else if (shell_streq_ci(argv[0], "DIR") || shell_streq_ci(argv[0], "LS")) {
        vfs_list_dir(argc > 1 ? argv[1] : vfs_cwd(), ctx->writeln);
    } else if (shell_streq_ci(argv[0], "CD")) {
        if (argc < 2) sh_writeln(ctx, vfs_cwd());
        else if (!vfs_chdir(argv[1])) sh_writeln(ctx, "Path not found.");
    } else if (shell_streq_ci(argv[0], "TYPE") || shell_streq_ci(argv[0], "CAT")) {
        cmd_type(ctx, argc > 1 ? argv[1] : NULL);
    } else if (shell_streq_ci(argv[0], "ECHO")) {
        for (int i = 1; i < argc; ++i) {
            sh_write(ctx, argv[i]);
            if (i + 1 < argc) sh_write(ctx, " ");
        }
        sh_write(ctx, "\n");
    } else if (shell_streq_ci(argv[0], "MEM")) {
        sh_writeln(ctx, "Heap  : bump allocator (1 MiB region)");
        sh_writeln(ctx, "Paging: flat identity map (32-bit protected mode)");
        sh_writeln(ctx, "Sched : preemptive round-robin, timer IRQ 0");
    } else if (shell_streq_ci(argv[0], "PS")) {
        sh_writeln(ctx, "PID  NAME            STATE");
        sh_writeln(ctx, "  1  shell           running");
        sh_writeln(ctx, "  2  idle            ready");
    } else if (shell_streq_ci(argv[0], "UPTIME")) {
        char tmp[64];
        shell_format_u64(tmp, sizeof(tmp), "ticks = ", sched_ticks());
        sh_writeln(ctx, tmp);
    } else if (shell_streq_ci(argv[0], "INSTALL")) {
        if (argc < 2) {
            sh_writeln(ctx, "Usage: INSTALL <packaged-app.capp>");
        } else if (capp_install(argv[1]) != 0) {
            sh_writeln(ctx, "Install failed. Check that the HDD app volume is mounted.");
        } else {
            sh_writeln(ctx, "App installed to HDD:/APPS.");
        }
    } else if (shell_streq_ci(argv[0], "RUN") || shell_streq_ci(argv[0], "EXEC")) {
        if (argc < 2) {
            sh_writeln(ctx, "Usage: RUN <program.capp> [args]");
        } else if (capp_exec(argv[1], argc - 1, &argv[1]) != 0) {
            sh_writeln(ctx, "Error: program exited with non-zero status.");
        }
    } else if (has_capp_ext(argv[0])) {
        if (capp_exec(argv[0], argc, argv) != 0) {
            sh_writeln(ctx, "Error: program exited with non-zero status.");
        }
    } else if (shell_starts_with_ci(argv[0], "./") && has_capp_ext(argv[0] + 2)) {
        if (capp_exec(argv[0] + 2, argc, argv) != 0) {
            sh_writeln(ctx, "Error: program exited with non-zero status.");
        }
    } else {
        sh_write(ctx, "Unknown command: ");
        sh_writeln(ctx, argv[0]);
        sh_writeln(ctx, "Type HELP for a list of commands.");
    }
}

int shell_app_main(const capp_context_t *ctx)
{
    char line[SHELL_LINE_MAX];
    const char *argv[SHELL_MAX_ARGS];

    ctx->writeln("");
    ctx->writeln("ZaytoonOS Command Processor v1.0 (.capp)");
    ctx->writeln("Type HELP for available commands.");
    ctx->writeln("");

    if (capp_exists("startup.capp")) {
        ctx->writeln("Executing startup.capp...");
        const char *startup_argv[] = { "startup.capp" };
        capp_exec("startup.capp", 1, startup_argv);
        ctx->writeln("");
    }

    sh_prompt(ctx);
    for (;;) {
        u32 pos = 0;
        for (;;) {
            char c = ctx->readchar();
            if (c == '\n' || c == '\r') {
                line[pos] = '\0';
                sh_write(ctx, "\n");
                break;
            }
            if (c == '\b' || (unsigned char)c == 0x7Fu) {
                if (pos > 0) {
                    --pos;
                    sh_write(ctx, "\b \b");
                }
            } else if ((unsigned char)c >= 32 && pos < SHELL_LINE_MAX - 1) {
                line[pos++] = c;
                ctx->write_char(c);
            }
        }

        if (pos > 0) {
            char input[SHELL_LINE_MAX];
            shell_copy(input, sizeof(input), line);
            int argc = tokenize(input, argv);
            if (argc > 0) {
                if (shell_streq_ci(argv[0], "EXIT") || shell_streq_ci(argv[0], "QUIT")) {
                    ctx->writeln("Returning to caller.");
                    return 0;
                }
                exec_line(ctx, argc, argv);
            }
        }
        sh_prompt(ctx);
    }
}
