import os
import json
import time
import subprocess
import psycopg2
from pathlib import Path
from datetime import datetime, timezone

RECORDINGS_DIR = Path(os.getenv("RECORDINGS_DIR", "/recordings"))
PROCESSOR_BIN  = Path(os.getenv("PROCESSOR_BIN",  "/app/wav_processor"))
BASELINES_FILE = Path(os.getenv("BASELINES_FILE", "/app/baselines.json"))
DATABASE_URL   = os.getenv("DATABASE_URL", "postgresql://postgres:password@postgres:5432/measurements")
POLL_INTERVAL  = int(os.getenv("POLL_INTERVAL_S", "5"))


def connect_db():
    while True:
        try:
            conn = psycopg2.connect(DATABASE_URL)
            print("[DB] connected")
            return conn
        except Exception as e:
            print(f"[DB] waiting for database: {e}")
            time.sleep(2)


def run_processor(wav_path: Path) -> dict | None:
    cmd = [str(PROCESSOR_BIN), str(wav_path)]
    if BASELINES_FILE.exists():
        cmd.append(str(BASELINES_FILE))

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[ERROR] processor failed for {wav_path.name}:\n{result.stderr.strip()}")
        return None

    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as e:
        print(f"[ERROR] invalid JSON from processor: {e}\nOutput was: {result.stdout!r}")
        return None


def save_measurement(conn, wav_path: Path, data: dict):
    with conn.cursor() as cur:
        cur.execute(
            """
            INSERT INTO measurements
                (filename, recorded_at, rms_db, peak_frequency_hz, baseline_delta_db, passed_baseline)
            VALUES (%s, %s, %s, %s, %s, %s)
            """,
            (
                wav_path.name,
                datetime.now(timezone.utc),
                data.get("rms_db"),
                data.get("peak_frequency_hz"),
                data.get("baseline_delta_db"),
                data.get("passed_baseline"),
            ),
        )
    conn.commit()
    status = "PASS" if data.get("passed_baseline") else "FAIL"
    print(
        f"[{status}] {wav_path.name} "
        f"rms={data.get('rms_db')} dBFS  "
        f"peak={data.get('peak_frequency_hz')} Hz  "
        f"delta={data.get('baseline_delta_db')} dB"
    )


def move_to_processed(wav_path: Path):
    processed = RECORDINGS_DIR / "processed"
    processed.mkdir(exist_ok=True)
    wav_path.rename(processed / wav_path.name)


def main():
    conn = connect_db()
    processed = set()

    print(f"[START] watching {RECORDINGS_DIR} every {POLL_INTERVAL}s ...")

    while True:
        for wav in sorted(RECORDINGS_DIR.glob("*.wav")):
            if wav in processed:
                continue
            processed.add(wav)
            print(f"[FOUND] {wav.name}")

            data = run_processor(wav)
            if data is None:
                continue

            save_measurement(conn, wav, data)
            move_to_processed(wav)

        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
