# Audio Measurement Pipeline — Engineering Challenge

Welcome. This is a real engineering challenge, not a tutorial.

You have been given a partially-built data pipeline that records audio, analyses it, stores the results, and displays them on a live dashboard. The infrastructure is already in place and working — but the analysis engine at the heart of it is a stub that emits fake values. Your job is to replace it with a real implementation, verify the whole system end-to-end, and extend the visualisation layer.

By the time you are done you will have touched Python, C++, SQL, Docker, and Grafana. You are not expected to be an expert in all of them. You are expected to figure things out.

---

## System Overview

The pipeline has four stages:

```
[Microphone / WAV file]
     │  recorder/record.py  (runs on your machine)
     ▼
recordings/                 ← shared folder between your machine and Docker
     │  watched by
     ▼
audio-worker                ← Python daemon; calls a C++ binary, writes results to the DB
     │
     ▼
PostgreSQL                  ← single table: measurements
     │
     ▼
Grafana                     ← live dashboard at http://localhost:3000
```

The Docker services (audio-worker, postgres, grafana, plus an optional API and web UI for the bonus tasks) are defined in `docker-compose.yml` — that file is the map of the whole system. The `recordings/` folder is the handoff point between your host machine and the Docker containers. Everything else flows from there.

---

## Required Tasks

Work through these in order. Each task builds on the previous one.

---

### Task 1 — Bring the pipeline to life

Before writing a single line of code, get the system running and trace the full data path yourself.

**Start the stack:**

```bash
docker compose up --build
```

**Drop a WAV file into the pipeline.** If you have a microphone, install the recorder dependencies and record a short clip:

```bash
pip install sounddevice numpy
python recorder/record.py --duration 5 --output ./recordings
```

The recorder defaults to 48 kHz, which any standard mic supports. If you have the 384 kHz ultrasonic USB mic, add `--rate 384000`.

If you do not have a suitable microphone, two sample ultrasonic recordings (`recordings/noisy.wav` and `recordings/quiet.wav`) are included in the repository — drop either one into `recordings/` and the pipeline will exercise itself. Any other mono WAV file works too.

**Watch the worker pick it up:**

```bash
docker compose logs -f audio-worker
```

You should see it find the file, call the processor, and write a row to the database. Open Grafana at `http://localhost:3000` — the *Audio Measurements* dashboard should show a new data point (with stub values, for now).

**The goal of this task is not to produce real data.** It is to confirm that your environment works and that you understand the full path a file takes from the `recordings/` folder to a point on a Grafana chart. Do not move on until you can describe that path in your own words.

---

### Task 2 — Replace the C++ stub with a real processor

Open `audio-worker/processor/wav_processor.cpp`. The file is short and the top section explains exactly what it must do. This is its interface contract — the Python worker calls this binary and expects a specific output format:

**Input:**
- `argv[1]` — path to a WAV file to analyse
- `argv[2]` — (optional) path to a baselines JSON file

**Output on success:** a single line of JSON to stdout, exit code 0:

```json
{
  "rms_db":            -20.5,
  "peak_frequency_hz": 40000.0,
  "baseline_delta_db": 2.1,
  "passed_baseline":   true
}
```

**Output on failure:** a human-readable error message to stderr, exit code 1.

**What each field means:**

- `rms_db` — the Root Mean Square level of the audio signal, in dBFS (decibels relative to full scale). This tells you how loud the signal is on average.
- `peak_frequency_hz` — the dominant frequency in the signal, detected via a Fourier transform. For an ultrasonic recording this might be 40 kHz; for a normal recording it will be in the audible range.
- `baseline_delta_db` — how far the current RMS is from a reference baseline. If no baselines file is provided, output `0.0`.
- `passed_baseline` — `true` if the delta is within an acceptable range (a reasonable threshold is ±6 dB), `false` otherwise. If there is no baseline, output `true`.

**The baselines file** (`argv[2]`) is a JSON object containing the reference RMS level:

```json
{
  "rms_db": -20.0
}
```

Compute `baseline_delta_db` as the current `rms_db` minus this reference, and set `passed_baseline` to `true` when `|baseline_delta_db| ≤ 6 dB`. If `argv[2]` is not given, output a delta of `0.0` and `passed_baseline: true`. An example file ships in the worker image at `/app/baselines.json` (see `audio-worker/baselines.json`), so the worker passes it on every call.

**On libraries:** you are free to use any C++ libraries that help. Some useful ones:

