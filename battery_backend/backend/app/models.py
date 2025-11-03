from sqlalchemy import Column, Integer, String, Float, Boolean, DateTime, BigInteger, func
from .db import Base


class Reading(Base):
    __tablename__ = "readings"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), index=True)
    timestamp = Column(BigInteger, index=True)  # Unix timestamp (hiện đang dùng thời gian server)
    temperature = Column(Float)
    smoke_value = Column(Integer)
    smoke_connected = Column(Boolean)
    mq2_preheated = Column(Boolean)
    fire_detected = Column(Boolean)
    alert_active = Column(Boolean)
    created_at = Column(DateTime(timezone=True), server_default=func.now())


