"""Synthesises Eldoria's sound effects as WAV files (pure Python, no dependencies).

Usage: python3 Scripts/make_sounds.py   -> writes Audio/Source/*.wav
They are imported into the project with Scripts/import_sounds.py.
"""

import math
import os
import random
import struct
import wave

RATE = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "Audio", "Source")
random.seed(7)


def write(name, samples, rate=RATE):
    os.makedirs(OUT, exist_ok=True)
    peak = max(1e-9, max(abs(s) for s in samples))
    gain = 0.89 / peak
    with wave.open(os.path.join(OUT, f"{name}.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s * gain)) * 32767)) for s in samples))
    print(f"wrote {name}.wav ({len(samples) / rate:.2f}s)")


class Biquad:
    """RBJ cookbook filter; freq can be changed per sample."""

    def __init__(self, kind, freq, q=0.707, rate=RATE):
        self.kind, self.q, self.rate = kind, q, rate
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0
        self.set(freq)

    def set(self, freq):
        w = 2 * math.pi * min(freq, self.rate * 0.45) / self.rate
        a = math.sin(w) / (2 * self.q)
        c = math.cos(w)
        if self.kind == "lp":
            b0, b1, b2 = (1 - c) / 2, 1 - c, (1 - c) / 2
        elif self.kind == "hp":
            b0, b1, b2 = (1 + c) / 2, -(1 + c), (1 + c) / 2
        else:  # band-pass
            b0, b1, b2 = a, 0.0, -a
        a0, a1, a2 = 1 + a, -2 * c, 1 - a
        self.b = (b0 / a0, b1 / a0, b2 / a0)
        self.a = (a1 / a0, a2 / a0)

    def __call__(self, x):
        b0, b1, b2 = self.b
        a1, a2 = self.a
        y = b0 * x + b1 * self.x1 + b2 * self.x2 - a1 * self.y1 - a2 * self.y2
        self.x2, self.x1, self.y2, self.y1 = self.x1, x, self.y1, y
        return y


def noise():
    return random.uniform(-1, 1)


def env(t, attack, decay):
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) / decay)


def secs(d, rate=RATE):
    return range(int(d * rate))


# ---------- combat ----------

def sword_swing(peak=0.12, width=0.035, bright=1.0, seed=0):
    """Blade swish: short and snappy. Noise whose band sweeps up as the blade approaches and down
    as it passes (a doppler-like curve), run through a moving comb filter that gives the airy,
    edge-cutting texture of a real swing. No tonal parts."""
    random.seed(100 + seed)
    band = Biquad("bp", 800, q=0.9)
    air = Biquad("hp", 1200)
    body = Biquad("lp", 300, q=0.7)
    dur = 0.32
    n = int(dur * RATE)
    delay_line = [0.0] * 2048
    out = []
    for i in range(n):
        t = i / RATE
        # speed of the blade: sharp rise, slightly longer tail
        w = width if t < peak else width * 2.2
        v = math.exp(-((t - peak) / w) ** 2)
        # doppler-style sweep: centre frequency climbs to the peak, then drops after it
        d = (t - peak) / (width * 3)
        centre = 700 + 2600 * bright * v * (1.0 - 0.35 * max(0.0, d))
        band.set(centre)
        x = noise()
        swish = band(x) * 1.2 + air(x) * 0.35 * v + body(noise()) * 0.6 * v
        # comb filter with a sweeping delay (0.4-2.5 ms): the "shhh-vvv" edge texture
        delay = int((0.0004 + 0.0021 * (1 - v)) * RATE)
        delayed = delay_line[(i - delay) % 2048]
        y = swish + 0.7 * delayed
        delay_line[i % 2048] = swish
        out.append(y * v ** 1.4)
    return out


