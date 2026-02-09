import json
import os
import threading
import time
from crypto_utils import sign, constant_time_equal
from telemetry import record_event, inc

_SESSIONS = {}          # session_id -> dict
_LOCK = threading.Lock()
_SWEEPER = None

def start_sweeper(interval_seconds: int = 2) -> None:
    """Start a background thread that removes expired sessions."""
    global _SWEEPER
    if _SWEEPER is not None:
        return

    def loop():
        while True:
            time.sleep(interval_seconds)
            now = time.time()
            with _LOCK:
                expired = [sid for sid, s in _SESSIONS.items() if s.get("expires_at", 0) <= now]
                for sid in expired:
                    _SESSIONS.pop(sid, None)
                    record_event("session.expired", {"sid": sid})

    _SWEEPER = threading.Thread(target=loop, daemon=True)
    _SWEEPER.start()

def create_session(session_id: str, user_id: str, secret: bytes, ttl_seconds: int = 10) -> str:
    """
    Create a session record and return an auth token.
    Token format: "<session_id>.<signature>"
    """
    token = f"{session_id}"
    sig = sign(token, secret)
    with _LOCK:
        _SESSIONS[session_id] = {
            "user_id": user_id,
            "created_at": time.time(),
            "expires_at": time.time() + ttl_seconds,
        }
    inc("writes")
    record_event("session.created", {"sid": session_id, "user": user_id})
    return f"{token}.{sig}"

def validate_token(token_with_sig: str, secret: bytes) -> str | None:
    """
    Validate token signature and expiration.
    Return user_id if valid else None.
    """
    if "." not in token_with_sig:
        inc("fails")
        return None

    token, sig = token_with_sig.split(".", 1)
    expected = sign(token, secret)
    if not constant_time_equal(sig, expected):
        inc("fails")
        record_event("session.invalid_sig", {"sid": token})
        return None

    with _LOCK:
        s = _SESSIONS.get(token)
        if not s:
            inc("fails")
            return None
        if s["expires_at"] <= time.time():
            inc("fails")
            record_event("session.expired_seen", {"sid": token})
            return None
        inc("reads")
        return s["user_id"]

def save_to_disk(path: str) -> None:
    """Persist sessions to disk as JSON."""
    with _LOCK:
        data = dict(_SESSIONS)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f)
    record_event("session.saved", {"path": path})

def load_from_disk(path: str) -> None:
    """Load sessions from disk JSON, overwriting in-memory store."""
    if not os.path.exists(path):
        record_event("session.load_missing", {"path": path})
        return
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    with _LOCK:
        _SESSIONS.clear()
        _SESSIONS.update(data)
    record_event("session.loaded", {"path": path})