# -*- coding: utf-8 -*-
"""
@file main.py
@brief FastAPI application chính cho Battery Monitor Backend.

Module này cung cấp:
- REST API endpoints để nhận dữ liệu từ ESP32
- Web dashboard để xem dữ liệu cảm biến
- Firmware management APIs (check, upload, download)
- Telegram notification khi có cảnh báo
- Database migration tự động cho SQLite
"""
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
# from typing import Optional  # unused
import requests

# Tự động tạo bảng trong database nếu chưa tồn tại
models.Base.metadata.create_all(bind=engine)

# --------------------------------------------------------------------
# Database Migration: Tự động thêm cột mới cho SQLite
# --------------------------------------------------------------------
def ensure_migrations():
    """
    Tự động thêm các cột mới vào bảng readings nếu chưa tồn tại.
    
    Hàm này chỉ chạy với SQLite, kiểm tra schema hiện tại và thêm các cột:
    - fire_value: Giá trị cảm biến lửa KY-026
    - temp_alert, smoke_alert, fire_alert: Các cờ cảnh báo
    
    Lưu ý: Với database khác (PostgreSQL, MySQL), dùng migration tool như Alembic.
    """
    try:
        if str(engine.url).startswith("sqlite"):
            from sqlalchemy import text
            with engine.connect() as conn:
                # Lấy danh sách cột hiện tại trong bảng readings
                res = conn.execute(text("PRAGMA table_info(readings)"))
                cols = {row[1] for row in res}
                
                # Thêm cột fire_value nếu chưa có
                if "fire_value" not in cols:
                    conn.execute(text("ALTER TABLE readings ADD COLUMN fire_value INTEGER"))
                    print(" Added column fire_value to readings")
                
                # Thêm các cột cảnh báo nếu chưa có
                for col in ("temp_alert","smoke_alert","fire_alert"):
                    if col not in cols:
                        conn.execute(text(f"ALTER TABLE readings ADD COLUMN {col} BOOLEAN"))
                        print(f" Added column {col} to readings")
    except Exception as e:
        print(f" Migration check failed: {e}")

# Chạy migration khi khởi động ứng dụng
ensure_migrations()

# --------------------------------------------------------------------
# Khởi tạo FastAPI Application
# --------------------------------------------------------------------
app = FastAPI(title="Battery Monitor Backend")

# Cấu hình CORS: cho phép mọi origin truy cập (phù hợp cho IoT)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Production nên giới hạn origin cụ thể
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Khởi tạo Jinja2 templates cho web dashboard
templates = Jinja2Templates(directory="backend/app/templates")

# --------------------------------------------------------------------
# Jinja2 Template Filters: Chuyển đổi thời gian cho web dashboard
# --------------------------------------------------------------------

def timestamp_to_datetime(timestamp):
    """
    Convert Unix timestamp sang chuỗi thời gian địa phương (UTC+7).
    
    Hàm này được dùng trong Jinja2 templates để hiển thị timestamp từ database.
    Xử lý an toàn các trường hợp timestamp không hợp lệ.
    
    Args:
        timestamp (int): Unix timestamp
        
    Returns:
        str: Chuỗi thời gian dạng "YYYY-MM-DD HH:MM:SS" (UTC+7) hoặc thông báo lỗi
    """
    try:
        if not timestamp or timestamp <= 0:
            return "No timestamp"
        # Validate khoảng thời gian hợp lý (2020–2030) để tránh giá trị sai
        if timestamp < 1577836800 or timestamp > 1893456000:
            return f"Invalid ({timestamp})"

        # Convert từ UTC sang UTC+7 (Việt Nam)
        dt = datetime.fromtimestamp(timestamp, tz=timezone.utc)
        local_dt = dt.astimezone(timezone(timedelta(hours=7)))
        return local_dt.strftime("%Y-%m-%d %H:%M:%S")
    except (ValueError, OSError, OverflowError):
        return f"Error ({timestamp})"

# Đăng ký filter với Jinja2 để dùng trong templates: {{ timestamp | timestamp_to_datetime }}
templates.env.filters["timestamp_to_datetime"] = timestamp_to_datetime


