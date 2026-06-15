import os
import wave
from datetime import datetime
from pathlib import Path

import numpy as np
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

app = FastAPI(title="Audio Trigger API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

RECORDINGS_DIR = Path(os.getenv("RECORDINGS_DIR", "/recordings"))


class TriggerRequest(BaseModel):
    duration: int = 5
    frequency_hz: float = 1000.0
    sample_rate: int = 48000


@app.post("/trigger")
def trigger_recording(body: TriggerRequest):
    """Generate a synthetic test-tone WAV and drop it into the shared volume.

    The container has no microphone, so this writes a mono 16-bit sine wave
    instead. Real mic recordings are made on the host via recorder/record.py.
    """
    filename = RECORDINGS_DIR / f"testtone_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
    try:
        t = np.arange(body.duration * body.sample_rate) / body.sample_rate
        audio = (0.5 * np.sin(2 * np.pi * body.frequency_hz * t) * 32767).astype(np.int16)

        RECORDINGS_DIR.mkdir(parents=True, exist_ok=True)
        with wave.open(str(filename), "w") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(body.sample_rate)
            wf.writeframes(audio.tobytes())
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Tone generation failed: {e}")
    return {"status": "ok", "file": filename.name}


@app.get("/health")
def health():
    return {"status": "ok"}
