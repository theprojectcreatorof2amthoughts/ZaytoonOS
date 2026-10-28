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