def datetime_to_local(dt):
    """
    Convert datetime object sang chuỗi thời gian địa phương (UTC+7).
    
    Hàm này xử lý datetime từ SQLAlchemy (created_at) để hiển thị trong dashboard.
    
    Args:
        dt (datetime): Datetime object (có thể có hoặc không có timezone)
        
    Returns:
        str: Chuỗi thời gian dạng "YYYY-MM-DD HH:MM:SS" (UTC+7) hoặc thông báo lỗi
    """
    try:
        if not dt:
            return "No time"
        # Nếu datetime không có tzinfo, coi như UTC
        if getattr(dt, "tzinfo", None) is None:
            dt = dt.replace(tzinfo=timezone.utc)
        # Convert sang UTC+7
        local_dt = dt.astimezone(timezone(timedelta(hours=7)))
        return local_dt.strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return str(dt)

# Đăng ký filter với Jinja2: {{ reading.created_at | datetime_to_local }}
templates.env.filters["datetime_to_local"] = datetime_to_local

# --------------------------------------------------------------------
# API Key Authentication
# --------------------------------------------------------------------
# Lấy API key từ biến môi trường, fallback về key mặc định (chỉ dùng development)
API_KEY = os.getenv("BATTERY_API_KEY")
if not API_KEY:
    print("  WARNING: BATTERY_API_KEY not set in environment variables!")
    print("   Using default key for development. Change this in production!")
    API_KEY = "battery_monitor_2024_secure_key"

print(f"API Key loaded: {API_KEY[:8]}...{API_KEY[-8:] if len(API_KEY) > 16 else '***'}")


def verify_api_key(x_api_key: str = Header(None)):
    """
    Dependency function để xác thực API key từ header X-API-Key.
    
    FastAPI sẽ tự động gọi hàm này khi endpoint có `api_key: str = Depends(verify_api_key)`.
    Nếu key không hợp lệ, trả về HTTP 401 Unauthorized.
    
    Args:
        x_api_key (str): API key từ header X-API-Key
        
    Returns:
        str: API key nếu hợp lệ
        
    Raises:
        HTTPException: 401 nếu key không hợp lệ hoặc thiếu
    """
    if not x_api_key or x_api_key != API_KEY:
        raise HTTPException(status_code=401, detail="Invalid API key")
    return x_api_key


# --------------------------------------------------------------------
# Telegram Notification Configuration
# --------------------------------------------------------------------
# Lấy thông tin bot Telegram từ biến môi trường
TELEGRAM_BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID")


def _can_send_telegram() -> bool:
    """
    Kiểm tra xem có cấu hình đầy đủ để gửi Telegram không.
    
    Returns:
        bool: True nếu có cả token và chat_id, False nếu thiếu
    """
    return bool(TELEGRAM_BOT_TOKEN and TELEGRAM_CHAT_ID)


def send_telegram_message(text: str):
    """
    Gửi tin nhắn cảnh báo qua Telegram bot.
    
    Hàm này được gọi bất đồng bộ (background task) khi có cảnh báo từ ESP32.
    Nếu không cấu hình Telegram, hàm sẽ bỏ qua và log thông báo.
    
    Args:
        text (str): Nội dung tin nhắn (HTML format)
    """
    if not _can_send_telegram():
        print(" Telegram not configured. Skipping message.")
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
            print(f" Telegram send failed: {resp.status_code} {resp.text}")
    except Exception as e:
        print(f" Telegram send error: {e}")


