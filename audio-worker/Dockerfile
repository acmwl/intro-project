FROM python:3.11-slim AS builder

RUN apt-get update && apt-get install -y \
    g++ cmake make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY processor/ ./
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

FROM python:3.11-slim

# Unbuffered stdout so `docker compose logs -f` shows worker output live
ENV PYTHONUNBUFFERED=1

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY --from=builder /build/build/wav_processor ./wav_processor
COPY worker.py .
COPY baselines.json .

CMD ["python", "worker.py"]
