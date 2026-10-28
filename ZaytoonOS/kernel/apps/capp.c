#include "kernel/apps/capp.h"
#include "kernel/apps/capp_format.h"
#include "kernel/drivers/console.h"
#include "kernel/drivers/keyboard.h"
#include "kernel/fs/disk_store.h"
#include "kernel/kernel/printk.h"
#include "kernel/kernel/sched.h"
#include "kernel/shell/string.h"

static int sysinfo_main(const capp_context_t *ctx);
static int hello_main(const capp_context_t *ctx);
static int ticks_main(const capp_context_t *ctx);
extern int cappytxt_main(const capp_context_t *ctx);
extern int shell_app_main(const capp_context_t *ctx);

static const capp_image_t native_capps[] = {
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "hello.capp",         "print a greeting",                     hello_main         },
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "",       "kernel and runtime information",       sysinfo_main       },
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "ticks.capp",         "scheduler uptime tick counter",        ticks_main         },
    { CAPP_MAGIC, CAPP_ABI_VERSION, sizeof(capp_image_t), "cappytxt.capp",      "text editor for HDD files",            cappytxt_main      },
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

static u32 read_line(const capp_context_t *ctx, char *input, u32 capacity)
{
    u32 length = 0;
    for (;;) {
        char c = ctx->readchar();
        if (c == '\r' || c == '\n') {
            ctx->write("\n");
            break;
        }
        if (c == '\b' || (u8)c == 0x7Fu) {
            if (length) {
                --length;
                ctx->write("\b \b");
            }
        } else if ((u8)c >= 32u && length < capacity - 1u) {
            input[length++] = c;
            ctx->write_char(c);
        }
    }
    input[length] = '\0';
    return length;
}

static void flight_show_status(const capp_context_t *ctx, u32 speed, u32 altitude,
                               u32 heading, u32 throttle, u32 fuel, u32 runway,
                               int pitch, int bank, bool airborne, bool gear_down)
{
    char line[64];
    ctx->writeln("");
    ctx->writeln("----------- ZAYFLIGHT COCKPIT -----------");
    shell_format_u64(line, sizeof(line), "Airspeed: ", speed);
    ctx->write(line);
    ctx->writeln(" kt");
    shell_format_u64(line, sizeof(line), "Altitude: ", altitude);
    ctx->write(line);
    ctx->writeln(" ft");
    shell_format_u64(line, sizeof(line), "Heading:  ", heading);
    ctx->write(line);
    ctx->writeln(" deg");
    shell_format_u64(line, sizeof(line), "Throttle: ", throttle);
    ctx->write(line);
    ctx->writeln("%");
    shell_format_u64(line, sizeof(line), "Fuel:     ", fuel);
    ctx->write(line);
    ctx->writeln("%");
    if (airborne) {
        ctx->writeln("Aircraft: AIRBORNE");
    } else {
        shell_format_u64(line, sizeof(line), "Runway:   ", runway);
        ctx->write(line);
        ctx->writeln(" m remaining");
        ctx->writeln("Aircraft: ON RUNWAY");
    }
    ctx->write("Pitch: ");
    ctx->write(pitch > 0 ? "nose up" : pitch < 0 ? "nose down" : "level");
    ctx->write(" | Bank: ");
    ctx->write(bank > 0 ? "right" : bank < 0 ? "left" : "level");
    ctx->write(" | Gear: ");
    ctx->writeln(gear_down ? "down" : "up");
    ctx->writeln("-----------------------------------------");
}

