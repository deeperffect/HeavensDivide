"""Generate an original, deterministic jackpot fanfare; no external samples."""
import array
import hashlib
import json
import math
import random
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RATE, DURATION = 48000, 3.25
samples = [0.0] * int(RATE * DURATION)
rng = random.Random(20261011)


def bell(start, frequency, gain, decay=0.42):
    for i in range(int(start * RATE), len(samples)):
        t = i / RATE - start
        envelope = min(1.0, t / 0.004) * math.exp(-t / decay)
        tone = sum(a * math.sin(2 * math.pi * frequency * r * t)
                   for a, r in ((1, 1), (.32, 2), (.12, 3.01), (.07, 4.17)))
        samples[i] += gain * envelope * tone


# Bright ascending C-major bells, then chords aligned with the three card locks.
for i, frequency in enumerate((523.25, 659.25, 783.99, 1046.5, 1318.51, 1567.98)):
    bell(i * .095, frequency, .13, .5)
for start, root in ((1.2, 523.25), (1.6267, 659.25), (2.0533, 783.99)):
    for ratio in (1, 1.25, 1.5, 2):
        bell(start, root * ratio, .07, .45)
# Soft low impact and glitter, kept below the melodic reward cue.
for i in range(len(samples)):
    t = i / RATE
    samples[i] += .18 * math.sin(2 * math.pi * 70 * t) * math.exp(-t * 12)
    samples[i] += rng.uniform(-1, 1) * .025 * math.exp(-t * 7) * min(1, t / .008)
dry = samples.copy()
for delay, gain in ((.11, .17), (.23, .09)):
    offset = int(delay * RATE)
    for i in range(offset, len(samples)):
        samples[i] += dry[i - offset] * gain
peak = max(map(abs, samples))
rms = math.sqrt(sum(v * v for v in samples) / len(samples))
gain = min(.7 / peak, .12 / rms)
samples = [v * gain * min(1, (DURATION - i / RATE) / .04) for i, v in enumerate(samples)]
pcm = array.array('h', (round(v * 32767) for v in samples))
target = ROOT / 'SourceAudio/Combat/ChestJackpot.wav'
target.parent.mkdir(parents=True, exist_ok=True)
with wave.open(str(target), 'wb') as output:
    output.setnchannels(1)
    output.setsampwidth(2)
    output.setframerate(RATE)
    output.writeframes(pcm.tobytes())
report = dict(duration=DURATION, sample_rate=RATE, peak=max(map(abs, samples)),
              rms=math.sqrt(sum(v*v for v in samples) / len(samples)),
              sha256=hashlib.sha256(target.read_bytes()).hexdigest())
(ROOT / 'SourceAudio/Combat/ChestJackpot.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
