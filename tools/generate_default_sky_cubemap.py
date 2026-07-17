"""Generate a simple RGBA cubemap DDS for engine default sky content."""
import os
import struct

SIZE = 64
FACE_COLORS = [
    (0.45, 0.62, 0.92),
    (0.40, 0.58, 0.88),
    (0.55, 0.72, 0.95),
    (0.18, 0.24, 0.38),
    (0.42, 0.60, 0.90),
    (0.38, 0.56, 0.86),
]


def face_pixels(face_index: int) -> bytes:
    base_r, base_g, base_b = FACE_COLORS[face_index]
    pixels = bytearray()
    for y in range(SIZE):
        t = y / max(SIZE - 1, 1)
        for _x in range(SIZE):
            if face_index == 2:
                r, g, b = 0.70 + 0.20 * t, 0.82 + 0.10 * t, 0.98
            elif face_index == 3:
                r, g, b = base_r, base_g, base_b
            else:
                r = base_r * (0.85 + 0.15 * t)
                g = base_g * (0.85 + 0.15 * t)
                b = base_b * (0.85 + 0.15 * t)
            pixels.extend(
                (
                    int(min(max(b * 255.0, 0.0), 255.0)),
                    int(min(max(g * 255.0, 0.0), 255.0)),
                    int(min(max(r * 255.0, 0.0), 255.0)),
                    255,
                )
            )
    return bytes(pixels)


def write_dds(path: str) -> None:
    pixel_data = b"".join(face_pixels(i) for i in range(6))
    header = bytearray(128)
    struct.pack_into("<4s", header, 0, b"DDS ")
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<I", header, 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000)
    struct.pack_into("<I", header, 12, SIZE)
    struct.pack_into("<I", header, 16, SIZE)
    struct.pack_into("<I", header, 20, SIZE * SIZE * 4)
    struct.pack_into("<I", header, 24, 0)
    struct.pack_into("<I", header, 28, 1)
    struct.pack_into("<I", header, 76, 32)
    struct.pack_into("<I", header, 80, 0x41)
    struct.pack_into("<I", header, 84, 0)
    struct.pack_into("<I", header, 88, 32)
    struct.pack_into("<I", header, 92, 0x00FF0000)
    struct.pack_into("<I", header, 96, 0x0000FF00)
    struct.pack_into("<I", header, 100, 0x000000FF)
    struct.pack_into("<I", header, 104, 0xFF000000)
    struct.pack_into("<I", header, 108, 0x1000 | 0x8)
    struct.pack_into("<I", header, 112, 0x200 | 0x400 | 0x800 | 0x1000 | 0x2000 | 0x4000)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(header)
        f.write(pixel_data)


if __name__ == "__main__":
    out = os.path.join("rom", "texture", "sky", "default_sky.dds")
    write_dds(out)
    print(f"Wrote {out}")
