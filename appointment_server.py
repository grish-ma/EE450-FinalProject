import json
import socket
from dataclasses import dataclass
from pathlib import Path


HOST = "127.0.0.1"
USC_ID_SUFFIX = 818
APPT_UDP_PORT = 23000 + USC_ID_SUFFIX  # 23818


def hash_suffix(sha256_hex: str) -> str:
    return sha256_hex[-5:]


@dataclass(frozen=True)
class TimeBlock:
    hh: int
    mm: int

    @staticmethod
    def parse(hhmm: str) -> "TimeBlock | None":
        try:
            hh_s, mm_s = hhmm.split(":")
            hh = int(hh_s)
            mm = int(mm_s)
            if mm != 0:
                return None
            return TimeBlock(hh=hh, mm=mm)
        except Exception:
            return None

    def as_str(self) -> str:
        return f"{self.hh:02d}:{self.mm:02d}"


def allowed_timeblock(tb: TimeBlock) -> bool:
    # 8 blocks: 09:00 .. 16:00 inclusive (17:00 not inclusive)
    return 9 <= tb.hh <= 16 and tb.mm == 0


def iter_doctor_blocks(lines: list[str]) -> dict[str, list[str]]:
    """
    appointments.txt:
      doctor_name
      <start_time> [<hashed_patient> <illnessID>]
      ...
    Returns: doctor -> list of raw block lines (without doctor headers)
    """
    out: dict[str, list[str]] = {}
    cur_doctor: str | None = None
    for raw in lines:
        s = raw.strip()
        if not s:
            continue
        parts = s.split()
        if len(parts) == 1 and ":" not in parts[0]:
            cur_doctor = parts[0]
            out.setdefault(cur_doctor, [])
            continue
        if cur_doctor is None:
            continue
        out[cur_doctor].append(s)
    return out


def load_appointments(path: Path) -> dict[str, list[str]]:
    if not path.exists():
        return {}
    return iter_doctor_blocks(path.read_text(encoding="utf-8").splitlines())


def save_appointments(path: Path, doctors: dict[str, list[str]]) -> None:
    # Preserve simple format; order is arbitrary but stable by doctor name.
    lines: list[str] = []
    for doctor in sorted(doctors.keys()):
        lines.append(doctor)
        lines.extend(doctors[doctor])
    path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")


def blocks_available(block_lines: list[str]) -> list[str]:
    avail: list[str] = []
    for line in block_lines:
        parts = line.split()
        if len(parts) == 1:
            avail.append(parts[0])
    return avail


def find_patient_appointment(doctors: dict[str, list[str]], patient_hash: str) -> tuple[str, str, str] | None:
    """
    Returns (doctor, time, illness) if found.
    """
    for doctor, block_lines in doctors.items():
        for line in block_lines:
            parts = line.split()
            if len(parts) >= 3 and parts[1] == patient_hash:
                return (doctor, parts[0], parts[2])
    return None


def cancel_patient(doctors: dict[str, list[str]], patient_hash: str) -> tuple[bool, str | None, str | None]:
    """
    Removes patient+illness from their time block, leaving only the time.
    Returns (ok, doctor, time).
    """
    for doctor, block_lines in doctors.items():
        for i, line in enumerate(block_lines):
            parts = line.split()
            if len(parts) >= 3 and parts[1] == patient_hash:
                time = parts[0]
                block_lines[i] = time
                return (True, doctor, time)
    return (False, None, None)


def schedule_patient(
    doctors: dict[str, list[str]],
    doctor: str,
    time_block: str,
    patient_hash: str,
    illness: str,
) -> tuple[bool, list[str]]:
    """
    Returns (ok, other_available_times_if_failed).
    """
    block_lines = doctors.get(doctor)
    if block_lines is None:
        return (False, [])

    tb = TimeBlock.parse(time_block)
    if tb is None or not allowed_timeblock(tb):
        return (False, blocks_available(block_lines))

    for i, line in enumerate(block_lines):
        parts = line.split()
        if parts and parts[0] == time_block:
            if len(parts) == 1:
                block_lines[i] = f"{time_block} {patient_hash} {illness}"
                return (True, [])
            return (False, blocks_available(block_lines))

    return (False, blocks_available(block_lines))


def doctor_scheduled_times(block_lines: list[str]) -> list[str]:
    out: list[str] = []
    for line in block_lines:
        parts = line.split()
        if len(parts) >= 3:
            out.append(parts[0])
    return out


