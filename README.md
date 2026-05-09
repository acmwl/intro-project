# Audio Measurement Pipeline

A minimal end-to-end data pipeline that records audio, analyses it with a C++ signal processor, stores the results in PostgreSQL, and displays them on a live Grafana dashboard.

---

## Overview

The pipeline is designed around a real-world inspection use case: capture audio from a USB microphone, compute signal metrics (RMS level, peak frequency, baseline deviation), and make the results queryable and visible in real time. The stack touches Python, C++, SQL, Docker, and Grafana — each layer doing one job and handing off to the next via a well-defined interface.

The system runs entirely in Docker Compose. The only part that runs on the host machine is the audio recorder, which needs direct access to the microphone.

---

## Architecture

```
[Microphone / WAV file]
     │  recorder/record.py  (runs on your machine)
     ▼
recordings/                 ← shared folder between host and Docker
     │  watched by
     ▼
audio-worker                ← Python daemon; calls a C++ processor binary, writes to DB
     │
     ▼
PostgreSQL                  ← measurements table
     │
     ▼
Grafana                     ← live dashboard at http://localhost:3000
```

---

## Prerequisites

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) (Mac / Windows) or Docker Engine + Compose plugin (Linux)
- Python 3.9+ on your host machine (for the recorder)
- A microphone — any mic works for development; a 384 kHz USB ultrasonic mic is needed for full-fidelity measurements

---

## Fork & Clone (Start Here)

You should work on your own copy of this repository, not the original. That way you can push commits, open pull requests, and experiment freely without affecting anyone else.

**1. Fork the repository**

Go to the repository page on GitHub and click the **Fork** button (top-right corner). GitHub will create a copy under your own account (e.g. `github.com/<your-username>/go2w_data_server`).

**2. Clone your fork**

```bash
git clone https://github.com/<your-username>/go2w_data_server.git
cd go2w_data_server/introduction-project
```

Replace `<your-username>` with your GitHub username. This downloads your fork to your machine and sets `origin` to point at it, so `git push` goes to your copy.

**3. Keep a link to the original (optional but recommended)**

```bash
git remote add upstream https://github.com/Achilleas-Pappas/go2w_data_server.git
```

This lets you pull in future updates from the original with:

```bash
git fetch upstream
git merge upstream/main
```

---

## Getting Started

**1. Start the stack**

```bash
docker compose up --build
```

This builds and starts the audio-worker, PostgreSQL, and Grafana. The first build takes a minute because it compiles the C++ processor inside the container.

**2. Record from your microphone**

Run this on your host machine (not inside Docker):

```bash
pip install sounddevice numpy
python recorder/record.py --duration 5 --output ./recordings
```

The worker picks up the file automatically within a few seconds.

If you do not have a microphone handy, drop any mono WAV file into the `recordings/` folder and the pipeline will process it.

**3. Open the dashboard**

```
http://localhost:3000
```

The *Audio Measurements* dashboard loads automatically — no login required.

---

## Where to Start

Read [`Assignment.md`](Assignment.md). It describes the project goals, the tasks to work through (in order), and tips for navigating the codebase and the Docker environment. That document is your entry point.

---

## Services

| Service | Description | Port |
|---------|-------------|------|
| `audio-worker` | Polls `recordings/` for WAV files, runs the C++ processor, writes to DB | — |
| `postgres` | Stores measurement results in a single `measurements` table | 5432 |
| `grafana` | Live dashboard, auto-provisioned with the measurements datasource | 3000 |
| `api` *(optional)* | FastAPI trigger endpoint — uncomment in `docker-compose.yml` to enable | 8000 |

---

## Useful Commands

```bash
# Tail worker logs in real time
docker compose logs -f audio-worker

# Connect to the database
docker compose exec postgres psql -U postgres -d measurements

# Rebuild only the worker (faster when iterating on C++ changes)
docker compose up --build audio-worker

# Tear down, keep data
docker compose down

# Tear down and wipe all data volumes
docker compose down -v
```
