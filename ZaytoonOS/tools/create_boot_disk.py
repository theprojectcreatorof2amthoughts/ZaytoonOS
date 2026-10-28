#!/usr/bin/env python3
import argparse
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile
import uuid

SECTOR_SIZE = 512
IMAGE_SIZE = 64 * 1024 * 1024
FIRST_PARTITION_LBA = 2048
PARTITION_TYPE_LINUX = 0x83


def run(*command: str) -> None:
    subprocess.run(command, check=True)


def write_mbr(image: pathlib.Path) -> None:
    total_sectors = IMAGE_SIZE // SECTOR_SIZE
    partition_sectors = total_sectors - FIRST_PARTITION_LBA
    with image.open("r+b") as output:
        mbr = bytearray(SECTOR_SIZE)
        mbr[446:462] = struct.pack(
            "<B3sB3sII",
            0x80,
            b"\x00\x02\x00",
            PARTITION_TYPE_LINUX,
            b"\xfe\xff\xff",
            FIRST_PARTITION_LBA,
            partition_sectors,
        )
        mbr[510:512] = b"\x55\xaa"
        output.write(mbr)


def format_partition(image: pathlib.Path, root: pathlib.Path, filesystem_uuid: str) -> None:
    partition_blocks = (IMAGE_SIZE // SECTOR_SIZE - FIRST_PARTITION_LBA) // 2
    run(
        "mke2fs",
        "-q",
        "-F",
        "-t",
        "ext2",
        "-b",
        "1024",
        "-U",
        filesystem_uuid,
        "-E",
        f"offset={FIRST_PARTITION_LBA * SECTOR_SIZE}",
        "-d",
        str(root),
        str(image),
        str(partition_blocks),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a bootable ZaytoonOS hard-drive image.")
    parser.add_argument("--overwrite", action="store_true", help="replace the generated output image")
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("kernel", type=pathlib.Path)
    parser.add_argument("grub_config", type=pathlib.Path)
    args = parser.parse_args()

    required_tools = ("grub-install", "mke2fs")
    missing = [tool for tool in required_tools if shutil.which(tool) is None]
    if missing:
        parser.error("missing required tools: " + ", ".join(missing))
    for path in (args.kernel, args.grub_config):
        if not path.is_file():
            parser.error(f"input file not found: {path}")
    if args.output.exists() and not args.overwrite:
        parser.error(f"refusing to overwrite existing image: {args.output}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as temp_dir:
        temp = pathlib.Path(temp_dir)
        image = temp / "disk.img"
        root = temp / "root"
        boot = root / "boot"
        grub_dir = boot / "grub"
        grub_dir.mkdir(parents=True)
        shutil.copyfile(args.kernel, boot / "krnl.elf")
        shutil.copyfile(args.grub_config, grub_dir / "grub.cfg")

        with image.open("wb") as output:
            output.truncate(IMAGE_SIZE)
        write_mbr(image)

        filesystem_uuid = str(uuid.uuid4())
        format_partition(image, root, filesystem_uuid)
        run(
            "grub-install",
            "--target=i386-pc",
            f"--boot-directory={boot}",
            "--modules=part_msdos ext2 multiboot2",
            "--recheck",
            "--skip-fs-probe",
            "--force",
            str(image),
        )
        format_partition(image, root, filesystem_uuid)
        run(
            "grub-install",
            "--target=i386-pc",
            f"--boot-directory={boot}",
            "--modules=part_msdos ext2 multiboot2",
            "--recheck",
            "--skip-fs-probe",
            "--force",
            str(image),
        )
        os.replace(image, args.output)

    print(f"Created bootable disk image: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
