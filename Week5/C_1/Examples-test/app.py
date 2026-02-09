import os
import time
from session_store import (
    start_sweeper,
    create_session,
    validate_token,
    save_to_disk,
    load_from_disk,
)

SECRET = b"supersecret"

def main():
    start_sweeper(1)

    token = create_session("abc123", "user42", SECRET, 5)
    print("Token:", token)

    user = validate_token(token, SECRET)
    print("Validated user:", user)

    save_to_disk("sessions.json")
    load_from_disk("sessions.json")

    time.sleep(2)

if __name__ == "__main__":
    main()