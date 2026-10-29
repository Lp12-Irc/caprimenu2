# language: Python 3.11+, file: database.py
import sqlite3
from contextlib import contextmanager
from typing import Optional

class Database:
    def __init__(self, path: str):
        self._path = path

    @contextmanager
    def _conn(self):
        conn = sqlite3.connect(self._path, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        try:
            yield conn
            conn.commit()
        finally:
            conn.close()

    def init(self):
        with self._conn() as c:
            c.execute("""
                CREATE TABLE IF NOT EXISTS keys (
                    id          INTEGER PRIMARY KEY AUTOINCREMENT,
                    key_value   TEXT    NOT NULL UNIQUE,
                    created_at  TEXT    NOT NULL,
                    expires_at  TEXT    NOT NULL DEFAULT 'never',
                    hwid_bound  TEXT    DEFAULT '',
                    status      TEXT    NOT NULL DEFAULT 'active',
                    note        TEXT    DEFAULT ''
                )
            """)
            c.execute("""
                CREATE TABLE IF NOT EXISTS auth_log (
                    id         INTEGER PRIMARY KEY AUTOINCREMENT,
                    key_value  TEXT,
                    hwid       TEXT,
                    ip         TEXT,
                    result     TEXT,
                    ts         TEXT
                )
            """)

    def get_key(self, key_value: str) -> Optional[dict]:
        with self._conn() as c:
            row = c.execute("SELECT * FROM keys WHERE key_value=?", (key_value,)).fetchone()
            return dict(row) if row else None

    def bind_hwid(self, key_value: str, hwid: str):
        with self._conn() as c:
            c.execute("UPDATE keys SET hwid_bound=? WHERE key_value=?", (hwid, key_value))

    def set_status(self, key_value: str, status: str):
        with self._conn() as c:
            c.execute("UPDATE keys SET status=? WHERE key_value=?", (status, key_value))

    def log_auth(self, key: str, hwid: str, ip: str, result: str):
        from datetime import datetime, timezone
        with self._conn() as c:
            c.execute("INSERT INTO auth_log(key_value,hwid,ip,result,ts) VALUES(?,?,?,?,?)",
                      (key, hwid, ip, result, datetime.now(timezone.utc).isoformat()))
