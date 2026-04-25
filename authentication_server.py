import hashlib
import json
import socket
from pathlib import Path


HOST = "127.0.0.1"
USC_ID_SUFFIX = 818
AUTH_UDP_PORT = 21000 + USC_ID_SUFFIX  # 21818


def sha256_hash(text: str) -> str:
    text = text.strip()
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def hash_suffix(sha256_hex: str) -> str:
    return sha256_hex[-5:]


def load_users(users_path: Path) -> set[tuple[str, str]]:
    """
    users.txt lines:
      <hashed_username> <hashed_password>
    """
    entries: set[tuple[str, str]] = set()
    if not users_path.exists():
        return entries
    for raw in users_path.read_text(encoding="utf-8").splitlines():
        raw = raw.strip()
        if not raw:
            continue
        parts = raw.split()
        if len(parts) >= 2:
            entries.add((parts[0], parts[1]))
    return entries


def main() -> None:
    users_path = Path("users.txt")
    users = load_users(users_path)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((HOST, AUTH_UDP_PORT))

    print(f"Authentication Server is up and running using UDP on\nport {AUTH_UDP_PORT}.")

    while True:
        data, addr = sock.recvfrom(65535)
        try:
            msg = json.loads(data.decode("utf-8"))
        except Exception:
            continue

        if msg.get("type") != "auth_req":
            continue

        u_hash = str(msg.get("u_hash", ""))
        p_hash = str(msg.get("p_hash", ""))
        suffix = hash_suffix(u_hash)

        print(
            "Authentication Server has received an authentication\n"
            f"request for a user with hash suffix: {suffix}."
        )

        ok = (u_hash, p_hash) in users
        if ok:
            print(f"Authentication succeeded for a user with hash suffix:\n{suffix}.")
        else:
            print(f"Authentication failed for a user with hash suffix:\n{suffix}.")

        resp = {"type": "auth_resp", "ok": ok, "u_hash": u_hash}
        sock.sendto(json.dumps(resp).encode("utf-8"), addr)
        print("The Authentication Server has sent the authentication\nresult to the Hospital Server.")


if __name__ == "__main__":
    main()

