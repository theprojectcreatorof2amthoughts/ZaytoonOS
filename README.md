# ZaytoonOS

A minimalist x86_64 kernel with a modular runtime architecture and basic preemptive scheduling.

This version adds a stronger kernel foundation:

- formatted `printk` logging with `%s`, `%d`, `%x`, `%p`, and more
- VGA text console with scrolling, cursor updates, and tab handling
- a kernel heap allocator based on linker-provided heap symbols
- PIC remapping and timer-driven IRQ scheduling
- a lightweight task subsystem with context switching
- improved build flags for bare-metal compilation
- buffered PS/2 keyboard input and a native C `.capp` command shell

## Build and boot

From the `ZaytoonOS` directory:

    make

This builds the 32-bit kernel as `krnl.elf`. To create a bootable GRUB2 ISO
containing the kernel, run:

    make install-image

The generated `zaytoonos.iso` can boot ZaytoonOS from a virtual CD or USB
drive. The target stages the built kernel and the repository's GRUB
configuration automatically; it does not write to a physical drive. To boot
the ISO in QEMU with the persistent app data disk attached, run:

    make run-iso

The ISO build requires `grub-mkrescue` and `xorriso` in addition to the kernel
build tools.

To build a bootable raw hard-drive image containing ZaytoonOS, run:

    make disk-image

The generated `ZaytoonOS/zaytoonos-hdd.img` includes a GRUB BIOS bootloader,
an ext2 system partition, and the current kernel. Test it in QEMU with:

    make run-disk-image

This target only creates an image file; it never writes to a physical disk.
Writing that image to a real drive replaces its partition table and contents,
so verify the destination device carefully and keep a backup. The image build
requires `grub-install`, the GRUB i386-pc modules, and `mke2fs`.

## noVNC

A local noVNC setup is available in `novnc/`.

To launch a virtual X desktop and expose it through noVNC:

    cd novnc
    ./start_novnc.sh

Then open the forwarded browser URL for port `6080`.

If your workspace requires a specific VNC target instead of the built-in virtual desktop, use:
```bash
    cd novnc
8
```
All of this is if you want to run ```qemu``` in Github CodeSpaces, YOU DO NOT HAVE TO DO THIS IN WINDOWS OR GUI LINUX!
## Structure

- `kernel/` — kernel subsystem sources
- `appcompiler/` - the `commandline.capp` compiling folder
- `tools/` - the tools used
- `maincompiler.c` — root startup wrapper that begins kernel execution
- `Makefile` — build rules for the OS image
- `linker.ld` — bare-metal linker layout for the kernel

## Keyboard and `.capp` shell

The keyboard driver handles PS/2 IRQ1 scancodes and buffers translated
characters for the shell and applications. In headless QEMU runs, shell input
is also read from COM1 so typing into the `make run` terminal works. The boot
shell is the native C application `shell.capp`, implemented in
[`ZaytoonOS/kernel/apps/shell.c`](./ZaytoonOS/kernel/apps/shell.c) and launched
through the `.capp` runtime. Native C apps are compiled into the kernel and
registered with that runtime; the appcompiler's packaged `.capp` files use the
separate record-based format.

## Number guessing game and hard-drive app storage

`number_guess.capp` is an interactive number-guessing game, built from
`ZaytoonOS/appcompiler/number_guess.c`. Run it directly from the shell with:

    number_guess.capp

`make run` creates and attaches a dedicated 16 MiB QEMU IDE data disk at
`ZaytoonOS/zaytoonos-data.img`. This is persistent app storage, not a
partitioner or a full-OS installer. The kernel mounts only a compatible
ZaytoonOS app volume; missing, unformatted, or incompatible disks are left
untouched.

Install the game from the shell:

    INSTALL number_guess.capp

List installed apps with `DIR HDD:/APPS`, then launch the installed copy:

    RUN HDD:/APPS/number_guess.capp

The image is retained across runs, and `make clean` does not remove it. To
create a separate data disk manually, use
`python3 ZaytoonOS/tools/create_data_disk.py <image-path>`; the tool refuses
to overwrite an existing image.

## ZayFlight simulator

`zayflight.capp` is a packaged interactive text flight simulator. Build from
the `ZaytoonOS/` directory, boot the OS, and enter this command at the shell:

    zayflight.capp

Use W/S to pitch, A/D to bank, +/- to change throttle, F to toggle flaps, G
to toggle landing gear, H to level the aircraft, and Q to exit. Each command
advances the simulation by five seconds. The simulator is packaged into the
kernel along with the other `.capp` applications.

The keyboard driver supports both PS/2 input (including v86) and COM1 serial
input for headless QEMU sessions.

## Windows note

The kernel, ISO, and hard-drive image builds require Linux tools. On Windows,
use WSL and install the required packages:

```bash
sudo apt install -y build-essential grub-common grub2-common grub-pc-bin xorriso e2fsprogs python3
```

Install WSL from PowerShell with `wsl --install` if it is not already set up.
