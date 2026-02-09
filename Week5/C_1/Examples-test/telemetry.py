_counters = {}

def inc(name: str):
    _counters[name] = _counters.get(name, 0) + 1
    print(f"[counter] {name} = {_counters[name]}")

def record_event(name: str):
    print(f"[event] {name}")