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

## Build

From the `ZaytoonOS` directory:

    make

This produces `zaytoonos.elf` from the kernel and root entry point.

## noVNC

A local noVNC setup is available in `novnc/`.

To launch a virtual X desktop and expose it through noVNC:

    cd novnc
    ./start_novnc.sh

Then open the forwarded browser URL for port `6080`.

If your workspace requires a specific VNC target instead of the built-in virtual desktop, use:
```bash
    cd novnc
    ./utils/novnc_proxy --vnc localhost:5901 --listen 6080
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

## NOTE!!1!
if you want to compile the .elf into an iso first put it in the boot folder in iso (NOT IN THE GRUB FOLDER) then run

```bash
grub-mkrescue -o /workspaces/ZaytoonOS/zaytoonos.iso /workspaces/ZaytoonOS/iso

```
IF you have ```grub-common``` and it's packages

if you don't have ```grub-common``` or the other packages run
```bash
sudo apt install -y grub-common grub2-common xorriso grub-pc-bin python3
```
in linux

## WINDOWS NOTE
if you want to use windows, install WSL in powershell from 
```bash 
wsl -install                                                                                 
```
because compiling is only on linux