static int flight_sim_main(const capp_context_t *ctx)
{
    u32 speed = 0;
    u32 altitude = 0;
    u32 heading = 90;
    u32 throttle = 100;
    u32 fuel = 100;
    u32 runway = 1800;
    int pitch = 0;
    int bank = 0;
    bool airborne = false;
    bool gear_down = true;
    bool flaps_down = true;

    ctx->writeln("Controls: W/S pitch, A/D bank, +/- throttle, F flaps, G gear, H level, Q quit.");
    ctx->writeln("Each command advances the flight by five seconds. Take off above 90 kt.");
    for (;;) {
        char command[16];
        flight_show_status(ctx, speed, altitude, heading, throttle, fuel, runway,
                           pitch, bank, airborne, gear_down);
        ctx->write("Pilot> ");
        u32 length = read_line(ctx, command, sizeof(command));
        if (length == 0) {
            continue;
        }

        char action = command[0];
        if (action >= 'A' && action <= 'Z') {
            action = (char)(action + ('a' - 'A'));
        }
        if (action == 'q') {
            ctx->writeln("Flight ended.");
            return 0;
        } else if (action == 'w' && pitch < 12) {
            pitch += 2;
        } else if (action == 's' && pitch > -8) {
            pitch -= 2;
        } else if (action == 'a' && bank > -45) {
            bank -= 10;
        } else if (action == 'd' && bank < 45) {
            bank += 10;
        } else if (action == 'h') {
            pitch = 0;
            bank = 0;
        } else if (action == 'f') {
            flaps_down = !flaps_down;
            ctx->writeln(flaps_down ? "Flaps extended." : "Flaps retracted.");
        } else if (action == 'g') {
            gear_down = !gear_down;
            ctx->writeln(gear_down ? "Landing gear extended." : "Landing gear retracted.");
        } else if (action == '+' && throttle <= 90) {
            throttle += 10;
        } else if (action == '-' && throttle >= 10) {
            throttle -= 10;
        } else if (action != 'w' && action != 's' && action != 'a' && action != 'd' &&
                   action != 'h' && action != 'f' && action != 'g' &&
                   action != '+' && action != '-') {
            ctx->writeln("Unknown control. Use W/S/A/D, +/-, F, G, H, or Q.");
            continue;
        }

        if (fuel) {
            u32 burn = throttle / 50u + 1u;
            fuel = fuel > burn ? fuel - burn : 0;
        } else {
            throttle = 0;
        }

        if (!airborne) {
            int acceleration = ((int)throttle - 20) / 5;
            if (acceleration < 0 && speed < (u32)(-acceleration)) {
                speed = 0;
            } else {
                speed = (u32)((int)speed + acceleration);
            }
            if (speed >= 90 && pitch >= 6 && flaps_down) {
                airborne = true;
                altitude = 100;
                ctx->writeln("Rotation! Positive climb established.");
            } else {
                u32 distance = speed * 12u / 10u;
                runway = runway > distance ? runway - distance : 0;
                if (runway == 0) {
                    ctx->writeln("Runway overrun. Flight simulator reset to the runway.");
                    speed = 0;
                    altitude = 0;
                    runway = 1800;
                    pitch = 0;
                }
                continue;
            }
        } else {
            if (throttle > 50) {
                speed += 2;
            } else if (speed > 1) {
                --speed;
            }
            if (throttle == 0 && speed > 2) {
                speed -= 2;
            }
            if (pitch > 0) {
                altitude += (u32)(pitch * 40);
            } else if (pitch < 0) {
                u32 descent = (u32)(-pitch * 40);
                altitude = altitude > descent ? altitude - descent : 0;
            }
            if (bank < 0) {
                heading = (heading + 360u - (u32)(-bank / 5)) % 360u;
            } else {
                heading = (heading + (u32)(bank / 5)) % 360u;
            }
            if (!gear_down) {
                speed += 1;
            }
            if (altitude <= 100 && speed <= 90 && pitch >= -2 && pitch <= 2 &&
                bank >= -10 && bank <= 10 && gear_down) {
                ctx->writeln("Safe landing! Thanks for flying ZayFlight.");
                return 0;
            }
            if (altitude == 0) {
                ctx->writeln("Hard landing. Adjust pitch, speed, bank, and gear on approach.");
                return 1;
            }
        }
    }
}

