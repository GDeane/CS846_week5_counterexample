import hashlib
import hmac

def sign(data: str, secret: bytes) -> str:
    return hmac.new(secret, data.encode(), hashlib.sha256).hexdigest()

def constant_time_equal(a: str, b: str) -> bool:
    return hmac.compare_digest(a, b)