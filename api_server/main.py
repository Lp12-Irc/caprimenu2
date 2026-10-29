# language: Python 3.11+, file: main.py
# pip install fastapi uvicorn[standard] python-dateutil
# Run: uvicorn main:app --host 0.0.0.0 --port 443 --ssl-keyfile key.pem --ssl-certfile cert.pem

from fastapi import FastAPI, Request
from pydantic import BaseModel
from database import Database
from datetime import datetime, timezone
import logging

logging.basicConfig(level=logging.INFO)
app = FastAPI(docs_url=None, redoc_url=None)  # disable public docs
db  = Database("keys.db")
db.init()

class ValidateReq(BaseModel):
    key:  str
    hwid: str

@app.post("/validate")
async def validate(req: ValidateReq, request: Request):
    ip = request.client.host if request.client else "unknown"
    logging.info(f"validate | ip={ip} key={req.key[:8]}... hwid={req.hwid[:8]}...")

    key_row = db.get_key(req.key)

    if key_row is None:
        return {"valid": False, "reason": "not_found", "message": "Key not found."}

    if key_row["status"] == "revoked":
        return {"valid": False, "reason": "revoked", "message": "Key revoked."}

    if key_row["status"] == "expired":
        return {"valid": False, "reason": "expired", "message": "Key expired."}

    # Check expiry
    exp = key_row["expires_at"]
    if exp != "never":
        try:
            if datetime.fromisoformat(exp).replace(tzinfo=timezone.utc) < datetime.now(timezone.utc):
                db.set_status(req.key, "expired")
                return {"valid": False, "reason": "expired", "message": "Key expired."}
        except ValueError:
            pass

    # HWID check / bind
    bound = key_row["hwid_bound"] or ""
    if not bound:
        db.bind_hwid(req.key, req.hwid)
    elif bound != req.hwid:
        return {"valid": False, "reason": "hwid_mismatch", "message": "HWID mismatch."}

    return {
        "valid": True,
        "expires_at": exp,
        "message": "OK"
    }

# Health check (no auth)
@app.get("/health")
async def health():
    return {"status": "ok"}
