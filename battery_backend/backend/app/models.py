# -*- coding: utf-8 -*-
"""
@file models.py
@brief Định nghĩa SQLAlchemy models cho database schema.

Mỗi model tương ứng với một bảng trong database, định nghĩa cấu trúc dữ liệu
và các ràng buộc (constraints, indexes, defaults).
"""
from sqlalchemy import Column, Integer, String, Float, Boolean, DateTime, BigInteger, func
from .db import Base


class Reading(Base):
    """
    Model đại diện cho một bản ghi dữ liệu cảm biến từ thiết bị ESP32.
    
    Bảng này lưu trữ:
    - Dữ liệu cảm biến: nhiệt độ (DS18B20), khí độc (MQ-135), lửa (KY-026)
    - Trạng thái cảnh báo: temp_alert, smoke_alert, fire_alert
    - Metadata: device_id, timestamp, created_at
    
    Attributes:
        id (int): Primary key, tự động tăng
        device_id (str): ID của thiết bị ESP32 (tối đa 64 ký tự, có index)
        timestamp (int): Unix timestamp (BigInteger, có index) - hiện dùng thời gian server
        temperature (float): Nhiệt độ đo được từ DS18B20 (°C)
        smoke_value (int): Giá trị ADC từ cảm biến MQ-135 (0-4095, 12-bit)
        fire_value (int): Giá trị ADC từ cảm biến KY-026 (0-1023, 10-bit)
        temp_alert (bool): Cờ cảnh báo nhiệt độ cao (mặc định False)
        smoke_alert (bool): Cờ cảnh báo khí độc (mặc định False)
        fire_alert (bool): Cờ cảnh báo lửa (mặc định False)
        smoke_connected (bool): Trạng thái kết nối MQ-135 (legacy, có thể null)
        mq2_preheated (bool): Trạng thái preheat MQ-2 (legacy, có thể null)
        fire_detected (bool): Trạng thái phát hiện lửa (legacy, có thể null)
        alert_active (bool): Trạng thái cảnh báo tổng thể (legacy, có thể null)
        created_at (datetime): Thời gian tạo bản ghi (tự động, UTC timezone)
    """
    __tablename__ = "readings"

    # Primary key: ID tự động tăng, có index để tìm kiếm nhanh
    id = Column(Integer, primary_key=True, index=True)
    
    # Device ID: tối đa 64 ký tự, có index để query theo device
    device_id = Column(String(64), index=True)
    
    # Unix timestamp: BigInteger để chứa timestamp lớn, có index
    # Lưu ý: hiện tại server tự tạo timestamp, không dùng từ ESP32
    timestamp = Column(BigInteger, index=True)
    
    # Dữ liệu cảm biến
    temperature = Column(Float)  # Nhiệt độ từ DS18B20 (°C)
    smoke_value = Column(Integer)  # Giá trị MQ-135 (0-4095)
    fire_value = Column(Integer)  # Giá trị KY-026 (0-1023, 10-bit)
    
    # Cờ cảnh báo: mặc định False, được set khi vượt ngưỡng
    temp_alert = Column(Boolean, default=False)
    smoke_alert = Column(Boolean, default=False)
    fire_alert = Column(Boolean, default=False)
    
    # Các trường legacy (có thể null, không dùng trong logic mới)
    smoke_connected = Column(Boolean)
    mq2_preheated = Column(Boolean)
    fire_detected = Column(Boolean)
    alert_active = Column(Boolean)
    
    # Timestamp tự động: server tạo khi insert, có timezone UTC
    created_at = Column(DateTime(timezone=True), server_default=func.now())


