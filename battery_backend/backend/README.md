# Battery Monitor Backend

## Run with Docker

```bash
docker build -t battery-backend -f backend/Dockerfile .
docker run -d --name battery-backend -p 8000:8000 battery-backend
```

Open `http://localhost:8000/` for dashboard.

## Run locally (venv)

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r backend/requirements.txt
uvicorn backend.app.main:app --reload
```

## API

- POST `/api/ingest`

```json
{
  "timestamp": 123,
  "temperature": 25.5,
  "smoke_value": 150,
  "smoke_connected": true,
  "mq2_preheated": true,
  "fire_detected": false,
  "alert_active": false,
  "device_id": "battery_monitor_001"
}
```

- GET `/api/readings`
- GET `/api/readings/latest`

## Configure ESP32

Set in `src/config.h`:

```
#define BACKEND_HOST "your.server"
#define BACKEND_PORT 8000
#define BACKEND_PATH "/api/ingest"
```


