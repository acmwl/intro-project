import os
import subprocess
from datetime import datetime
from pathlib import Path

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
SAMPLE_RATE    = int(os.getenv("SAMPLE_RATE", "384000"))


class TriggerRequest(BaseModel):
    duration: int = 5


@app.post("/trigger")
def trigger_recording(body: TriggerRequest):
    """Record from the host mic for `duration` seconds and drop the WAV into the shared volume."""
    filename = RECORDINGS_DIR / f"recording_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav"
    try:
        subprocess.run(
            ["python", "/recorder/record.py", "--duration", str(body.duration), "--output", str(RECORDINGS_DIR)],
            check=True,
            timeout=body.duration + 10,
        )
    except subprocess.CalledProcessError as e:
        raise HTTPException(status_code=500, detail=f"Recording failed: {e}")
    return {"status": "ok", "file": filename.name}


@app.get("/health")
def health():
    return {"status": "ok"}
