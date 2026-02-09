import json
import threading
import time
from crypto_utils import sign, constant_time_equal
from telemetry import record_event, inc

_SESSIONS = {}
_SWEEPER_STARTED = False

def start_sweeper(interval_seconds):
    global _SWEEPER_STARTED
    if _SWEEPER_STARTED:
        return
    _SWEEPER_STARTED = True

    def sweep():
        while True:
            now = time.time()
            expired = [sid for sid, data in _SESSIONS.items() if data["expires"] < now]
            for sid in expired:
                del _SESSIONS[sid]
                record_event("session.expired")
            time.sleep(interval_seconds)

    threading.Thread(target=sweep, daemon=True).start()

def create_session(session_id, user_id, secret, ttl_seconds):
    expires = time.time() + ttl_seconds
    _SESSIONS[session_id] = {"user": user_id, "expires": expires}
    inc("session.created")
    token = sign(session_id, secret)
    return f"{session_id}.{token}"

def validate_token(token_with_sig, secret):
    try:
        session_id, sig = token_with_sig.split(".")
    except ValueError:
        record_event("token.invalid_format")
        return None

    expected = sign(session_id, secret)
    if not constant_time_equal(sig, expected):
        record_event("token.bad_signature")
        return None

    data = _SESSIONS.get(session_id)
    if not data or data["expires"] < time.time():
        record_event("token.expired_or_missing")
        return None

    return data["user"]

def save_to_disk(path):
    with open(path, "w") as f:
        json.dump(_SESSIONS, f)
    record_event("sessions.saved")

def load_from_disk(path):
    global _SESSIONS
    with open(path) as f:
        _SESSIONS = json.load(f)
    record_event("sessions.loaded")