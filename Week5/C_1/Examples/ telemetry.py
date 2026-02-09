import time

_EVENTS = []
_COUNTERS = {"writes": 0, "reads": 0, "fails": 0}

def record_event(name: str, meta: dict | None = None) -> None:
    """Record a telemetry event in memory."""
    _EVENTS.append({"t": time.time(), "name": name, "meta": meta or {}})

def inc(counter: str) -> None:
    """Increment a named counter."""
    if counter not in _COUNTERS:
        _COUNTERS[counter] = 0
    _COUNTERS[counter] += 1

def snapshot() -> dict:
    """Return a copy of counters and recent events."""
    return {"counters": dict(_COUNTERS), "events": list(_EVENTS[-50:])}