def flesh_hit(seed=0, weight=1.0):
    """Blade striking a body: a low thud, a sharp thwack and a short wet slice.
    Built only from shaped noise, with no ringing tones (those read as metal)."""
    random.seed(300 + seed)
    thud_lp = Biquad("lp", 140, q=0.9)
    thwack = Biquad("bp", 950 + seed * 180, q=1.4)
    slice_hp = Biquad("hp", 2600)
    wet = Biquad("bp", 420, q=2.2)
    out = []
    for i in secs(0.42):
        t = i / RATE
        # body mass: noise through a resonant low-pass, pitch-dropping punch
        thud = thud_lp(noise()) * env(t, 0.002, 0.07 * weight) * 3.2
        punch = math.sin(2 * math.pi * (95 - 50 * min(t / 0.06, 1)) * t) * env(t, 0.001, 0.045) * 0.9
        strike = thwack(noise()) * env(t, 0.0008, 0.028) * 1.6
        cut = slice_hp(noise()) * env(t, 0.0005, 0.018) * 0.7
        # a short, slightly wobbling mid layer gives the wet, fleshy texture
        wet.set(380 + 160 * math.sin(2 * math.pi * 31 * t))
        squelch = wet(noise()) * env(t, 0.006, 0.06) * 0.8
        out.append(thud + punch + strike + cut + squelch)
    return out


def shield_block():
    partials = ((523, 1.0, 0.7), (1187, 0.7, 0.45), (1893, 0.5, 0.3), (2711, 0.35, 0.2), (3530, 0.25, 0.12), (4410, 0.15, 0.08))
    hp = Biquad("hp", 2500)
    out = []
    for i in secs(1.1):
        t = i / RATE
        clang = sum(a * math.sin(2 * math.pi * f * t + f) * env(t, 0.001, d) for f, a, d in partials)
        click = hp(noise()) * env(t, 0.0005, 0.012) * 1.5
        out.append(clang * 0.6 + click)
    return out


def footstep():
    lp = Biquad("lp", 900)
    out = []
    for i in secs(0.16):
        t = i / RATE
        grit = lp(noise()) * env(t, 0.004, 0.035)
        thud = math.sin(2 * math.pi * 85 * t) * env(t, 0.002, 0.03) * 0.6
        out.append(grit + thud)
    return out


def enemy_death():
    lp = Biquad("lp", 1200)
    out = []
    for i in secs(1.2):
        t = i / RATE
        body = lp(noise()) * env(t, 0.01, 0.18)
        clanks = 0.0
        for start, f in ((0.0, 690), (0.18, 910), (0.33, 560)):
            if t >= start:
                clanks += math.sin(2 * math.pi * f * (t - start)) * env(t - start, 0.001, 0.09) * 0.35
        thump = math.sin(2 * math.pi * 55 * t) * env(t, 0.01, 0.2)
        out.append(body + clanks + thump)
    return out


def boss_roar():
    lp = Biquad("lp", 500, q=1.2)
    lp2 = Biquad("lp", 900)
    out = []
    phase = 0.0
    dur = 2.4
    for i in secs(dur):
        t = i / RATE
        f = 62 + 30 * math.sin(math.pi * min(1, t / 0.6)) - 18 * max(0, t - 1.2)
        f += 6 * math.sin(2 * math.pi * 7 * t)
        phase += f / RATE
        saw = 2 * (phase % 1.0) - 1
        growl = lp(saw * (0.7 + 0.3 * math.sin(2 * math.pi * 23 * t)))
        breath = lp2(noise()) * 0.5
        shape = min(1, t / 0.25) * (1 if t < 1.6 else math.exp(-(t - 1.6) / 0.35))
        out.append((growl + breath) * shape)
    return out


# ---------- pickups and UI ----------

def coin_clatter(seed=0, clinks=2):
    """Coins knocking together: two or three tiny, very short metallic clinks a few
    milliseconds apart, each with slightly different (inharmonic) partials, plus a grain of noise."""
    random.seed(500 + seed)
    hp = Biquad("hp", 3000)
    out = [0.0] * int(0.45 * RATE)
    start = 0.0
    for c in range(clinks):
        base = random.uniform(2300, 3100)
        partials = [(base * r, a, d) for r, a, d in ((1.0, 1.0, 0.09), (2.31, 0.6, 0.06), (3.79, 0.4, 0.04), (5.12, 0.25, 0.03))]
        amp = 1.0 if c == 0 else random.uniform(0.45, 0.75)
        for i in range(int(0.16 * RATE)):
            t = i / RATE
            k = int((start + t) * RATE)
            if k >= len(out):
                break
            ring = sum(a * math.sin(2 * math.pi * f * t + f) * math.exp(-t / d) for f, a, d in partials)
            tick = hp(noise()) * math.exp(-t / 0.004) * 0.8
            out[k] += (ring * 0.5 + tick) * amp
        start += random.uniform(0.035, 0.08)
    return out


