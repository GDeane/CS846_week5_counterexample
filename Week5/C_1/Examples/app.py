import os
from session_store import start_sweeper, create_session, validate_token, save_to_disk, load_from_disk

def main():
    secret = os.environ.get("APP_SECRET", "dev-secret").encode("utf-8")
    start_sweeper()

    token = create_session("s1", "u123", secret, ttl_seconds=3)
    print("Token:", token)

    print("Validate:", validate_token(token, secret))

    save_to_disk("sessions.json")
    load_from_disk("sessions.json")

if __name__ == "__main__":
    main()