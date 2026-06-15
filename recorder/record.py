#!/usr/bin/env python3
# Run on the host machine (needs direct mic access).
# Usage: python record.py [--duration 5] [--output ./recordings] [--rate 48000]

import argparse
import wave
from datetime import datetime
from pathlib import Path

import numpy as np
import sounddevice as sd

CHANNELS  = 1
BIT_DEPTH = 16


def record(duration: int, output_dir: Path, sample_rate: int) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    filename = output_dir / f"recording_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"

    print(f"Recording {duration}s at {sample_rate} Hz → {filename}")
    audio = sd.rec(
        int(duration * sample_rate),
        samplerate=sample_rate,
        channels=CHANNELS,
        dtype="int16",
    )
    sd.wait()

    with wave.open(str(filename), "w") as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(BIT_DEPTH // 8)
        wf.setframerate(sample_rate)
        wf.writeframes(audio.tobytes())

    print(f"Saved: {filename}")
    return filename


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Record from a microphone and save WAV.")
    parser.add_argument("--duration", type=int,  default=5,                  help="Duration in seconds (default: 5)")
    parser.add_argument("--output",   type=Path, default=Path("./recordings"), help="Output directory")
    parser.add_argument("--rate",     type=int,  default=48_000,
                        help="Sample rate in Hz (default: 48000; use --rate 384000 for the ultrasonic USB mic)")
    args = parser.parse_args()

    record(args.duration, args.output, args.rate)
