from pydantic import BaseModel
from typing import Optional
from datetime import datetime


class IngestRequest(BaseModel):
    timestamp: Optional[int] = None  # Không bắt buộc, server sẽ tự tạo
    temperature: float
    smoke_value: int
    fire_value: int  # KY-026 10-bit (0..1023)
    temp_alert: bool = False
    smoke_alert: bool = False
    fire_alert: bool = False
    device_id: str


class IngestResponse(BaseModel):
    id: int
    status: str


class ReadingOut(BaseModel):
    id: int
    device_id: str
    timestamp: int  # Unix timestamp (hiện đang dùng thời gian server)
    temperature: float
    smoke_value: int
    fire_value: int
    temp_alert: bool
    smoke_alert: bool
    fire_alert: bool
    created_at: datetime

    class Config:
        from_attributes = True


class FirmwareCheckRequest(BaseModel):
    current_version: str
    device_id: str = "battery_monitor_001"


class FirmwareCheckResponse(BaseModel):
    update_available: bool
    latest_version: Optional[str] = None
    latest_build: Optional[int] = None
    current_version: Optional[str] = None
    download_url: Optional[str] = None
    file_size: Optional[int] = None
    release_notes: Optional[str] = None
    checksum: Optional[str] = None
    message: Optional[str] = None
    error: Optional[str] = None


class FirmwareUploadResponse(BaseModel):
    status: str
    message: str
    firmware_info: dict


class FirmwareInfo(BaseModel):
    version: str
    build: int
    download_url: str
    file_size: int
    release_notes: str
    checksum: str
    uploaded_at: str
    filename: str