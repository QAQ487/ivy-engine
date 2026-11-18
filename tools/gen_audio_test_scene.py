#!/usr/bin/env python3
"""Generate the audio-format test clips and testbed/rom/scene/scene_audio_formats_fly.json.

Verification scene for WAV loading (AudioManager::LoadWav, engine/audio/wav_parse.h).
Free-fly debug camera, no scripts. Eight pillars stand in a row along X, 20 m
apart. Each has a looping spatial Audio component (SFX bus) whose clip is a
different WAV variant. Clip k beeps k times every 1.6 s, so the number of
beeps tells you which pillar you hear; pitch rises along the row.

Fly along the row (A / D). Only the pillar in front of you is audible
(distanceScale 10). Expected: pillars 1-7 beep, pillar 8 (red) is silent and
logs "AudioManager: unsupported WAV format chunk" (malformed on purpose).

  1  pcm16_mono_22k.wav        PCM 16-bit mono 22050 Hz, 16-byte fmt
  2  pcm16_stereo_list.wav     PCM 16-bit stereo, odd-sized LIST chunk before data
  3  pcm16_junk_cbsize.wav     PCM 16-bit mono, 18-byte fmt with junk cbSize
  4  float32_mono.wav          IEEE float 32-bit mono
  5  ext_pcm16_stereo.wav      WAVE_FORMAT_EXTENSIBLE, PCM 16-bit stereo
  6  ext_pcm24_mono.wav        WAVE_FORMAT_EXTENSIBLE, PCM 24-bit mono
  7  中文檔名_測試.wav          PCM 16-bit mono, CJK file name (UTF-8 path)
  8  ext_bad_cbsize.wav        EXTENSIBLE with cbSize past the fmt chunk -> rejected

Run:  python3 tools/gen_audio_test_scene.py
"""
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTBED = ROOT / "testbed"
CLIP_DIR_ROM = "rom/sound/audio_test"
OUT_SCENE = TESTBED / "rom/scene/scene_audio_formats_fly.json"

SPACING = 20.0
DISTANCE_SCALE = 10.0
CAMERA_Z = -6.0
LOOP_SECONDS = 1.6
BEEP_SECONDS = 0.12
GAP_SECONDS = 0.08
FADE_SECONDS = 0.005
AMPLITUDE = 0.5

WAVE_FORMAT_PCM = 0x0001
WAVE_FORMAT_IEEE_FLOAT = 0x0003
WAVE_FORMAT_EXTENSIBLE = 0xFFFE
# KSDATAFORMAT_SUBTYPE_PCM {00000001-0000-0010-8000-00aa00389b71}, little-endian GUID bytes.
SUBTYPE_PCM = bytes.fromhex("0100000000001000800000aa00389b71")


def beeps(count, rate, freq):
    """Mono samples in [-1, 1]: `count` short beeps, then silence to LOOP_SECONDS."""
    total = int(round(LOOP_SECONDS * rate))
    beep = int(round(BEEP_SECONDS * rate))
    period = beep + int(round(GAP_SECONDS * rate))
    fade = max(1, int(round(FADE_SECONDS * rate)))
    out = [0.0] * total
    for b in range(count):
        start = b * period
        for i in range(beep):
            n = start + i
            if n >= total:
                break
            env = min(1.0, i / fade, (beep - 1 - i) / fade)
            out[n] = AMPLITUDE * env * math.sin(2.0 * math.pi * freq * i / rate)
    return out


def pcm16(samples, channels):
    frames = []
    for s in samples:
        v = int(round(max(-1.0, min(1.0, s)) * 32767))
        frames.append(struct.pack("<h", v) * channels)
    return b"".join(frames)


def pcm24(samples):
    out = bytearray()
    for s in samples:
        v = int(round(max(-1.0, min(1.0, s)) * 8388607))
        out += struct.pack("<i", v)[:3]
    return bytes(out)


def float32(samples):
    return b"".join(struct.pack("<f", s) for s in samples)


