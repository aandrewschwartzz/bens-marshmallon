"""Synthesize a cartoon gelatin wobble for Marshmallon's touch animation.

Output is mono 16-bit PCM at the firmware's fixed 22,050 Hz rate. The sound
has a rounded 'boing' attack, slowing pitch and amplitude wobbles, two small
rebound accents, and a soft tail. No external audio samples are used.
"""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 22_050
DURATION = 0.56
ASSETS = Path(__file__).resolve().parents[1] / 'assets'
WAV_OUT = ASSETS / 'jiggle.wav'
PCM_OUT = ASSETS / 'jiggle.pcm'
rng = random.Random(41)
phase = 0.0
noise_state = 0.0
samples = []
for n in range(round(RATE * DURATION)):
    t = n / RATE
    # A cartoon spring's fast initial wobble relaxes as the gelatin settles.
    wobble_phase = 2 * math.pi * (11.5 * t - 4.2 * t * t)
    pitch = 285 + 112 * math.exp(-5.0 * t) * math.cos(wobble_phase)
    pitch += 26 * math.exp(-3.5 * t) * math.sin(0.53 * wobble_phase)
    phase += 2 * math.pi * pitch / RATE
    rounded_buzz = (math.sin(phase) + 0.34 * math.sin(2 * phase + 0.45)
                    + 0.11 * math.sin(3 * phase + 1.1)) / 1.45
    tremolo = 0.66 + 0.34 * math.cos(wobble_phase - 0.7)
    attack = 1 - math.exp(-t / 0.004)
    envelope = attack * math.exp(-t / 0.23) * (1 - min(t / DURATION, 1) ** 6)
    body = 0.88 * envelope * tremolo * rounded_buzz

    # Two smaller resonant bounces make the tail sound gelatinous.
    rebound = 0.0
    for start, strength, freq in ((0.13, 0.18, 330), (0.27, 0.13, 285)):
        age = t - start
        if age >= 0:
            rebound += strength * (1 - math.exp(-age / 0.003)) * math.exp(-age / 0.052) * math.sin(2 * math.pi * (freq * age - 80 * age * age))

    # Tiny soft tap at onset, with most of the energy in the wobble itself.
    noise_state = 0.72 * noise_state + 0.28 * (2 * rng.random() - 1)
    tap = 0.08 * math.exp(-(t / 0.009) ** 2) * noise_state
    value = max(-0.94, min(0.94, body + rebound + tap))
    samples.append(round(value * 32767))

pcm = struct.pack('<%dh' % len(samples), *samples)
PCM_OUT.write_bytes(pcm)
with wave.open(str(WAV_OUT), 'wb') as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(RATE)
    wav.writeframes(pcm)
print(WAV_OUT)
print(PCM_OUT)
