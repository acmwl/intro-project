#!/usr/bin/env python3
# Run on the host machine (needs direct mic access).
# Usage: python record.py [--duration 5] [--output ./recordings]

import argparse
import wave
from datetime import datetime
from pathlib import Path

import numpy as np
import sounddevice as sd

SAMPLE_RATE = 384_000  # 384 kHz — ultrasonic USB mic
CHANNELS    = 1
BIT_DEPTH   = 16


def record(duration: int, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    filename = output_dir / f"recording_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"

    print(f"Recording {duration}s at {SAMPLE_RATE} Hz → {filename}")
    audio = sd.rec(
        int(duration * SAMPLE_RATE),
        samplerate=SAMPLE_RATE,
        channels=CHANNELS,
        dtype="int16",
    )
    sd.wait()

    with wave.open(str(filename), "w") as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(BIT_DEPTH // 8)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(audio.tobytes())

    print(f"Saved: {filename}")
    return filename


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Record from ultrasonic mic and save WAV.")
    parser.add_argument("--duration", type=int,  default=5,                  help="Duration in seconds (default: 5)")
    parser.add_argument("--output",   type=Path, default=Path("./recordings"), help="Output directory")
    args = parser.parse_args()

    record(args.duration, args.output)
