CREATE TABLE IF NOT EXISTS measurements (
    id                SERIAL PRIMARY KEY,
    filename          TEXT        NOT NULL,
    recorded_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    rms_db            FLOAT,
    peak_frequency_hz FLOAT,
    baseline_delta_db FLOAT,
    passed_baseline   BOOLEAN
);
