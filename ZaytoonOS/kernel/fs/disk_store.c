#include "kernel/fs/disk_store.h"
#include "kernel/apps/capp_format.h"
#include "kernel/drivers/ata.h"
#include "kernel/kernel/printk.h"
#include "kernel/shell/string.h"

#define STORE_MAGIC        0x3153445Au
#define STORE_VERSION      1u
#define STORE_ENTRY_COUNT  12u
#define STORE_SLOT_SECTORS 128u
#define STORE_SLOT_BYTES   (STORE_SLOT_SECTORS * 512u)
#define STORE_FIRST_DATA_LBA 1u

typedef struct store_entry {
    char name[CAPP_NAME_MAX];
    u32 size;
} __attribute__((packed)) store_entry_t;

typedef struct store_header {
    u32 magic;
    u16 version;
    u16 entry_count;
    u16 slot_sectors;
    u16 reserved;
    u32 total_sectors;
    store_entry_t entries[STORE_ENTRY_COUNT];
    u8 padding[64];
} __attribute__((packed)) store_header_t;

_Static_assert(sizeof(store_header_t) == 512, "disk store metadata must fit one sector");

static store_header_t volume;
static unsigned char app_buffer[STORE_SLOT_BYTES];
static bool mounted;

static const char *path_basename(const char *path)
{
    const char *name = path;
    if (!path) return NULL;
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

static bool entry_name_valid(const store_entry_t *entry)
{
    if (!entry->name[0]) return entry->size == 0;
    bool terminated = false;
    for (u32 i = 0; i < CAPP_NAME_MAX; ++i) {
        if (!entry->name[i]) {
            terminated = true;
            break;
        }
    }
    return terminated && entry->size <= STORE_SLOT_BYTES;
}

bool disk_store_init(void)
{
    mounted = false;
    if (!ata_init()) {
        printk("[disk] no primary-master ATA disk detected");
        return false;
    }
    if (!ata_read_sector(0, &volume)) {
        printk("[disk] unable to read disk metadata");
        return false;
    }
    if (volume.magic != STORE_MAGIC || volume.version != STORE_VERSION ||
        volume.entry_count != STORE_ENTRY_COUNT || volume.slot_sectors != STORE_SLOT_SECTORS ||
        volume.total_sectors > ata_sector_count() ||
        volume.total_sectors < STORE_FIRST_DATA_LBA + STORE_ENTRY_COUNT * STORE_SLOT_SECTORS) {
        printk("[disk] no compatible app volume; disk left unchanged");
        return false;
    }
    for (u32 i = 0; i < STORE_ENTRY_COUNT; ++i) {
        if (!entry_name_valid(&volume.entries[i])) {
            printk("[disk] invalid app-volume directory; disk left unchanged");
            return false;
        }
    }
    mounted = true;
    printk("[disk] mounted persistent app volume (%u sectors)", volume.total_sectors);
    return true;
}

bool disk_store_contains(const char *path)
{
    const char *name = path_basename(path);
    if (!mounted || !name || !*name) return false;
    for (u32 i = 0; i < STORE_ENTRY_COUNT; ++i) {
        if (volume.entries[i].name[0] && shell_streq_ci(name, volume.entries[i].name)) return true;
    }
    return false;
}

const unsigned char *disk_store_read(const char *path, u32 *size_out)
{
    const char *name = path_basename(path);
    if (size_out) *size_out = 0;
    if (!mounted || !name || !*name) return NULL;

    for (u32 slot = 0; slot < STORE_ENTRY_COUNT; ++slot) {
        const store_entry_t *entry = &volume.entries[slot];
        if (!entry->name[0] || !shell_streq_ci(name, entry->name)) continue;
        u32 sectors_needed = (entry->size + 511u) / 512u;
        u32 first_lba = STORE_FIRST_DATA_LBA + slot * STORE_SLOT_SECTORS;
        for (u32 i = 0; i < sectors_needed; ++i) {
            if (!ata_read_sector(first_lba + i, app_buffer + i * 512u)) {
                printk("[disk] read failed for %s", entry->name);
                return NULL;
            }
        }
        if (size_out) *size_out = entry->size;
        return app_buffer;
    }
    return NULL;
}

static bool file_name_valid(const char *name, u32 *length_out)
{
    if (!name || !*name) return false;
    u32 name_length = shell_strlen(name);
    if (name_length >= CAPP_NAME_MAX || shell_streq_ci(name, ".") || shell_streq_ci(name, ".."))
        return false;
    for (u32 i = 0; i < name_length; ++i) {
        if (name[i] == '/' || name[i] == '\\' || (u8)name[i] < 32u) return false;
    }
    if (length_out) *length_out = name_length;
    return true;
}

bool disk_store_write_file(const char *name, const unsigned char *data, u32 size)
{
    u32 name_length;
    if (!mounted || (!data && size) || size > STORE_SLOT_BYTES ||
        !file_name_valid(name, &name_length)) return false;

    u32 slot = STORE_ENTRY_COUNT;
    for (u32 i = 0; i < STORE_ENTRY_COUNT; ++i) {
        if (volume.entries[i].name[0] && shell_streq_ci(name, volume.entries[i].name)) {
            slot = i;
            break;
        }
        if (!volume.entries[i].name[0] && slot == STORE_ENTRY_COUNT) slot = i;
    }
    if (slot == STORE_ENTRY_COUNT) {
        printk("[disk] app volume is full");
        return false;
    }

    u32 sectors_needed = (size + 511u) / 512u;
    u32 first_lba = STORE_FIRST_DATA_LBA + slot * STORE_SLOT_SECTORS;
    for (u32 i = 0; i < sectors_needed; ++i) {
        unsigned char sector[512];
        for (u32 j = 0; j < sizeof(sector); ++j) sector[j] = 0;
        u32 offset = i * 512u;
        u32 remaining = size - offset;
        u32 copy_size = remaining < sizeof(sector) ? remaining : sizeof(sector);
        for (u32 j = 0; j < copy_size; ++j) sector[j] = data[offset + j];
        if (!ata_write_sector(first_lba + i, sector)) {
            printk("[disk] write failed while installing %s", name);
            return false;
        }
    }

    for (u32 i = 0; i < CAPP_NAME_MAX; ++i) volume.entries[slot].name[i] = 0;
    for (u32 i = 0; i < name_length; ++i) volume.entries[slot].name[i] = name[i];
    volume.entries[slot].size = size;
    if (!ata_write_sector(0, &volume)) {
        mounted = false;
        printk("[disk] directory update failed; app volume unmounted");
        return false;
    }
    printk("[disk] saved %s (%u bytes)", name, size);
    return true;
}

bool disk_store_install(const char *name, const unsigned char *data, u32 size)
{
    if (!name || !data || size < sizeof(capp_file_header_t)) return false;
    const capp_file_header_t *header = (const capp_file_header_t *)data;
    if (header->magic != CAPP_FILE_MAGIC || header->abi_version != CAPP_FILE_ABI_VERSION ||
        header->header_size != sizeof(*header) ||
        header->name[CAPP_NAME_MAX - 1u] != '\0' ||
        !shell_streq_ci(name, header->name)) return false;
    return disk_store_write_file(name, data, size);
}

void disk_store_list(void (*writer)(const char *line))
{
    if (!mounted) {
        writer("No compatible hard-drive app volume.");
        return;
    }
    bool any = false;
    for (u32 i = 0; i < STORE_ENTRY_COUNT; ++i) {
        if (volume.entries[i].name[0]) {
            writer(volume.entries[i].name);
            any = true;
        }
    }
    if (!any) writer("(no installed apps)");
}
