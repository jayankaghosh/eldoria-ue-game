"""Trims leading/trailing silence and levels a 16-bit mono WAV in place (peaks at -1 dBFS, at most +15 dB).
usage: python3 Scripts/level_audio.py file.wav [...]"""

import math
import struct
import sys
import wave

for path in sys.argv[1:]:
    with wave.open(path) as w:
        rate, frames = w.getframerate(), w.readframes(w.getnframes())
    samples = list(struct.unpack(f"<{len(frames) // 2}h", frames))
    peak = max(1, max(abs(s) for s in samples))
    # everything quieter than 40 dB under the peak counts as silence at the ends
    threshold = peak * 0.01
    voiced = [i for i, s in enumerate(samples) if abs(s) > threshold]
    pad = int(rate * 0.15)
    start, end = (max(0, voiced[0] - pad), min(len(samples), voiced[-1] + pad)) if voiced else (0, len(samples))
    samples = samples[start:end]
    gain = min(10 ** (15 / 20), 32767 * 10 ** (-1 / 20) / peak)
    out = [max(-32768, min(32767, int(s * gain))) for s in samples]
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack(f"<{len(out)}h", *out))
    print(f"{path}: {len(out) / rate:.1f}s, gain {20 * math.log10(gain):+.1f} dB")
