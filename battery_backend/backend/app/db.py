# -*- coding: utf-8 -*-
"""
@file db.py
@brief Cấu hình kết nối database và quản lý session SQLAlchemy.

Module này thiết lập engine, session factory và dependency injection cho FastAPI.
Hỗ trợ SQLite (mặc định) và các database khác qua biến môi trường DATABASE_URL.
"""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base
import os
import pathlib

# --------------------------------------------------------------------
# Cấu hình đường dẫn database mặc định
# --------------------------------------------------------------------
# Đường dẫn mặc định: ./data/battery.db (khớp với docker volume)
# Tự động tạo thư mục data nếu chưa tồn tại
default_db_path = pathlib.Path("./data/battery.db")
default_db_path.parent.mkdir(parents=True, exist_ok=True)

# Lấy URL database từ biến môi trường, fallback về SQLite nếu không có
DB_URL = os.getenv("DATABASE_URL", f"sqlite:///{default_db_path.as_posix()}")

# --------------------------------------------------------------------
# Khởi tạo SQLAlchemy Engine và Session Factory
# --------------------------------------------------------------------
# Tạo engine với cấu hình phù hợp:
# - SQLite: bật check_same_thread=False để cho phép multi-thread
# - Database khác: dùng cấu hình mặc định
engine = create_engine(
    DB_URL,
    connect_args={"check_same_thread": False} if DB_URL.startswith("sqlite") else {}
)

# Session factory: autocommit=False, autoflush=False để kiểm soát transaction thủ công
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

# Base class cho tất cả models (dùng declarative_base)
Base = declarative_base()


def get_db():
    """
    Dependency injection function cho FastAPI để lấy database session.
    
    FastAPI sẽ tự động gọi hàm này khi endpoint cần `db: Session = Depends(get_db)`.
    Đảm bảo session được đóng sau khi request hoàn tất.
    
    Yields:
        Session: SQLAlchemy database session
        
    Example:
        @app.get("/api/readings")
        def list_readings(db: Session = Depends(get_db)):
            return db.query(Reading).all()
    """
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