static int blob_exec(const capp_blob_t *blob, const capp_file_header_t *hdr, const capp_context_t *ctx)
{
    u32 off = hdr->record_offset;
    int exit_code = 0;
    for (u32 ri = 0; ri < hdr->record_count; ++ri) {
        if (off > blob->size || sizeof(capp_record_header_t) > blob->size - off) {
            printk("[capp] truncated record table in %s", hdr->name);
            return -1;
        }
        const capp_record_header_t *rec = (const capp_record_header_t *)(blob->data + off);
        off += sizeof(capp_record_header_t);
        if (rec->length > blob->size - off) {
            printk("[capp] truncated record payload in %s", hdr->name);
            return -1;
        }
        const char *payload = (const char *)(blob->data + off);
        switch (rec->opcode) {
        case CAPP_OP_PRINT:
        case CAPP_OP_PRINTLN:
            if (!rec->length || payload[rec->length - 1u] != '\0') {
                printk("[capp] invalid string record in %s", hdr->name);
                return -1;
            }
            if (rec->opcode == CAPP_OP_PRINT) ctx->write(payload);
            else ctx->writeln(payload);
            break;
        case CAPP_OP_ARGS:    write_args(ctx);         break;
        case CAPP_OP_NUMBER_GUESS: {
            if (rec->length != 0) {
                printk("[capp] invalid game record in %s", hdr->name);
                return -1;
            }
            u32 secret = (u32)(ctx->uptime_ticks() % 50u) + 1u;
            u32 attempts = 0;
            for (;;) {
                char input[12];
                u32 length;
                u32 guess = 0;
                bool valid = true;

                ctx->write("Guess (1-50, Q to quit): ");
                length = read_line(ctx, input, sizeof(input));

                if (length == 1 && (input[0] == 'q' || input[0] == 'Q')) {
                    ctx->writeln("Game ended.");
                    break;
                }
                if (!length) valid = false;
                for (u32 i = 0; i < length && valid; ++i) {
                    if (input[i] < '0' || input[i] > '9') {
                        valid = false;
                    } else {
                        guess = guess * 10u + (u32)(input[i] - '0');
                        if (guess > 50u) valid = false;
                    }
                }
                if (!valid || guess == 0) {
                    ctx->writeln("Enter a number from 1 to 50, or Q.");
                    continue;
                }

                ++attempts;
                if (guess == secret) {
                    char count[32];
                    shell_format_u64(count, sizeof(count), "You got it in ", attempts);
                    ctx->write(count);
                    ctx->writeln(" guesses!");
                    break;
                }
                ctx->writeln(guess < secret ? "Too low." : "Too high.");
            }
        } break;
        case CAPP_OP_FLIGHT_SIM:
            if (rec->length != 0) {
                printk("[capp] invalid flight simulator record in %s", hdr->name);
                return -1;
            }
            exit_code = flight_sim_main(ctx);
            if (exit_code != 0) return exit_code;
            break;
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
    return blob_find(path, &h) != NULL || native_find(path) != NULL || disk_store_contains(path);
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
    bool explicit_hdd_path = path && shell_starts_with_ci(path, "HDD:/APPS/");
    u32 disk_size = 0;
    const unsigned char *disk_data = disk_store_read(path, &disk_size);
    if (disk_data) {
        capp_blob_t disk_blob = { path, disk_data, disk_size };
        if (!blob_header_valid(&disk_blob, &hdr)) {
            printk("[capp] invalid installed app: %s", path);
            return -1;
        }
        return blob_exec(&disk_blob, hdr, &ctx);
    }
    if (explicit_hdd_path) {
        printk("[capp] installed app not found: %s", path);
        return -1;
    }

    const capp_blob_t *blob = blob_find(path, &hdr);
    if (blob) return blob_exec(blob, hdr, &ctx);

    const capp_image_t *image = explicit_hdd_path ? NULL : native_find(path);
    if (!native_valid(image)) {
        printk("[capp] not found: %s", path ? path : "<null>");
        return -1;
    }
    return image->entry(&ctx);
}

int capp_install(const char *path)
{
    const capp_file_header_t *header = NULL;
    const capp_blob_t *blob = blob_find(path, &header);
    if (!blob) {
        printk("[capp] only packaged apps can be installed: %s", path ? path : "<null>");
        return -1;
    }
    return disk_store_install(header->name, (const unsigned char *)blob->data, blob->size) ? 0 : -1;
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
