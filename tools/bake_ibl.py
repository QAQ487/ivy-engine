"""Offline Karis split-sum IBL baker.

Reads an LDR cubemap DDS, writes irradiance + GGX-prefiltered specular cubes
and a BRDF LUT PNG. Does not reuse the display cubemap at runtime.
"""
from __future__ import annotations

import math
import os
import struct
import sys
import zlib

PI = math.pi
SRC_SIZE = 256
IRRADIANCE_SIZE = 32
PREFILTER_SIZE = 128
PREFILTER_MIPS = 5
LUT_SIZE = 256
IRRADIANCE_SAMPLES = 64
PREFILTER_SAMPLES = 64
LUT_SAMPLES = 64

DDSD_CAPS = 0x1
DDSD_HEIGHT = 0x2
DDSD_WIDTH = 0x4
DDSD_PITCH = 0x8
DDSD_PIXELFORMAT = 0x1000
DDSD_MIPMAPCOUNT = 0x20000
DDSCAPS_COMPLEX = 0x8
DDSCAPS_MIPMAP = 0x400000
DDSCAPS_TEXTURE = 0x1000
DDSCAPS2_CUBEMAP = 0x200
DDSCAPS2_CUBEMAP_ALL = 0x200 | 0x400 | 0x800 | 0x1000 | 0x2000 | 0x4000 | 0x8000


def radical_inverse_vdc(bits: int) -> float:
    bits = (bits << 16 | bits >> 16) & 0xFFFFFFFF
    bits = ((bits & 0x55555555) << 1 | (bits & 0xAAAAAAAA) >> 1) & 0xFFFFFFFF
    bits = ((bits & 0x33333333) << 2 | (bits & 0xCCCCCCCC) >> 2) & 0xFFFFFFFF
    bits = ((bits & 0x0F0F0F0F) << 4 | (bits & 0xF0F0F0F0) >> 4) & 0xFFFFFFFF
    bits = ((bits & 0x00FF00FF) << 8 | (bits & 0xFF00FF00) >> 8) & 0xFFFFFFFF
    return bits * 2.3283064365386963e-10


def hammersley(i: int, n: int) -> tuple[float, float]:
    return (i / float(n), radical_inverse_vdc(i))


def saturate(x: float) -> float:
    return 0.0 if x < 0.0 else (1.0 if x > 1.0 else x)