def potion():
    out = []
    for i in secs(0.9):
        t = i / RATE
        s = 0.0
        for k in range(7):
            start = k * 0.1 + random.uniform(0, 0.03)
            if t >= start:
                u = t - start
                f = 380 + k * 70 + 900 * u
                s += math.sin(2 * math.pi * f * u) * env(u, 0.003, 0.04)
        out.append(s)
    return out


def chest():
    lp = Biquad("lp", 700, q=4)
    out = []
    phase = 0.0
    for i in secs(1.6):
        t = i / RATE
        creak = 0.0
        if t < 0.6:
            f = 95 + 60 * t + 15 * math.sin(2 * math.pi * 11 * t)
            phase += f / RATE
            creak = lp((2 * (phase % 1.0) - 1) * (0.5 + 0.5 * random.random())) * math.sin(math.pi * t / 0.6)
        chime = 0.0
        for k, f in enumerate((784, 988, 1175, 1568)):
            start = 0.55 + k * 0.08
            if t >= start:
                chime += math.sin(2 * math.pi * f * (t - start)) * env(t - start, 0.003, 0.3) * 0.5
        out.append(creak + chime)
    return out


def quest_fanfare():
    notes = ((0.0, 293.66), (0.16, 369.99), (0.32, 440.0), (0.5, 587.33))
    lp = Biquad("lp", 2400)
    out = []
    for i in secs(2.2):
        t = i / RATE
        s = 0.0
        for start, f in notes:
            if t >= start:
                u = t - start
                hold = 1.0 if start == 0.5 else 0.0
                e = env(u, 0.02, 0.9 if hold else 0.35)
                s += sum(math.sin(2 * math.pi * f * h * u) / h for h in (1, 2, 3, 4)) * e * 0.4
        out.append(lp(s))
    return out


# ---------- ambience ----------

def ambience():
    rate = 22050
    dur = 30.0
    wind_lp = Biquad("lp", 400, q=0.6, rate=rate)
    wind_bp = Biquad("bp", 300, q=0.8, rate=rate)
    n = int(dur * rate)
    out = [0.0] * n
    for i in range(n):
        t = i / rate
        gust = 0.55 + 0.25 * math.sin(2 * math.pi * t / dur * 3) + 0.2 * math.sin(2 * math.pi * t / dur * 7 + 1)
        wind_bp.set(220 + 260 * gust)
        out[i] = (wind_lp(noise()) * 0.8 + wind_bp(noise()) * 0.5) * gust * 0.5
    # birdsong: short frequency-modulated chirps scattered through the loop
    for _ in range(26):
        start = random.uniform(0.5, dur - 1.5)
        base = random.uniform(2400, 4200)
        for c in range(random.randint(2, 5)):
            c0 = start + c * random.uniform(0.09, 0.16)
            length = random.uniform(0.05, 0.11)
            for j in range(int(length * rate)):
                u = j / rate
                f = base + 900 * math.sin(math.pi * u / length) + 300 * math.sin(2 * math.pi * 40 * u)
                k = int((c0 + u) * rate)
                if k < n:
                    out[k] += math.sin(2 * math.pi * f * u) * math.sin(math.pi * u / length) * 0.12
    # seamless loop: crossfade the last second into the start
    fade = rate
    for i in range(fade):
        a = i / fade
        out[i] = out[i] * a + out[n - fade + i] * (1 - a)
    return out[: n - fade], rate


if __name__ == "__main__":
    for k, (peak, width, bright) in enumerate(((0.12, 0.035, 1.0), (0.14, 0.042, 0.85), (0.11, 0.03, 1.15))):
        write(f"SFX_SwordSwing_{k + 1}", sword_swing(peak, width, bright, seed=k))
    for k, w in enumerate((1.0, 0.85, 1.2)):
        write(f"SFX_FleshHit_{k + 1}", flesh_hit(seed=k, weight=w))
    write("SFX_ShieldBlock", shield_block())
    write("SFX_Footstep", footstep())
    write("SFX_EnemyDeath", enemy_death())
    write("SFX_BossRoar", boss_roar())
    for k, n in enumerate((2, 3, 2)):
        write(f"SFX_Coins_{k + 1}", coin_clatter(seed=k, clinks=n))
    write("SFX_Potion", potion())
    write("SFX_Chest", chest())
    write("SFX_QuestComplete", quest_fanfare())
    # the ambience bed is no longer generated: the Medieval Village provides real recorded ambience
