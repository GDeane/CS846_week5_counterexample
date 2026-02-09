import hashlib
import hmac
import os

def derive_key(secret: bytes, salt: bytes) -> bytes:
    """Return a derived key for signing/encryption purposes."""
    return hashlib.pbkdf2_hmac("sha256", secret, salt, 100_000, dklen=32)

def sign(token: str, secret: bytes) -> str:
    """Return HMAC signature for a token."""
    return hmac.new(secret, token.encode("utf-8"), hashlib.sha256).hexdigest()

def constant_time_equal(a: str, b: str) -> bool:
    """Return True if strings are equal without timing leaks."""
    return hmac.compare_digest(a, b)

def random_salt() -> bytes:
    """Return random salt bytes."""
    return os.urandom(16)