def format_alert_message(payload: schemas.IngestRequest, server_timestamp: int) -> str:
    """
    Format tin nhắn cảnh báo để gửi qua Telegram.
    
    Tạo tin nhắn HTML bao gồm:
    - Thông tin thiết bị
    - Thời gian cảnh báo (UTC+7)
    - Giá trị cảm biến (nhiệt độ, MQ-135, KY-026)
    - Danh sách modules đang cảnh báo
    
    Args:
        payload (schemas.IngestRequest): Dữ liệu cảm biến từ ESP32
        server_timestamp (int): Unix timestamp từ server
        
    Returns:
        str: Tin nhắn HTML đã format
    """
    # Convert timestamp sang UTC+7
    time_str = datetime.fromtimestamp(server_timestamp, tz=timezone(timedelta(hours=7))).strftime("%Y-%m-%d %H:%M:%S")
    
    # Thu thập danh sách modules đang cảnh báo
    modules = []
    if payload.temp_alert:
        modules.append("NHIET DO")
    if payload.smoke_alert:
        modules.append("MQ-135")
    if payload.fire_alert:
        modules.append("KY-026")

    # Tạo tin nhắn HTML
    lines = [
        "<b>CANH BAO</b> tu thiet bi",
        f"Device: <code>{payload.device_id}</code>",
        f"Thoi gian (UTC+7): {time_str}",
        f"Nhiet do: <b>{payload.temperature:.1f} C</b>",
        f"MQ-135: <b>{payload.smoke_value}</b>",
        f"KY-026 (10-bit): <b>{payload.fire_value}</b>",
        f"Modules: {', '.join(modules) if modules else 'NONE'}",
    ]
    return "\n".join(lines)


# --------------------------------------------------------------------
# Web Dashboard Endpoints
# --------------------------------------------------------------------

@app.get("/", response_class=HTMLResponse)
def dashboard(request: Request, db: Session = Depends(get_db)):
    """
    Trang dashboard chính hiển thị dữ liệu cảm biến.
    
    Lấy 50 bản ghi mới nhất từ database và render bằng template index.html.
    Nếu có lỗi, fallback về template đơn giản hơn.
    
    Args:
        request (Request): FastAPI request object
        db (Session): Database session từ dependency injection
        
    Returns:
        TemplateResponse: HTML response với dữ liệu cảm biến
    """
    try:
        latest = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(50).all()
        return templates.TemplateResponse("index.html", {"request": request, "readings": latest})
    except Exception as e:
        print(f"Error in dashboard: {e}")
        return templates.TemplateResponse("index_simple.html", {"request": request, "readings": []})


@app.get("/test", response_class=HTMLResponse)
def test_dashboard(request: Request, db: Session = Depends(get_db)):
    """
    Trang dashboard test với template đơn giản.
    
    Endpoint này dùng để test template index_simple.html khi có vấn đề với template chính.
    
    Args:
        request (Request): FastAPI request object
        db (Session): Database session
        
    Returns:
        TemplateResponse hoặc str: HTML response hoặc error message
    """
    try:
        latest = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(50).all()
        return templates.TemplateResponse("index_simple.html", {"request": request, "readings": latest})
    except Exception as e:
        print(f"Error in test dashboard: {e}")
        return f"Error: {str(e)}"


@app.get("/debug", response_class=HTMLResponse)
def debug_dashboard(request: Request, db: Session = Depends(get_db)):
    """
    Trang dashboard debug với template tối giản nhất.
    
    Endpoint này dùng để debug khi cả 2 template trên đều lỗi.
    Template index_debug.html chỉ hiển thị dữ liệu thô, không có styling.
    
    Args:
        request (Request): FastAPI request object
        db (Session): Database session
        
    Returns:
        TemplateResponse hoặc str: HTML response hoặc error message
    """
    try:
        latest = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(50).all()
        return templates.TemplateResponse("index_debug.html", {"request": request, "readings": latest})
    except Exception as e:
        print(f"Error in debug dashboard: {e}")
        return f"Error: {str(e)}"


# --------------------------------------------------------------------
# REST API Endpoints: Nhận và truy vấn dữ liệu cảm biến
# --------------------------------------------------------------------

