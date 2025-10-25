from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base
import os
import pathlib

# Default DB location inside a persisted data directory (matches docker volume ./data)
# Ensure data directory exists for sqlite default
default_db_path = pathlib.Path("./data/battery.db")
default_db_path.parent.mkdir(parents=True, exist_ok=True)
DB_URL = os.getenv("DATABASE_URL", f"sqlite:///{default_db_path.as_posix()}")

engine = create_engine(DB_URL, connect_args={"check_same_thread": False} if DB_URL.startswith("sqlite") else {})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


