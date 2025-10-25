from fastapi import FastAPI, Depends, Request, HTTPException, Header, UploadFile, File, Form, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import HTMLResponse, FileResponse
from sqlalchemy.orm import Session
from . import models, schemas
from .db import get_db, engine
from fastapi.templating import Jinja2Templates
import os
import json
from datetime import datetime, timezone, timedelta
from typing import Optional
import requests

models.Base.metadata.create_all(bind=engine)

app = FastAPI(title="Battery Monitor Backend")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

templates = Jinja2Templates(directory="backend/app/templates")

# Thêm filter để convert timestamp thành datetime với timezone (safe version)
def timestamp_to_datetime(timestamp):
    """Convert Unix timestamp to readable datetime string with timezone"""
    try:
        print(f"Converting timestamp: {timestamp}")
        if timestamp and timestamp > 0:
            # Kiểm tra timestamp hợp lệ (từ 2020 đến 2030)
            if timestamp < 1577836800 or timestamp > 1893456000:
                print(f"Invalid timestamp range: {timestamp}")
                return f"Invalid ({timestamp})"
            
            # Sử dụng timezone-aware datetime
            dt = datetime.fromtimestamp(timestamp, tz=timezone.utc)
            # Convert về timezone local (UTC+7)
            local_tz = timezone(timedelta(hours=7))
            local_dt = dt.astimezone(local_tz)
            result = local_dt.strftime("%Y-%m-%d %H:%M:%S")
            print(f"Converted successfully: {timestamp} -> {result}")
            return result
        else:
            print(f"No timestamp: {timestamp}")
            return "No timestamp"
    except (ValueError, OSError, OverflowError) as e:
        print(f"Error converting timestamp {timestamp}: {e}")
        return f"Error ({timestamp})"

# Đăng ký filter với Jinja2
templates.env.filters["timestamp_to_datetime"] = timestamp_to_datetime

# Filter để convert datetime (created_at) sang giờ địa phương (UTC+7)
def datetime_to_local(dt):
    try:
        if not dt:
            return "No time"
        # Nếu datetime không có tzinfo, coi như UTC
        if getattr(dt, "tzinfo", None) is None:
            dt = dt.replace(tzinfo=timezone.utc)
        local_dt = dt.astimezone(timezone(timedelta(hours=7)))
        return local_dt.strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return str(dt)

templates.env.filters["datetime_to_local"] = datetime_to_local

# API Key authentication
API_KEY = os.getenv("BATTERY_API_KEY")
if not API_KEY:
    print("⚠️  WARNING: BATTERY_API_KEY not set in environment variables!")
    print("   Using default key for development. Change this in production!")
    API_KEY = "battery_monitor_2024_secure_key"

print(f"🔑 API Key loaded: {API_KEY[:8]}...{API_KEY[-8:] if len(API_KEY) > 16 else '***'}")

def verify_api_key(x_api_key: str = Header(None)):
    if not x_api_key or x_api_key != API_KEY:
        raise HTTPException(status_code=401, detail="Invalid API key")
    return x_api_key

# Telegram configuration
TELEGRAM_BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID")

def _can_send_telegram() -> bool:
    return bool(TELEGRAM_BOT_TOKEN and TELEGRAM_CHAT_ID)

def send_telegram_message(text: str):
    if not _can_send_telegram():
        print("ℹ️ Telegram not configured. Skipping message.")
        return
    try:
        url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
        payload = {
            "chat_id": TELEGRAM_CHAT_ID,
            "text": text,
            "parse_mode": "HTML",
            "disable_web_page_preview": True,
        }
        resp = requests.post(url, json=payload, timeout=10)
        if resp.status_code != 200:
            print(f"⚠️ Telegram send failed: {resp.status_code} {resp.text}")
    except Exception as e:
        print(f"⚠️ Telegram send error: {e}")

