#include "kernel/fs/vfs.h"
#include "kernel/apps/capp.h"
#include "kernel/fs/disk_store.h"
#include "kernel/kernel/printk.h"
#include "kernel/shell/string.h"

static char current_dir[32];

void vfs_init(void)
{
    shell_copy(current_dir, sizeof(current_dir), "A:/");
    printk("[vfs] mounted RAM drive at %s", current_dir);
    disk_store_init();
}

const char *vfs_cwd(void)
{
    return current_dir;
}

bool vfs_chdir(const char *path)
{
    if (!path || shell_streq_ci(path, "/") || shell_streq_ci(path, "A:/") || shell_streq_ci(path, "\\")) {
        shell_copy(current_dir, sizeof(current_dir), "A:/");
        return true;
    }
    if (shell_streq_ci(path, "BIN") || shell_streq_ci(path, "/BIN") || shell_streq_ci(path, "A:/BIN")) {
        shell_copy(current_dir, sizeof(current_dir), "A:/BIN");
        return true;
    }
    if (shell_streq_ci(path, "HDD:") || shell_streq_ci(path, "HDD:/")) {
        shell_copy(current_dir, sizeof(current_dir), "HDD:/");
        return true;
    }
    if (shell_streq_ci(path, "HDD:/APPS")) {
        shell_copy(current_dir, sizeof(current_dir), "HDD:/APPS");
        return true;
    }
    return false;
}

bool vfs_exists(const char *path)
{
    return shell_streq_ci(path, "BIN") || shell_streq_ci(path, "/BIN") || shell_streq_ci(path, "A:/BIN") ||
           shell_streq_ci(path, "HDD:") || shell_streq_ci(path, "HDD:/") ||
           shell_streq_ci(path, "HDD:/APPS") || capp_exists(path);
}

static const char *hdd_root_name(const char *path)
{
    if (!path || !shell_starts_with_ci(path, "HDD:/")) return NULL;
    const char *name = path + 5;
    if (!*name) return NULL;
    for (const char *p = name; *p; ++p) {
        if (*p == '/' || *p == '\\') return NULL;
    }
    return name;
}

const unsigned char *vfs_read_file(const char *path, u32 *size_out)
{
    if (size_out) *size_out = 0;
    const char *name = hdd_root_name(path);
    if (!name || shell_streq_ci(name, "APPS")) return NULL;
    return disk_store_read(name, size_out);
}

bool vfs_write_file(const char *path, const unsigned char *data, u32 size)
{
    const char *name = hdd_root_name(path);
    if (!name || shell_streq_ci(name, "APPS")) return false;
    return disk_store_write_file(name, data, size);
}

static void write_root(void (*writer)(const char *line))
{
    writer("<DIR> BIN");
    writer("<DIR> HDD");
    writer("README.TXT");
}

static void write_bin(void (*writer)(const char *line))
{
    capp_list(writer);
}

static void write_hdd_root(void (*writer)(const char *line))
{
    writer("<DIR> APPS");
    disk_store_list(writer);
}

void vfs_list_dir(const char *path, void (*writer)(const char *line))
{
    if (!path || !*path || shell_streq_ci(path, "A:/") || shell_streq_ci(path, "/") || shell_streq_ci(path, "\\")) {
        write_root(writer);
        return;
    }
    if (shell_streq_ci(path, "BIN") || shell_streq_ci(path, "/BIN") || shell_streq_ci(path, "A:/BIN")) {
        write_bin(writer);
        return;
    }
    if (shell_streq_ci(path, "HDD:") || shell_streq_ci(path, "HDD:/")) {
        write_hdd_root(writer);
        return;
    }
    if (shell_streq_ci(path, "HDD:/APPS")) {
        disk_store_list(writer);
        return;
    }
    writer("File not found");
}
