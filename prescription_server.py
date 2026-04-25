import json
import socket
from pathlib import Path


HOST = "127.0.0.1"
USC_ID_SUFFIX = 818
PRESC_UDP_PORT = 22000 + USC_ID_SUFFIX  # 22818


def hash_suffix(sha256_hex: str) -> str:
    return sha256_hex[-5:]


def load_prescriptions(path: Path) -> list[tuple[str, str, str, str]]:
    """
    prescriptions.txt columns:
      <doctor_name> <hashed_patient> <treatment> <frequency>
    """
    out: list[tuple[str, str, str, str]] = []
    if not path.exists():
        return out
    for raw in path.read_text(encoding="utf-8").splitlines():
        s = raw.strip()
        if not s:
            continue
        parts = s.split()
        if len(parts) >= 4:
            out.append((parts[0], parts[1], parts[2], parts[3]))
    return out


def save_prescriptions(path: Path, rows: list[tuple[str, str, str, str]]) -> None:
    lines = [f"{d} {p} {t} {f}" for (d, p, t, f) in rows]
    path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")


def upsert_prescription(
    rows: list[tuple[str, str, str, str]],
    doctor: str,
    patient_hash: str,
    treatment: str,
    frequency: str,
) -> None:
    # Keep it simple: remove existing entries for patient, then append new.
    rows[:] = [r for r in rows if r[1] != patient_hash]
    rows.append((doctor, patient_hash, treatment, frequency))


def find_prescription(
    rows: list[tuple[str, str, str, str]],
    patient_hash: str,
) -> tuple[str, str, str] | None:
    for doctor, p_hash, treatment, freq in rows:
        if p_hash == patient_hash:
            return (doctor, treatment, freq)
    return None


def main() -> None:
    path = Path("prescriptions.txt")
    rows = load_prescriptions(path)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((HOST, PRESC_UDP_PORT))

    print(f"Prescription Server is up and running using UDP on port\n{PRESC_UDP_PORT}.")

    while True:
        data, addr = sock.recvfrom(65535)
        try:
            msg = json.loads(data.decode("utf-8"))
        except Exception:
            continue

        mtype = msg.get("type")

        if mtype == "prescribe_req":
            doctor = str(msg.get("doctor", ""))
            patient_hash = str(msg.get("patient_hash", ""))
            treatment = str(msg.get("treatment", ""))
            frequency = str(msg.get("frequency", ""))

            print(
                "Prescription Server has received a request from\n"
                f"{doctor} to prescribe the user with hash suffix\n{hash_suffix(patient_hash)}."
            )

            upsert_prescription(rows, doctor, patient_hash, treatment, frequency)
            save_prescriptions(path, rows)
            print(f"Successfully saved the prescription details for user with\nhash suffix: {hash_suffix(patient_hash)}.")

            resp = {"type": "prescribe_resp", "ok": True}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)

        elif mtype == "view_prescription_req":
            patient_hash = str(msg.get("patient_hash", ""))
            print(
                "The prescription server has received a request to view the\n"
                f"prescription for the user with hash suffix: {hash_suffix(patient_hash)}."
            )
            found = find_prescription(rows, patient_hash)
            if found is None or found[2] == "None":
                print("There are no current prescriptions for this user.")
                resp = {"type": "view_prescription_resp", "ok": False}
            else:
                print("A prescription exists for this user.")
                doctor, treatment, freq = found
                resp = {"type": "view_prescription_resp", "ok": True, "doctor": doctor, "treatment": treatment, "frequency": freq}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)


if __name__ == "__main__":
    main()

