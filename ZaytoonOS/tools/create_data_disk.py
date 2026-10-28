#!/usr/bin/env python3
import pathlib
import struct
import sys

SECTOR_SIZE = 512
IMAGE_SIZE = 16 * 1024 * 1024
ENTRY_COUNT = 12
SLOT_SECTORS = 128
STORE_MAGIC = 0x3153445A


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: create_data_disk.py OUTPUT.img", file=sys.stderr)
        return 2

    image_path = pathlib.Path(sys.argv[1])
    try:
        with image_path.open("xb") as image:
            image.truncate(IMAGE_SIZE)
            metadata = struct.pack(
                "<IHHHHI",
                STORE_MAGIC,
                1,
                ENTRY_COUNT,
                SLOT_SECTORS,
                0,
                IMAGE_SIZE // SECTOR_SIZE,
            )
            image.write(metadata)
            image.write(bytes(SECTOR_SIZE - len(metadata)))
    except FileExistsError:
        print(f"refusing to overwrite existing disk image: {image_path}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
