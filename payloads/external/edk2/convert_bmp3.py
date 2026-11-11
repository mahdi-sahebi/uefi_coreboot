#!/usr/bin/env python3
"""Convert an EDK2 logo to the Windows BMP3 format with Pillow."""

import sys

from PIL import Image


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: convert_bmp3.py INPUT OUTPUT")

    with Image.open(sys.argv[1]) as image:
        if image.mode not in ("RGB", "RGBA"):
            image = image.convert("RGBA")
        image.save(sys.argv[2], format="BMP")


if __name__ == "__main__":
    main()