def main() -> None:
    appt_path = Path("appointments.txt")
    doctors = load_appointments(appt_path)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((HOST, APPT_UDP_PORT))

    print(f"Appointment Server is up and running using UDP on port\n{APPT_UDP_PORT}.")

    while True:
        data, addr = sock.recvfrom(65535)
        try:
            msg = json.loads(data.decode("utf-8"))
        except Exception:
            continue

        mtype = msg.get("type")

        if mtype == "doctor_list_req":
            print("The Appointment Server has received a doctor availability\nrequest.")
            resp = {"type": "doctor_list_resp", "doctors": sorted(list(doctors.keys()))}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)
            print("The Appointment Server has sent the lookup result to the\nHospital Server.")

        elif mtype == "doctor_avail_req":
            print("The Appointment Server has received a doctor availability\nrequest.")
            doctor = str(msg.get("doctor", ""))
            block_lines = doctors.get(doctor, [])
            avail = blocks_available(block_lines)
            if len(avail) == 8:
                print(f"All time blocks are available for {doctor}.")
            elif len(avail) > 0:
                print(f"{doctor} has some time slots available.")
            else:
                print(f"{doctor} has no time slots available.")
            resp = {"type": "doctor_avail_resp", "doctor": doctor, "avail": avail}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)
            print("The Appointment Server has sent the lookup result to the\nHospital Server.")

        elif mtype == "schedule_req":
            doctor = str(msg.get("doctor", ""))
            time_block = str(msg.get("time", ""))
            patient_hash = str(msg.get("patient_hash", ""))
            illness = str(msg.get("illness", ""))

            print(
                "Appointment scheduling request received (time:\n"
                f"{time_block}, doctor: {doctor}, patient hash suffix:\n"
                f"{hash_suffix(patient_hash)}, illness: {illness})."
            )

            ok, other = schedule_patient(doctors, doctor, time_block, patient_hash, illness)
            if ok:
                save_appointments(appt_path, doctors)
                print(f"Appointment has been scheduled successfully for user\n{hash_suffix(patient_hash)} with {doctor}.")
            else:
                print("The requested appointment time is not available.")

            resp = {
                "type": "schedule_resp",
                "ok": ok,
                "doctor": doctor,
                "time": time_block,
                "other_avail": other,
            }
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)

        elif mtype == "view_appt_req":
            patient_hash = str(msg.get("patient_hash", ""))
            print(
                "Appointment Server has received a view appointment\n"
                f"command for the user with hash suffix {hash_suffix(patient_hash)}."
            )
            found = find_patient_appointment(doctors, patient_hash)
            if found is None:
                print(f"The user with hash suffix {hash_suffix(patient_hash)} has no\nappointment in the system.")
                resp = {"type": "view_appt_resp", "ok": False}
            else:
                print(f"Returning details regarding the appointment for the user\nwith hash suffix {hash_suffix(patient_hash)}.")
                doctor, time, _ill = found
                resp = {"type": "view_appt_resp", "ok": True, "doctor": doctor, "time": time}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)

        elif mtype == "cancel_req":
            patient_hash = str(msg.get("patient_hash", ""))
            print(
                "Appointment Server has received a cancel appointment\n"
                f"command for the user with hash suffix: {hash_suffix(patient_hash)}."
            )
            ok, doctor, time = cancel_patient(doctors, patient_hash)
            if ok:
                save_appointments(appt_path, doctors)
                print("Successfully cancelled appointment.")
                resp = {"type": "cancel_resp", "ok": True, "doctor": doctor, "time": time}
            else:
                print("Error: Failed to find appointment.")
                resp = {"type": "cancel_resp", "ok": False}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)

        elif mtype == "view_appointments_req":
            doctor = str(msg.get("doctor", ""))
            print(f"Appointment Server has received a request to view\nappointments scheduled for {doctor}.")
            block_lines = doctors.get(doctor, [])
            times = doctor_scheduled_times(block_lines)
            if not times:
                print(f"No appointments have been made for\n{doctor}.")
            else:
                print(f"Returning the scheduled appointments for\n{doctor}.")
            resp = {"type": "view_appointments_resp", "doctor": doctor, "times": times}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)

        elif mtype == "fetch_illness_req":
            doctor = str(msg.get("doctor", ""))
            patient_hash = str(msg.get("patient_hash", ""))
            print(
                "Appointment Server has received a request from Hospital\n"
                f"Server regarding information about a user with hash suffix\n{hash_suffix(patient_hash)} from {doctor}."
            )
            found = find_patient_appointment(doctors, patient_hash)
            illness = found[2] if found else None
            print("Sending back the requested information to the Hospital\nserver.")
            if found:
                _ok, _doc, time = cancel_patient(doctors, patient_hash)
                save_appointments(appt_path, doctors)
                print(
                    f"Successfully removed {hash_suffix(patient_hash)} appointment slot,\n"
                    f"{time} is now free to be scheduled for tomorrow."
                )
            resp = {"type": "fetch_illness_resp", "ok": illness is not None, "illness": illness}
            sock.sendto(json.dumps(resp).encode("utf-8"), addr)


if __name__ == "__main__":
    main()

