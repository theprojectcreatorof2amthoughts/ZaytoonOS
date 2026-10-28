#include "kernel/drivers/ata.h"
#include "kernel/arch/io.h"

#define ATA_DATA       0x1F0u
#define ATA_ERROR      0x1F1u
#define ATA_SECTOR_COUNT 0x1F2u
#define ATA_LBA_LOW    0x1F3u
#define ATA_LBA_MID    0x1F4u
#define ATA_LBA_HIGH   0x1F5u
#define ATA_DRIVE      0x1F6u
#define ATA_STATUS     0x1F7u
#define ATA_COMMAND    0x1F7u
#define ATA_CONTROL    0x3F6u

#define ATA_STATUS_ERR  0x01u
#define ATA_STATUS_DRQ  0x08u
#define ATA_STATUS_DF   0x20u
#define ATA_STATUS_BSY  0x80u
#define ATA_TIMEOUT     1000000u

static u32 sectors;

static void ata_delay(void)
{
    (void)inb(ATA_CONTROL);
    (void)inb(ATA_CONTROL);
    (void)inb(ATA_CONTROL);
    (void)inb(ATA_CONTROL);
}

static bool wait_data_ready(void)
{
    for (u32 i = 0; i < ATA_TIMEOUT; ++i) {
        u8 status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFFu) return false;
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) return false;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) return true;
        asm volatile("pause");
    }
    return false;
}

static bool wait_command_done(void)
{
    for (u32 i = 0; i < ATA_TIMEOUT; ++i) {
        u8 status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFFu) return false;
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) return false;
        if (!(status & (ATA_STATUS_BSY | ATA_STATUS_DRQ))) return true;
        asm volatile("pause");
    }
    return false;
}

bool ata_init(void)
{
    sectors = 0;
    outb(ATA_DRIVE, 0xA0u);
    ata_delay();
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, 0xECu);

    u8 status = inb(ATA_STATUS);
    if (status == 0 || status == 0xFFu) return false;
    ata_delay();

    for (u32 i = 0; i < ATA_TIMEOUT; ++i) {
        status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFFu) return false;
        if (!(status & ATA_STATUS_BSY)) break;
        if (i + 1u == ATA_TIMEOUT) return false;
    }

    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0 || !wait_data_ready()) return false;

    u16 identify[256];
    for (u32 i = 0; i < 256; ++i) identify[i] = inw(ATA_DATA);
    sectors = (u32)identify[60] | ((u32)identify[61] << 16);
    return sectors != 0;
}

u32 ata_sector_count(void)
{
    return sectors;
}

static bool select_lba(u32 lba, u8 command)
{
    if (!sectors || lba >= sectors || lba > 0x0FFFFFFFu) return false;
    if (!wait_command_done()) return false;
    outb(ATA_DRIVE, (u8)(0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_delay();
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, (u8)lba);
    outb(ATA_LBA_MID, (u8)(lba >> 8));
    outb(ATA_LBA_HIGH, (u8)(lba >> 16));
    outb(ATA_COMMAND, command);
    return wait_data_ready();
}

bool ata_read_sector(u32 lba, void *buffer)
{
    if (!buffer || !select_lba(lba, 0x20u)) return false;
    u16 *words = (u16 *)buffer;
    for (u32 i = 0; i < 256; ++i) words[i] = inw(ATA_DATA);
    return wait_command_done();
}

bool ata_write_sector(u32 lba, const void *buffer)
{
    if (!buffer || !select_lba(lba, 0x30u)) return false;
    const u16 *words = (const u16 *)buffer;
    for (u32 i = 0; i < 256; ++i) outw(ATA_DATA, words[i]);
    outb(ATA_COMMAND, 0xE7u);
    return wait_command_done();
}