def format_alert_message(payload: schemas.IngestRequest, server_timestamp: int) -> str:
    # Simple alert message including temperature, smoke, and fire status
    time_str = datetime.fromtimestamp(server_timestamp, tz=timezone(timedelta(hours=7))).strftime("%Y-%m-%d %H:%M:%S")
    lines = [
        "🚨 <b>CẢNH BÁO</b> từ thiết bị",
        f"Device: <code>{payload.device_id}</code>",
        f"Thời gian (UTC+7): {time_str}",
        f"Nhiệt độ: <b>{payload.temperature:.1f}°C</b>",
        f"Khói (MQ2): <b>{payload.smoke_value}</b> (preheated: {payload.mq2_preheated})",
        f"Cảm biến khói kết nối: {"✅" if payload.smoke_connected else "❌"}",
        f"Lửa phát hiện: {"🔥 Có" if payload.fire_detected else "✅ Không"}",
        f"Alert active: {"🟥" if payload.alert_active else "⬜"}",
    ]
    return "\n".join(lines)


@app.get("/", response_class=HTMLResponse)
def dashboard(request: Request, db: Session = Depends(get_db)):
    try:
        latest = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(50).all()
        return templates.TemplateResponse("index.html", {"request": request, "readings": latest})
    except Exception as e:
        print(f"Error in dashboard: {e}")
        return templates.TemplateResponse("index_simple.html", {"request": request, "readings": []})

@app.get("/test", response_class=HTMLResponse)
def test_dashboard(request: Request, db: Session = Depends(get_db)):
    try:
        latest = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(50).all()
        return templates.TemplateResponse("index_simple.html", {"request": request, "readings": latest})
    except Exception as e:
        print(f"Error in test dashboard: {e}")
        return f"Error: {str(e)}"

@app.get("/debug", response_class=HTMLResponse)
def debug_dashboard(request: Request, db: Session = Depends(get_db)):
    """Debug endpoint để test template đơn giản"""
    try:
        latest = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(50).all()
        return templates.TemplateResponse("index_debug.html", {"request": request, "readings": latest})
    except Exception as e:
        print(f"Error in debug dashboard: {e}")
        return f"Error: {str(e)}"