def normalize(v: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = v
    length = math.sqrt(x * x + y * y + z * z)
    if length < 1e-8:
        return (0.0, 1.0, 0.0)
    inv = 1.0 / length
    return (x * inv, y * inv, z * inv)


def face_uv_to_dir(face: int, u: float, v: float) -> tuple[float, float, float]:
    uc = 2.0 * u - 1.0
    vc = 2.0 * v - 1.0
    if face == 0:
        return normalize((1.0, -vc, -uc))
    if face == 1:
        return normalize((-1.0, -vc, uc))
    if face == 2:
        return normalize((uc, 1.0, vc))
    if face == 3:
        return normalize((uc, -1.0, -vc))
    if face == 4:
        return normalize((uc, -vc, 1.0))
    return normalize((-uc, -vc, -1.0))


def dir_to_face_uv(dx: float, dy: float, dz: float) -> tuple[int, float, float]:
    ax, ay, az = abs(dx), abs(dy), abs(dz)
    if ax >= ay and ax >= az:
        if dx >= 0.0:
            face, sc, tc, ma = 0, -dz, -dy, ax
        else:
            face, sc, tc, ma = 1, dz, -dy, ax
    elif ay >= ax and ay >= az:
        if dy >= 0.0:
            face, sc, tc, ma = 2, dx, dz, ay
        else:
            face, sc, tc, ma = 3, dx, -dz, ay
    else:
        if dz >= 0.0:
            face, sc, tc, ma = 4, dx, -dy, az
        else:
            face, sc, tc, ma = 5, -dx, -dy, az
    ma = max(ma, 1e-8)
    return face, saturate(0.5 * (sc / ma + 1.0)), saturate(0.5 * (tc / ma + 1.0))


def sample_face(face: list[float], size: int, u: float, v: float) -> tuple[float, float, float]:
    x = saturate(u) * (size - 1)
    y = saturate(v) * (size - 1)
    x0 = int(math.floor(x))
    y0 = int(math.floor(y))
    x1 = min(x0 + 1, size - 1)
    y1 = min(y0 + 1, size - 1)
    tx = x - x0
    ty = y - y0

    def fetch(px: int, py: int) -> tuple[float, float, float]:
        i = (py * size + px) * 3
        return (face[i], face[i + 1], face[i + 2])

    c00 = fetch(x0, y0)
    c10 = fetch(x1, y0)
    c01 = fetch(x0, y1)
    c11 = fetch(x1, y1)
    return (
        (c00[0] * (1 - tx) + c10[0] * tx) * (1 - ty) + (c01[0] * (1 - tx) + c11[0] * tx) * ty,
        (c00[1] * (1 - tx) + c10[1] * tx) * (1 - ty) + (c01[1] * (1 - tx) + c11[1] * tx) * ty,
        (c00[2] * (1 - tx) + c10[2] * tx) * (1 - ty) + (c01[2] * (1 - tx) + c11[2] * tx) * ty,
    )


def sample_cube(faces: list[list[float]], size: int, d: tuple[float, float, float]) -> tuple[float, float, float]:
    face, u, v = dir_to_face_uv(d[0], d[1], d[2])
    return sample_face(faces[face], size, u, v)


def tangent_basis(n: tuple[float, float, float]) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    if abs(n[1]) < 0.999:
        up = (0.0, 1.0, 0.0)
    else:
        up = (0.0, 0.0, 1.0)
    tx = n[1] * up[2] - n[2] * up[1]
    ty = n[2] * up[0] - n[0] * up[2]
    tz = n[0] * up[1] - n[1] * up[0]
    tangent = normalize((tx, ty, tz))
    bx = n[1] * tangent[2] - n[2] * tangent[1]
    by = n[2] * tangent[0] - n[0] * tangent[2]
    bz = n[0] * tangent[1] - n[1] * tangent[0]
    return tangent, (bx, by, bz)


def importance_sample_ggx(xi: tuple[float, float], n: tuple[float, float, float], roughness: float):
    a = max(roughness * roughness, 1e-4)
    phi = 2.0 * PI * xi[0]
    cos_theta = math.sqrt((1.0 - xi[1]) / (1.0 + (a * a - 1.0) * xi[1]))
    sin_theta = math.sqrt(max(1.0 - cos_theta * cos_theta, 0.0))
    h = (math.cos(phi) * sin_theta, math.sin(phi) * sin_theta, cos_theta)
    tangent, bitangent = tangent_basis(n)
    return normalize((
        tangent[0] * h[0] + bitangent[0] * h[1] + n[0] * h[2],
        tangent[1] * h[0] + bitangent[1] * h[1] + n[1] * h[2],
        tangent[2] * h[0] + bitangent[2] * h[1] + n[2] * h[2],
    ))


def geometry_schlick_ggx_ibl(nv: float, roughness: float) -> float:
    a = roughness
    k = (a * a) / 2.0
    return nv / (nv * (1.0 - k) + k + 1e-5)


def geometry_smith_ibl(n: tuple[float, float, float], v: tuple[float, float, float], l: tuple[float, float, float], roughness: float) -> float:
    nv = saturate(n[0] * v[0] + n[1] * v[1] + n[2] * v[2])
    nl = saturate(n[0] * l[0] + n[1] * l[1] + n[2] * l[2])
    return geometry_schlick_ggx_ibl(nv, roughness) * geometry_schlick_ggx_ibl(nl, roughness)


def empty_face(size: int) -> list[float]:
    return [0.0] * (size * size * 3)


def make_cube(size: int) -> list[list[float]]:
    return [empty_face(size) for _ in range(6)]


def set_texel(face: list[float], size: int, x: int, y: int, rgb: tuple[float, float, float]) -> None:
    i = (y * size + x) * 3
    face[i] = rgb[0]
    face[i + 1] = rgb[1]
    face[i + 2] = rgb[2]


def load_dds_cube(path: str) -> tuple[int, list[bytes], tuple[int, int, int, int]]:
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"DDS ":
        raise ValueError("not a DDS file: " + path)
    height = struct.unpack_from("<I", data, 12)[0]
    width = struct.unpack_from("<I", data, 16)[0]
    mip_count = max(struct.unpack_from("<I", data, 28)[0], 1)
    rmask = struct.unpack_from("<I", data, 92)[0]
    gmask = struct.unpack_from("<I", data, 96)[0]
    bmask = struct.unpack_from("<I", data, 100)[0]
    amask = struct.unpack_from("<I", data, 104)[0]
    fourcc = data[84:88]
    if fourcc == b"DX10":
        raise ValueError("DX10 DDS not supported by this baker")
    if width != height:
        raise ValueError("cubemap faces must be square")

    def shift_of(mask: int) -> int:
        if mask == 0:
            return 0
        shift = 0
        while mask and (mask & 1) == 0:
            mask >>= 1
            shift += 1
        return shift

    shifts = (shift_of(rmask), shift_of(gmask), shift_of(bmask), shift_of(amask))
    header_size = 128
    face_bytes = width * height * 4
    # Skip mips after the top level of each face.
    faces = []
    offset = header_size
    remaining = width
    mip_sizes = []
    for _ in range(mip_count):
        mip_sizes.append(remaining * remaining * 4)
        remaining = max(remaining // 2, 1)
    for _face in range(6):
        faces.append(data[offset:offset + face_bytes])
        offset += sum(mip_sizes)
        if len(faces[-1]) != face_bytes:
            raise ValueError("truncated cubemap DDS")
    return width, faces, shifts


def decode_bgra(pixel: bytes, shifts: tuple[int, int, int, int]) -> tuple[int, int, int]:
    value = struct.unpack_from("<I", pixel)[0]
    r = (value >> shifts[0]) & 0xFF
    g = (value >> shifts[1]) & 0xFF
    b = (value >> shifts[2]) & 0xFF
    return r, g, b


def sample_raw_face(src: bytes, src_size: int, u: float, v: float, shifts: tuple[int, int, int, int]) -> tuple[float, float, float]:
    x = saturate(u) * (src_size - 1)
    y = saturate(v) * (src_size - 1)
    x0 = int(math.floor(x))
    y0 = int(math.floor(y))
    x1 = min(x0 + 1, src_size - 1)
    y1 = min(y0 + 1, src_size - 1)
    tx = x - x0
    ty = y - y0

    def fetch(px: int, py: int) -> tuple[float, float, float]:
        i = (py * src_size + px) * 4
        r, g, b = decode_bgra(src[i:i + 4], shifts)
        return (r / 255.0, g / 255.0, b / 255.0)

    c00 = fetch(x0, y0)
    c10 = fetch(x1, y0)
    c01 = fetch(x0, y1)
    c11 = fetch(x1, y1)
    return (
        (c00[0] * (1 - tx) + c10[0] * tx) * (1 - ty) + (c01[0] * (1 - tx) + c11[0] * tx) * ty,
        (c00[1] * (1 - tx) + c10[1] * tx) * (1 - ty) + (c01[1] * (1 - tx) + c11[1] * tx) * ty,
        (c00[2] * (1 - tx) + c10[2] * tx) * (1 - ty) + (c01[2] * (1 - tx) + c11[2] * tx) * ty,
    )


def downsample_faces(raw_faces: list[bytes], src_size: int, dst_size: int, shifts: tuple[int, int, int, int]) -> list[list[float]]:
    out = make_cube(dst_size)
    for face in range(6):
        src = raw_faces[face]
        for y in range(dst_size):
            v = (y + 0.5) / dst_size
            for x in range(dst_size):
                u = (x + 0.5) / dst_size
                set_texel(out[face], dst_size, x, y, sample_raw_face(src, src_size, u, v, shifts))
    return out


def bake_irradiance(src: list[list[float]], src_size: int) -> list[list[float]]:
    dst = make_cube(IRRADIANCE_SIZE)
    for face in range(6):
        print(f"  irradiance face {face + 1}/6")
        for y in range(IRRADIANCE_SIZE):
            v = (y + 0.5) / IRRADIANCE_SIZE
            for x in range(IRRADIANCE_SIZE):
                u = (x + 0.5) / IRRADIANCE_SIZE
                n = face_uv_to_dir(face, u, v)
                tangent, bitangent = tangent_basis(n)
                acc = [0.0, 0.0, 0.0]
                weight = 0.0
                for i in range(IRRADIANCE_SAMPLES):
                    xi = hammersley(i, IRRADIANCE_SAMPLES)
                    cos_theta = math.sqrt(1.0 - xi[1])
                    sin_theta = math.sqrt(xi[1])
                    phi = 2.0 * PI * xi[0]
                    lx = math.cos(phi) * sin_theta
                    ly = math.sin(phi) * sin_theta
                    lz = cos_theta
                    l = normalize((
                        tangent[0] * lx + bitangent[0] * ly + n[0] * lz,
                        tangent[1] * lx + bitangent[1] * ly + n[1] * lz,
                        tangent[2] * lx + bitangent[2] * ly + n[2] * lz,
                    ))
                    ndotl = saturate(n[0] * l[0] + n[1] * l[1] + n[2] * l[2])
                    if ndotl <= 0.0:
                        continue
                    rgb = sample_cube(src, src_size, l)
                    acc[0] += rgb[0] * ndotl
                    acc[1] += rgb[1] * ndotl
                    acc[2] += rgb[2] * ndotl
                    weight += ndotl
                inv = 1.0 / max(weight, 1e-5)
                set_texel(dst[face], IRRADIANCE_SIZE, x, y, (acc[0] * inv, acc[1] * inv, acc[2] * inv))
    return dst


def bake_prefilter(src: list[list[float]], src_size: int) -> list[list[list[float]]]:
    mips: list[list[list[float]]] = []
    size = PREFILTER_SIZE
    for mip in range(PREFILTER_MIPS):
        roughness = 0.0 if PREFILTER_MIPS == 1 else mip / float(PREFILTER_MIPS - 1)
        print(f"  prefilter mip {mip + 1}/{PREFILTER_MIPS} size={size} roughness={roughness:.2f}")
        cube = make_cube(size)
        for face in range(6):
            for y in range(size):
                v = (y + 0.5) / size
                for x in range(size):
                    u = (x + 0.5) / size
                    n = face_uv_to_dir(face, u, v)
                    if roughness < 0.04:
                        set_texel(cube[face], size, x, y, sample_cube(src, src_size, n))
                        continue
                    acc = [0.0, 0.0, 0.0]
                    weight = 0.0
                    for i in range(PREFILTER_SAMPLES):
                        xi = hammersley(i, PREFILTER_SAMPLES)
                        h = importance_sample_ggx(xi, n, roughness)
                        vdotn = n[0] * n[0] + n[1] * n[1] + n[2] * n[2]
                        vdoth = h[0] * n[0] + h[1] * n[1] + h[2] * n[2]
                        l = normalize((
                            2.0 * vdoth * h[0] - n[0] * vdotn,
                            2.0 * vdoth * h[1] - n[1] * vdotn,
                            2.0 * vdoth * h[2] - n[2] * vdotn,
                        ))
                        ndotl = saturate(n[0] * l[0] + n[1] * l[1] + n[2] * l[2])
                        if ndotl <= 0.0:
                            continue
                        rgb = sample_cube(src, src_size, l)
                        acc[0] += rgb[0] * ndotl
                        acc[1] += rgb[1] * ndotl
                        acc[2] += rgb[2] * ndotl
                        weight += ndotl
                    inv = 1.0 / max(weight, 1e-5)
                    set_texel(cube[face], size, x, y, (acc[0] * inv, acc[1] * inv, acc[2] * inv))
        mips.append(cube)
        size = max(size // 2, 1)
    return mips


def bake_brdf_lut() -> bytes:
    pixels = bytearray()
    for y in range(LUT_SIZE):
        roughness = (y + 0.5) / LUT_SIZE
        print(f"  brdf lut row {y + 1}/{LUT_SIZE}") if y % 32 == 0 else None
        for x in range(LUT_SIZE):
            nov = max((x + 0.5) / LUT_SIZE, 1e-4)
            v = (math.sqrt(1.0 - nov * nov), 0.0, nov)
            n = (0.0, 0.0, 1.0)
            a = 0.0
            b = 0.0
            for i in range(LUT_SAMPLES):
                xi = hammersley(i, LUT_SAMPLES)
                h = importance_sample_ggx(xi, n, roughness)
                l = normalize((
                    2.0 * (v[0] * h[0] + v[1] * h[1] + v[2] * h[2]) * h[0] - v[0],
                    2.0 * (v[0] * h[0] + v[1] * h[1] + v[2] * h[2]) * h[1] - v[1],
                    2.0 * (v[0] * h[0] + v[1] * h[1] + v[2] * h[2]) * h[2] - v[2],
                ))
                ndotl = saturate(l[2])
                ndotv = saturate(v[2])
                ndoth = saturate(h[2])
                vdoth = saturate(v[0] * h[0] + v[1] * h[1] + v[2] * h[2])
                if ndotl <= 0.0:
                    continue
                g = geometry_smith_ibl(n, v, l, roughness)
                g_vis = (g * vdoth) / max(ndoth * ndotv, 1e-5)
                fc = pow(1.0 - vdoth, 5.0)
                a += (1.0 - fc) * g_vis
                b += fc * g_vis
            inv = 1.0 / float(LUT_SAMPLES)
            r = int(saturate(a * inv) * 255.0 + 0.5)
            g = int(saturate(b * inv) * 255.0 + 0.5)
            pixels.extend((r, g, 0))
    return bytes(pixels)


def write_png_rgb(path: str, width: int, height: int, rgb: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b""
    stride = width * 3
    for y in range(height):
        raw += b"\x00" + rgb[y * stride:(y + 1) * stride]
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def pack_rgb_face(face: list[float], size: int) -> bytes:
    out = bytearray(size * size * 4)
    for i in range(size * size):
        r = int(saturate(face[i * 3]) * 255.0 + 0.5)
        g = int(saturate(face[i * 3 + 1]) * 255.0 + 0.5)
        b = int(saturate(face[i * 3 + 2]) * 255.0 + 0.5)
        # BGRA8 masks.
        out[i * 4] = b
        out[i * 4 + 1] = g
        out[i * 4 + 2] = r
        out[i * 4 + 3] = 255
    return bytes(out)


def write_dds_cube(path: str, size: int, mips: list[list[list[float]]]) -> None:
    mip_count = len(mips)
    flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT
    caps = DDSCAPS_TEXTURE | DDSCAPS_COMPLEX
    if mip_count > 1:
        flags |= DDSD_MIPMAPCOUNT
        caps |= DDSCAPS_MIPMAP
    header = bytearray(128)
    struct.pack_into("<4s", header, 0, b"DDS ")
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<I", header, 8, flags)
    struct.pack_into("<I", header, 12, size)
    struct.pack_into("<I", header, 16, size)
    struct.pack_into("<I", header, 20, size * 4)
    struct.pack_into("<I", header, 24, 0)
    struct.pack_into("<I", header, 28, mip_count)
    struct.pack_into("<I", header, 76, 32)
    struct.pack_into("<I", header, 80, 0x41)
    struct.pack_into("<I", header, 88, 32)
    struct.pack_into("<I", header, 92, 0x00FF0000)
    struct.pack_into("<I", header, 96, 0x0000FF00)
    struct.pack_into("<I", header, 100, 0x000000FF)
    struct.pack_into("<I", header, 104, 0xFF000000)
    struct.pack_into("<I", header, 108, caps)
    struct.pack_into("<I", header, 112, DDSCAPS2_CUBEMAP_ALL)
    payload = bytearray()
    for face in range(6):
        for mip in mips:
            mip_size = int(math.sqrt(len(mip[face]) / 3))
            payload.extend(pack_rgb_face(mip[face], mip_size))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(header)
        f.write(payload)


def main() -> int:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_path = os.path.join(root, "resources", "skybox", "MySkybox.dds")
    out_dir = os.path.join(root, "resources", "skybox", "ibl")
    if len(sys.argv) >= 2:
        src_path = sys.argv[1]
    if len(sys.argv) >= 3:
        out_dir = sys.argv[2]

    print("Loading", src_path)
    src_size, raw_faces, shifts = load_dds_cube(src_path)
    print(f"Source cube {src_size}x{src_size}, downsample to {SRC_SIZE}")
    src = downsample_faces(raw_faces, src_size, SRC_SIZE, shifts)

    print("Baking irradiance")
    irradiance = bake_irradiance(src, SRC_SIZE)
    print("Baking prefilter")
    prefilter = bake_prefilter(src, SRC_SIZE)
    print("Baking BRDF LUT")
    lut = bake_brdf_lut()

    irr_path = os.path.join(out_dir, "irradiance.dds")
    pre_path = os.path.join(out_dir, "prefilter.dds")
    lut_path = os.path.join(out_dir, "brdf_lut.png")
    write_dds_cube(irr_path, IRRADIANCE_SIZE, [irradiance])
    write_dds_cube(pre_path, PREFILTER_SIZE, prefilter)
    write_png_rgb(lut_path, LUT_SIZE, LUT_SIZE, lut)
    print("Wrote", irr_path)
    print("Wrote", pre_path)
    print("Wrote", lut_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
