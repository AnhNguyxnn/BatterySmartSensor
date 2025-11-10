# -*- coding: utf-8 -*-
"""
@file schemas.py
@brief Định nghĩa Pydantic schemas cho request/response validation.

Schemas đảm bảo:
- Request data từ ESP32 được validate đúng format
- Response data được serialize đúng cấu trúc
- Type safety và auto-documentation cho FastAPI
"""
from pydantic import BaseModel
from typing import Optional
from datetime import datetime


class IngestRequest(BaseModel):
    """
    Schema cho request POST /api/ingest từ ESP32.
    
    ESP32 gửi dữ liệu cảm biến lên server, server sẽ tự tạo timestamp.
    
    Attributes:
        timestamp (Optional[int]): Unix timestamp từ ESP32 (không bắt buộc, server tự tạo)
        temperature (float): Nhiệt độ từ DS18B20 (°C)
        smoke_value (int): Giá trị ADC từ MQ-135 (0-4095)
        fire_value (int): Giá trị ADC từ KY-026 (0-1023, 10-bit)
        temp_alert (bool): Cờ cảnh báo nhiệt độ cao (mặc định False)
        smoke_alert (bool): Cờ cảnh báo khí độc (mặc định False)
        fire_alert (bool): Cờ cảnh báo lửa (mặc định False)
        device_id (str): ID của thiết bị ESP32 (bắt buộc)
    """
    timestamp: Optional[int] = None  # Không bắt buộc, server sẽ tự tạo
    temperature: float
    smoke_value: int
    fire_value: int  # KY-026 10-bit (0..1023)
    temp_alert: bool = False
    smoke_alert: bool = False
    fire_alert: bool = False
    device_id: str


class IngestResponse(BaseModel):
    """
    Schema cho response từ POST /api/ingest.
    
    Attributes:
        id (int): ID của bản ghi vừa được tạo trong database
        status (str): Trạng thái xử lý ("ok" nếu thành công)
    """
    id: int
    status: str


class ReadingOut(BaseModel):
    """
    Schema cho response khi trả về dữ liệu cảm biến từ database.
    
    Dùng cho các endpoint GET /api/readings và GET /api/readings/latest.
    Tự động convert từ SQLAlchemy model sang Pydantic model.
    
    Attributes:
        id (int): Primary key của bản ghi
        device_id (str): ID thiết bị
        timestamp (int): Unix timestamp (server tạo)
        temperature (float): Nhiệt độ (°C)
        smoke_value (int): Giá trị MQ-135
        fire_value (int): Giá trị KY-026 (10-bit)
        temp_alert (bool): Cờ cảnh báo nhiệt độ
        smoke_alert (bool): Cờ cảnh báo khí độc
        fire_alert (bool): Cờ cảnh báo lửa
        created_at (datetime): Thời gian tạo bản ghi (UTC)
    """
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
        # Cho phép convert từ SQLAlchemy model (ORM) sang Pydantic model
        from_attributes = True


class FirmwareCheckRequest(BaseModel):
    """
    Schema cho request kiểm tra firmware update (query params).
    
    Attributes:
        current_version (str): Phiên bản hiện tại của ESP32 (ví dụ: "1.0.0-build7")
        device_id (str): ID thiết bị (mặc định "battery_monitor_001")
    """
    current_version: str
    device_id: str = "battery_monitor_001"


class FirmwareCheckResponse(BaseModel):
    """
    Schema cho response kiểm tra firmware update.
    
    Attributes:
        update_available (bool): Có firmware mới không?
        latest_version (Optional[str]): Phiên bản mới nhất (nếu có)
        latest_build (Optional[int]): Build number mới nhất
        current_version (Optional[str]): Phiên bản hiện tại
        download_url (Optional[str]): URL tải firmware (nếu có update)
        file_size (Optional[int]): Kích thước file firmware (bytes)
        release_notes (Optional[str]): Ghi chú phiên bản
        checksum (Optional[str]): MD5 checksum của file firmware
        message (Optional[str]): Thông báo (ví dụ: "Firmware is up to date")
        error (Optional[str]): Thông báo lỗi (nếu có)
    """
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
    """
    Schema cho response sau khi upload firmware thành công.
    
    Attributes:
        status (str): Trạng thái ("success")
        message (str): Thông báo thành công
        firmware_info (dict): Thông tin chi tiết firmware vừa upload
    """
    status: str
    message: str
    firmware_info: dict


class FirmwareInfo(BaseModel):
    """
    Schema cho thông tin firmware trong latest.json.
    
    Attributes:
        version (str): Phiên bản (ví dụ: "1.0.0")
        build (int): Build number
        download_url (str): URL tải firmware
        file_size (int): Kích thước file (bytes)
        release_notes (str): Ghi chú phiên bản
        checksum (str): MD5 checksum
        uploaded_at (str): Thời gian upload (ISO format)
        filename (str): Tên file firmware
    """
    version: str
    build: int
    download_url: str
    file_size: int
    release_notes: str
    checksum: str
    uploaded_at: str
    filename: str