- WAV file reading: [dr_wav](https://github.com/mackron/dr_libs) (single-header, easy to drop in) or libsndfile
- FFT: [KissFFT](https://github.com/mborgerding/kissfft) (lightweight, easy to integrate) or FFTW
- JSON output: [nlohmann/json](https://github.com/nlohmann/json) (single-header) or just write the JSON string manually (it is simple enough)

If you add external libraries, you will need to update `CMakeLists.txt` accordingly. That is expected and is part of the task.

**Do not change the interface contract.** The Python worker depends on the argument order, the JSON field names, and the exit codes. Everything else is yours to decide.

---

### Task 3 — Verify end-to-end with real values

Once your processor compiles and produces real output, rebuild and re-run:

```bash
docker compose up --build
```

Drop a WAV file in. Check the worker logs — you should see real values, not `-20.5 / 40000.0 / 2.1` every time. Open Grafana and confirm the timeseries charts are moving with varied data.

Also query the database directly to see what was stored.

`psql` is PostgreSQL's interactive command-line client — it connects you to the running database container and lets you run SQL queries. Once you are inside, you can type `\d measurements` to describe the table schema (columns, types, constraints) and `\q` to quit.

```bash
docker compose exec postgres psql -U postgres -d measurements
```

```sql
SELECT * FROM measurements ORDER BY recorded_at DESC LIMIT 10;
```

Breaking down that query: `SELECT *` retrieves every column; `FROM measurements` is the table to read from; `ORDER BY recorded_at DESC` sorts rows newest-first; `LIMIT 10` returns at most ten rows. Together it gives you a snapshot of the most recent measurements.

The values there should match what your processor computed. If they do not, trace the discrepancy through the worker logs and the processor's stdout.

This task is done when you can look at the Grafana dashboard with real data and explain where each value came from.

---

### Task 4 — Understand and extend the Grafana dashboard

**How Grafana works:** Grafana is a data visualisation tool. It connects to data sources — in this case, the PostgreSQL database — and lets you build dashboards from them. A **datasource** is a saved connection to a database (already configured for you). A **dashboard** is a page made up of one or more **panels**. Each panel is a single chart, table, or stat widget, and it is driven by a single SQL query: Grafana runs that query against the datasource and maps the result columns to the visual axes. The time-series panels, for example, expect a column named `time` (mapped to the x-axis) and one or more value columns (mapped to the y-axis). Understanding this model before you start will save you a lot of confusion.

The pre-built dashboard has six panels. Open Grafana, click into any panel, and hit **Edit**. You will see the SQL query that drives it. Read it. Understand what table it queries, what columns it selects, how it handles time filtering, and how the results map to the chart.

Also look at `grafana/dashboards/measurements.json` — this is the dashboard definition file. Grafana reads it on startup. You can modify this file and restart Grafana to make changes persist. Alternatively, you can edit panels through the UI and export the updated JSON when you are happy.

**Your task:** add at least one panel of your own design to the dashboard.

Pick a metric already in the `measurements` table (`rms_db`, `peak_frequency_hz`, `baseline_delta_db`, `passed_baseline`) and visualise it in a way the existing dashboard does not already cover. Some ideas:

- A bar chart or histogram showing the distribution of RMS values across all recordings
- A stat panel showing the all-time minimum and maximum peak frequency detected
- A table showing only the recordings that failed the baseline check
- A timeseries of the rolling average RMS over a sliding 10-minute window

If you want to go further, add a second panel that requires a more complex query — a conditional aggregation, a percentage, a window function.

**The goal is to write SQL you understand and produce a visualisation you can explain.** Do not copy-paste a query without knowing what it does.

---

## Bonus Tasks

These are optional extensions. They are not required, but each one teaches you something new about how multi-service systems are built.

---

### Bonus 1 — Trigger API

The `bonus/api/` folder contains the start of a FastAPI service. It already has a working `POST /trigger` endpoint that generates a test-tone WAV into the shared `recordings/` volume (the container has no microphone — real recordings still come from `recorder/record.py` on the host).

Uncomment the `api` service block in `docker-compose.yml` to enable it, then rebuild.

Your task: add a `GET /measurements` endpoint that reads the most recent rows from the `measurements` table and returns them as JSON. This endpoint will be called by the bonus UI.

Hint: the API container does not yet have any way to talk to the database. Look at how the audio-worker gets its database connection — you will need to give the `api` service the same kind of access (an environment variable in `docker-compose.yml` and a database client in `bonus/api/requirements.txt`).

---

### Bonus 2 — Live web UI

The `bonus/ui/index.html` page has a *Record Now* button that calls `POST /trigger`. It works — uncomment the `ui` service in `docker-compose.yml` and try it.

Your task: wire up the `GET /measurements` endpoint (from Bonus 1) so that the table below the button populates with live data and refreshes automatically. No Grafana required.

---

### Bonus 3 — Extend the pipeline

Invent something. Some directions:

- Add a new computed metric to the C++ processor (e.g. spectral centroid, crest factor, or total harmonic distortion). Add a column to the `measurements` table, thread the value through the worker, and build a Grafana panel for it. Note that `db/init.sql` only runs when the Postgres volume is created from scratch — to apply a schema change you either need `docker compose down -v` (wipes data) or a manual `ALTER TABLE` via psql.
- Make the baseline threshold configurable — right now it is hardcoded. Move it into the environment or expose it through the API.
- Instrument the worker with timing: how long does the processor take per file? Log it and store it.

---

## Tips

### Docker and containers — a quick orientation

A container is an isolated process that has its own filesystem, its own network interface, and its own environment variables — but shares the host machine's kernel. Think of it as a lightweight VM that starts in seconds. `docker-compose.yml` is the map of the whole system: it defines every service (container), what image or Dockerfile to build it from, what ports to expose, what environment variables to inject, and what volumes to mount.

`docker compose up --build` reads that file, builds images for any service that has a `build:` directive, starts all containers, and wires their internal network so they can reach each other by service name (e.g. the worker connects to `postgres:5432`, not `localhost:5432`).

The `recordings/` folder is a **bind mount** — a directory on your host machine that is mounted directly into the container at a specified path. When you drop a WAV file into `recordings/` on your laptop, the audio-worker container sees it instantly at `/recordings/` inside the container, without any copying.

Changes to Python or C++ source files require a rebuild (`--build`) because the source is copied into the image at build time. Changes to config files like the Grafana dashboard JSON only need a service restart.

---

### Docker and multi-service debugging

- `docker compose logs -f <service>` tails logs in real time. When something is not working, this is the first place to look.
- `docker compose ps` shows you the state and health of every service. If a service is unhealthy, its logs will tell you why.
- `docker compose exec <service> bash` gives you a shell inside a running container. Useful for inspecting files, running commands, or checking that a binary exists where you expect it.
- `docker compose exec postgres psql -U postgres -d measurements` connects you directly to the database. Always verify data at the source rather than trusting what the UI shows you.
- `docker compose up --build audio-worker` rebuilds and restarts only the worker service, without touching postgres or grafana. Faster than a full rebuild when you are iterating on C++ changes.
- `docker compose down -v` tears down everything and wipes all data volumes. Useful when you want a completely clean state. Be aware that it deletes your measurement history. It is also the only way to get `db/init.sql` to run again — that script only executes on a fresh Postgres volume.

---

### Adding libraries to CMakeLists.txt

CMake is a build system generator — it reads `CMakeLists.txt` and produces the instructions that the compiler uses to build your project. You do not usually interact with CMake directly; the `Dockerfile` runs it for you inside the container.

**Single-header libraries** (like dr_wav and nlohmann/json) need no CMake change at all. Download the `.h` file, drop it in the `processor/` directory alongside `wav_processor.cpp`, and `#include` it. The compiler will find it automatically.

**Multi-file libraries** (like KissFFT) need to be added to `CMakeLists.txt`. The cleanest way is `FetchContent`, which downloads and compiles the library as part of the build. Here is a minimal example for KissFFT:

```cmake
include(FetchContent)
FetchContent_Declare(
  kissfft
  GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(kissfft)

target_link_libraries(wav_processor PRIVATE kissfft)
```

Add the `include(FetchContent)` block before your `add_executable` line, and the `target_link_libraries` line after it. After any `CMakeLists.txt` change, rebuild the worker:

```bash
docker compose up --build audio-worker
```

---

### General engineering habits

Get the stub working before you write anything. Task 1 exists for a reason — an environment that does not work with the stub will not work with your implementation either, and debugging a broken environment at the same time as debugging your code is miserable.

Make one change at a time. When something breaks after two changes, you will not know which one caused it. When something breaks after one change, you do.

Use stderr and print statements aggressively while developing the C++ processor. They cost nothing and save hours. The worker captures both stdout and stderr from the binary — check the logs.

Commit to git when something works. Even if the code is messy, a commit is a checkpoint you can return to. You will be glad you made it when a later change breaks something.

When you are stuck, read the error message from the beginning. Not the last line — the first line. The root cause is almost always in the first line, and the lines that follow are just its consequences.