@app.post("/api/ingest", response_model=schemas.IngestResponse)
def ingest(payload: schemas.IngestRequest, background_tasks: BackgroundTasks, db: Session = Depends(get_db), api_key: str = Depends(verify_api_key)):
    # Lấy thời gian server thay vì dùng timestamp từ ESP32
    server_timestamp = int(datetime.now().timestamp())
    
    entity = models.Reading(
        device_id=payload.device_id,
        timestamp=server_timestamp,  # Sử dụng thời gian server
        temperature=payload.temperature,
        smoke_value=payload.smoke_value,
        smoke_connected=payload.smoke_connected,
        mq2_preheated=payload.mq2_preheated,
        fire_detected=payload.fire_detected,
        alert_active=payload.alert_active,
    )
    db.add(entity)
    db.commit()
    db.refresh(entity)

    # Send Telegram alert asynchronously if alert is active or fire detected
    try:
        if payload.alert_active or payload.fire_detected:
            message = format_alert_message(payload, server_timestamp)
            background_tasks.add_task(send_telegram_message, message)
    except Exception as e:
        print(f"⚠️ Failed to schedule Telegram alert: {e}")

    print(f"📊 Received data from {payload.device_id} at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    return schemas.IngestResponse(id=entity.id, status="ok")


@app.get("/api/readings", response_model=list[schemas.ReadingOut])
def list_readings(limit: int = 100, db: Session = Depends(get_db)):
    rows = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(limit).all()
    return [schemas.ReadingOut.from_orm(r) for r in rows]


@app.get("/api/readings/latest", response_model=schemas.ReadingOut | None)
def latest_reading(db: Session = Depends(get_db)):
    r = db.query(models.Reading).order_by(models.Reading.created_at.desc()).first()
    return schemas.ReadingOut.from_orm(r) if r else None


# Firmware Management APIs
@app.get("/api/firmware/check")
def check_firmware_update(current_version: str, device_id: str = "battery_monitor_001"):
    """Check if firmware update is available"""
    firmware_info_file = "firmware/latest.json"
    
    if not os.path.exists(firmware_info_file):
        return {"update_available": False, "message": "No firmware info found"}
    
    try:
        with open(firmware_info_file, 'r') as f:
            firmware_info = json.load(f)
        
        latest_version = firmware_info.get("version", "0.0.0")
        latest_build = firmware_info.get("build", 0)
        
        # Parse current version (format: "1.0.0" or "1.0.0-build123")
        current_parts = current_version.split("-")
        current_ver = current_parts[0]
        current_build = int(current_parts[1].replace("build", "")) if len(current_parts) > 1 else 0
        
        # Compare versions using proper semantic versioning
        def version_tuple(version_str):
            """Convert version string to tuple for proper comparison"""
            try:
                parts = version_str.split('.')
                return tuple(int(part) for part in parts)
            except ValueError:
                return (0, 0, 0)
        
        latest_tuple = version_tuple(latest_version)
        current_tuple = version_tuple(current_ver)
        
        if latest_tuple > current_tuple or (latest_tuple == current_tuple and latest_build > current_build):
            return {
                "update_available": True,
                "latest_version": latest_version,
                "latest_build": latest_build,
                "current_version": current_version,
                "download_url": firmware_info.get("download_url", ""),
                "file_size": firmware_info.get("file_size", 0),
                "release_notes": firmware_info.get("release_notes", ""),
                "checksum": firmware_info.get("checksum", "")
            }
        else:
            return {
                "update_available": False,
                "message": "Firmware is up to date",
                "current_version": current_version,
                "latest_version": latest_version
            }
            
    except Exception as e:
        return {"update_available": False, "error": str(e)}


@app.get("/api/firmware/download/{version}")
def download_firmware(version: str):
    """Download firmware binary"""
    firmware_file = f"firmware/battery_monitor_v{version}.bin"
    
    if not os.path.exists(firmware_file):
        raise HTTPException(status_code=404, detail="Firmware not found")
    
    return FileResponse(
        firmware_file,
        media_type="application/octet-stream",
        filename=f"battery_monitor_v{version}.bin"
    )


@app.post("/api/firmware/upload")
async def upload_firmware(
    file: UploadFile = File(...),
    version: str = Form(""),
    build: int = Form(0),
    release_notes: str = Form(""),
    api_key: str = Depends(verify_api_key)
):
    """Upload new firmware"""
    
    if not file.filename.endswith('.bin'):
        raise HTTPException(status_code=400, detail="Only .bin files are allowed")
    
    # Create firmware directory if not exists
    os.makedirs("firmware", exist_ok=True)
    
    # Save firmware file
    firmware_filename = f"battery_monitor_v{version}.bin"
    firmware_path = f"firmware/{firmware_filename}"
    
    with open(firmware_path, "wb") as buffer:
        content = await file.read()
        buffer.write(content)
    
    # Calculate checksum
    import hashlib
    checksum = hashlib.md5(content).hexdigest()
    
    # Update firmware info
    firmware_info = {
        "version": version,
        "build": build,
        "download_url": f"/api/firmware/download/{version}",
        "file_size": len(content),
        "release_notes": release_notes,
        "checksum": checksum,
        "uploaded_at": datetime.now().isoformat(),
        "filename": firmware_filename
    }
    
    with open("firmware/latest.json", "w") as f:
        json.dump(firmware_info, f, indent=2)
    
    return {
        "status": "success",
        "message": f"Firmware v{version} uploaded successfully",
        "firmware_info": firmware_info
    }


@app.get("/api/firmware/info")
def get_firmware_info():
    """Get current firmware information"""
    firmware_info_file = "firmware/latest.json"
    
    if not os.path.exists(firmware_info_file):
        return {"error": "No firmware info found"}
    
    try:
        with open(firmware_info_file, 'r') as f:
            return json.load(f)
    except Exception as e:
        return {"error": str(e)}