@app.post("/api/ingest", response_model=schemas.IngestResponse)
def ingest(
    payload: schemas.IngestRequest,
    background_tasks: BackgroundTasks,
    db: Session = Depends(get_db),
    api_key: str = Depends(verify_api_key)
):
    """
    Endpoint chính để ESP32 gửi dữ liệu cảm biến lên server.
    
    Quy trình:
    1. Nhận dữ liệu từ ESP32 (temperature, smoke_value, fire_value, alerts)
    2. Tạo timestamp từ server (không dùng timestamp từ ESP32 để đảm bảo đồng bộ)
    3. Lưu vào database
    4. Nếu có cảnh báo (temp_alert, smoke_alert, fire_alert), gửi Telegram bất đồng bộ
    
    Args:
        payload (schemas.IngestRequest): Dữ liệu cảm biến từ ESP32
        background_tasks (BackgroundTasks): FastAPI background tasks để gửi Telegram
        db (Session): Database session
        api_key (str): API key đã được verify
        
    Returns:
        schemas.IngestResponse: ID của bản ghi vừa tạo và status "ok"
    """
    # Lấy thời gian server thay vì dùng timestamp từ ESP32 (đảm bảo đồng bộ)
    server_timestamp = int(datetime.now().timestamp())
    
    # Tạo entity mới từ payload
    entity = models.Reading(
        device_id=payload.device_id,
        timestamp=server_timestamp,  # Sử dụng thời gian server
        temperature=payload.temperature,
        smoke_value=payload.smoke_value,
        fire_value=payload.fire_value,
        temp_alert=payload.temp_alert,
        smoke_alert=payload.smoke_alert,
        fire_alert=payload.fire_alert,
    )
    db.add(entity)
    db.commit()
    db.refresh(entity)

    # Gửi cảnh báo Telegram bất đồng bộ nếu có bất kỳ module nào cảnh báo
    try:
        if payload.temp_alert or payload.smoke_alert or payload.fire_alert:
            message = format_alert_message(payload, server_timestamp)
            background_tasks.add_task(send_telegram_message, message)
    except Exception as e:
        print(f" Failed to schedule Telegram alert: {e}")

    print(f" Received data from {payload.device_id} at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    return schemas.IngestResponse(id=entity.id, status="ok")


@app.get("/api/readings", response_model=list[schemas.ReadingOut])
def list_readings(limit: int = 100, db: Session = Depends(get_db)):
    """
    Lấy danh sách các bản ghi cảm biến từ database.
    
    Args:
        limit (int): Số lượng bản ghi tối đa (mặc định 100)
        db (Session): Database session
        
    Returns:
        list[schemas.ReadingOut]: Danh sách bản ghi, sắp xếp theo thời gian mới nhất
    """
    rows = db.query(models.Reading).order_by(models.Reading.created_at.desc()).limit(limit).all()
    return [schemas.ReadingOut.from_orm(r) for r in rows]


@app.get("/api/readings/latest", response_model=schemas.ReadingOut | None)
def latest_reading(db: Session = Depends(get_db)):
    """
    Lấy bản ghi cảm biến mới nhất.
    
    Args:
        db (Session): Database session
        
    Returns:
        schemas.ReadingOut | None: Bản ghi mới nhất hoặc None nếu database trống
    """
    r = db.query(models.Reading).order_by(models.Reading.created_at.desc()).first()
    return schemas.ReadingOut.from_orm(r) if r else None


# --------------------------------------------------------------------
# Firmware Management APIs
# --------------------------------------------------------------------

@app.get("/api/firmware/check")
def check_firmware_update(current_version: str, device_id: str = "battery_monitor_001"):
    """
    Kiểm tra xem có firmware mới để cập nhật không.
    
    ESP32 gọi endpoint này định kỳ để kiểm tra phiên bản mới.
    Server so sánh phiên bản hiện tại với phiên bản trong latest.json.
    
    Format version: "1.0.0" hoặc "1.0.0-build123"
    So sánh: version number trước, sau đó build number nếu version bằng nhau.
    
    Args:
        current_version (str): Phiên bản hiện tại của ESP32 (ví dụ: "1.0.0-build7")
        device_id (str): ID thiết bị (mặc định "battery_monitor_001")
        
    Returns:
        dict: Thông tin về firmware update:
            - update_available (bool): Có firmware mới không?
            - latest_version (str): Phiên bản mới nhất
            - latest_build (int): Build number mới nhất
            - download_url (str): URL tải firmware
            - file_size (int): Kích thước file
            - release_notes (str): Ghi chú phiên bản
            - checksum (str): MD5 checksum
            - error (str): Thông báo lỗi nếu có
    """
    firmware_info_file = "firmware/latest.json"
    
    # Kiểm tra file latest.json có tồn tại không
    if not os.path.exists(firmware_info_file):
        return {"update_available": False, "message": "No firmware info found"}
    
    try:
        # Đọc thông tin firmware từ JSON
        with open(firmware_info_file, 'r') as f:
            firmware_info = json.load(f)
        
        latest_version = firmware_info.get("version", "0.0.0")
        latest_build = firmware_info.get("build", 0)
        
        # Parse phiên bản hiện tại (format: "1.0.0" hoặc "1.0.0-build123")
        current_parts = current_version.split("-")
        current_ver = current_parts[0]  # Phần version (ví dụ: "1.0.0")
        current_build = int(current_parts[1].replace("build", "")) if len(current_parts) > 1 else 0
        
        # Hàm helper để convert version string sang tuple để so sánh
        def version_tuple(version_str):
            """
            Convert version string sang tuple để so sánh semantic versioning.
            
            Ví dụ: "1.2.3" -> (1, 2, 3)
            Nếu parse lỗi, trả về (0, 0, 0)
            """
            try:
                parts = version_str.split('.')
                return tuple(int(part) for part in parts)
            except ValueError:
                return (0, 0, 0)
        
        # So sánh version
        latest_tuple = version_tuple(latest_version)
        current_tuple = version_tuple(current_ver)
        
        # Có firmware mới nếu: version lớn hơn HOẶC version bằng nhau nhưng build lớn hơn
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
    """
    Tải file firmware binary để ESP32 cập nhật OTA.
    
    ESP32 gọi endpoint này sau khi phát hiện có firmware mới từ /api/firmware/check.
    Server trả về file .bin với media type application/octet-stream.
    
    Args:
        version (str): Phiên bản firmware (ví dụ: "1.0.0")
        
    Returns:
        FileResponse: File firmware binary
        
    Raises:
        HTTPException: 404 nếu file không tồn tại
    """
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
    """
    Upload firmware mới lên server (yêu cầu API key).
    
    Admin dùng endpoint này để upload file firmware mới.
    Server sẽ:
    1. Lưu file .bin vào thư mục firmware/
    2. Tính MD5 checksum
    3. Cập nhật latest.json với thông tin mới
    
    Args:
        file (UploadFile): File firmware .bin
        version (str): Phiên bản (ví dụ: "1.0.0")
        build (int): Build number
        release_notes (str): Ghi chú phiên bản
        api_key (str): API key đã được verify
        
    Returns:
        dict: Thông tin firmware vừa upload:
            - status: "success"
            - message: Thông báo thành công
            - firmware_info: Dict chứa thông tin chi tiết
            
    Raises:
        HTTPException: 400 nếu file không phải .bin
    """
    # Validate file extension
    if not file.filename.endswith('.bin'):
        raise HTTPException(status_code=400, detail="Only .bin files are allowed")
    
    # Tạo thư mục firmware nếu chưa tồn tại
    os.makedirs("firmware", exist_ok=True)
    
    # Lưu file firmware
    firmware_filename = f"battery_monitor_v{version}.bin"
    firmware_path = f"firmware/{firmware_filename}"
    
    with open(firmware_path, "wb") as buffer:
        content = await file.read()
        buffer.write(content)
    
    # Tính MD5 checksum để ESP32 verify file sau khi tải
    import hashlib
    checksum = hashlib.md5(content).hexdigest()
    
    # Tạo thông tin firmware và cập nhật latest.json
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
    
    # Ghi latest.json để ESP32 biết có firmware mới
    with open("firmware/latest.json", "w") as f:
        json.dump(firmware_info, f, indent=2)
    
    return {
        "status": "success",
        "message": f"Firmware v{version} uploaded successfully",
        "firmware_info": firmware_info
    }


@app.get("/api/firmware/info")
def get_firmware_info():
    """
    Lấy thông tin firmware hiện tại từ latest.json.
    
    Endpoint này dùng để xem thông tin firmware mới nhất mà không cần so sánh version.
    
    Returns:
        dict: Thông tin firmware từ latest.json hoặc {"error": "..."} nếu không tìm thấy
    """
    firmware_info_file = "firmware/latest.json"
    
    if not os.path.exists(firmware_info_file):
        return {"error": "No firmware info found"}
    
    try:
        with open(firmware_info_file, 'r') as f:
            return json.load(f)
    except Exception as e:
        return {"error": str(e)}