def fmt_basic(tag, channels, rate, bits, cb_size=None):
    align = channels * bits // 8
    body = struct.pack("<HHIIHH", tag, channels, rate, rate * align, align, bits)
    if cb_size is not None:
        body += struct.pack("<H", cb_size)
    return body


def fmt_extensible(channels, rate, container_bits, valid_bits, cb_size=22, mask=None):
    align = channels * container_bits // 8
    if mask is None:
        mask = 0x4 if channels == 1 else 0x3  # FRONT_CENTER / FRONT_LEFT|FRONT_RIGHT
    return (struct.pack("<HHIIHHH", WAVE_FORMAT_EXTENSIBLE, channels, rate, rate * align, align,
                        container_bits, cb_size)
            + struct.pack("<HI", valid_bits, mask) + SUBTYPE_PCM)


def chunk(tag, body):
    data = tag + struct.pack("<I", len(body)) + body
    return data + (b"\0" if len(body) & 1 else b"")


def riff(*chunks):
    payload = b"WAVE" + b"".join(chunks)
    return b"RIFF" + struct.pack("<I", len(payload)) + payload


def clips():
    """(file name, wav bytes, label) in pillar order."""
    r8 = 8000
    out = []

    s = beeps(1, 22050, 440.0)
    out.append(("pcm16_mono_22k.wav",
                riff(chunk(b"fmt ", fmt_basic(WAVE_FORMAT_PCM, 1, 22050, 16)),
                     chunk(b"data", pcm16(s, 1))),
                "1 PCM16 mono 22k"))

    s = beeps(2, r8, 494.0)
    out.append(("pcm16_stereo_list.wav",
                riff(chunk(b"fmt ", fmt_basic(WAVE_FORMAT_PCM, 2, r8, 16)),
                     # 15-byte LIST body ("INFO" + 3-byte ICMT): odd, so the parser
                     # must skip one pad byte before "data".
                     chunk(b"LIST", b"INFO" + b"ICMT" + struct.pack("<I", 3) + b"ivy"),
                     chunk(b"data", pcm16(s, 2))),
                "2 PCM16 stereo + odd LIST"))

    s = beeps(3, r8, 523.0)
    out.append(("pcm16_junk_cbsize.wav",
                riff(chunk(b"fmt ", fmt_basic(WAVE_FORMAT_PCM, 1, r8, 16, cb_size=0xBEEF)),
                     chunk(b"data", pcm16(s, 1))),
                "3 PCM16 junk cbSize"))

    s = beeps(4, r8, 587.0)
    out.append(("float32_mono.wav",
                riff(chunk(b"fmt ", fmt_basic(WAVE_FORMAT_IEEE_FLOAT, 1, r8, 32, cb_size=0)),
                     chunk(b"data", float32(s))),
                "4 float32 mono"))

    s = beeps(5, r8, 659.0)
    out.append(("ext_pcm16_stereo.wav",
                riff(chunk(b"fmt ", fmt_extensible(2, r8, 16, 16)),
                     chunk(b"data", pcm16(s, 2))),
                "5 EXTENSIBLE PCM16 stereo"))

    s = beeps(6, r8, 698.0)
    out.append(("ext_pcm24_mono.wav",
                riff(chunk(b"fmt ", fmt_extensible(1, r8, 24, 24)),
                     chunk(b"data", pcm24(s))),
                "6 EXTENSIBLE PCM24 mono"))

    s = beeps(7, r8, 784.0)
    out.append(("中文檔名_測試.wav",
                riff(chunk(b"fmt ", fmt_basic(WAVE_FORMAT_PCM, 1, r8, 16)),
                     chunk(b"data", pcm16(s, 1))),
                "7 PCM16 CJK file name"))

    s = beeps(8, r8, 880.0)
    out.append(("ext_bad_cbsize.wav",
                riff(chunk(b"fmt ", fmt_extensible(1, r8, 16, 16, cb_size=100)),
                     chunk(b"data", pcm16(s, 1))),
                "8 bad cbSize (silent)"))
    return out


def transform(x, y, z, scale=(1.0, 1.0, 1.0)):
    return {"SceneTransform": {
        "posX": x, "posY": y, "posZ": z, "rotX": 0.0, "rotY": 0.0, "rotZ": 0.0,
        "scaleX": scale[0], "scaleY": scale[1], "scaleZ": scale[2]}}


def node(name, comps, tag=0, children=None):
    return {"name": name, "tag": tag, "children": children or [], "components": comps}


def text(name, y, value, size=0.8):
    return {"type": "text", "name": name, "anchor": "TOP_LEFT",
            "position": {"x": 24, "y": y}, "pivot": {"x": 0, "y": 1}, "alignment": "LEFT",
            "text": value, "fontSize": size, "color": {"r": 1, "g": 1, "b": 1, "a": 1},
            "font": {"texture": "builtin:font/IBMPlexMono-Regular.ttf",
                     "charWidth": 32, "charHeight": 32, "charsPerRow": 16},
            "visible": True}


def build(clip_list):
    count = len(clip_list)
    x0 = 0.0
    nodes = [
        node("Directional Light", {
            **transform(0.0, 10.0, 0.0),
            "DirectionalLight": {"enabled": True, "castShadows": False,
                                 "color": [1.0, 1.0, 1.0], "intensity": 0.9,
                                 "specColor": [1.0, 1.0, 1.0], "specPower": 32.0}}),
        node("Ground", {
            **transform((count - 1) * SPACING / 2.0, -0.5, 0.0,
                        scale=((count + 1) * SPACING, 1.0, 40.0)),
            "visual": {"model": "rom/model/object/Box_Sand.fbx", "shader": "standard"}}, tag=2),
    ]
    for i, (file_name, _, label) in enumerate(clip_list):
        x = x0 + i * SPACING
        bad = file_name.startswith("ext_bad")
        model = "rom/model/object/Box_Red.fbx" if bad else "rom/model/object/Box_Blue.fbx"
        nodes.append(node(f"Pillar {label}", {
            **transform(x, 1.5, 0.0, scale=(1.0, 3.0, 1.0)),
            "visual": {"model": model, "shader": "standard"},
            "Audio": {"clipPath": f"{CLIP_DIR_ROM}/{file_name}", "bus": "SFX",
                      "volume": 1.0, "loop": True, "autoPlay": True,
                      "spatial": True, "distanceScale": DISTANCE_SCALE}}))
    legend = [text("Title", 20, "AUDIO FORMAT TEST - fly along the row with A / D", 1.0),
              text("Hint", 60, "Pillar k beeps k times. 1-7 must play; 8 (red) must stay silent.")]
    for i, (_, _, label) in enumerate(clip_list):
        legend.append(text(f"Clip{i + 1}", 100 + 34 * i, label))
    return {
        "version": 4,
        "settings": {
            "render": {"clearColor": [0.08, 0.09, 0.12, 1.0], "defaultShader": "standard",
                       "fogEnabled": False, "shadowQuality": "low", "skyboxEnabled": False},
            "debugCamera": {"mode": "fly", "enabledOnPlay": True, "yawDeg": 0.0,
                            "pitchDeg": -10.0, "position": {"x": x0, "y": 2.5, "z": CAMERA_Z}},
        },
        "nodes": nodes,
        "canvas": {"name": "scene", "elements": legend},
    }


def main():
    clip_list = clips()
    clip_dir = TESTBED / CLIP_DIR_ROM
    clip_dir.mkdir(parents=True, exist_ok=True)
    for file_name, data, _ in clip_list:
        (clip_dir / file_name).write_bytes(data)
        print(f"wrote {(clip_dir / file_name).relative_to(ROOT)} ({len(data)} bytes)")
    OUT_SCENE.write_text(json.dumps(build(clip_list), indent=2, ensure_ascii=False) + "\n",
                         encoding="utf-8")
    print(f"wrote {OUT_SCENE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
