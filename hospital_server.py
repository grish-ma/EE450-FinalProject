import hashlib
import json
import socket
from pathlib import Path


HOST = "127.0.0.1"
USC_ID_SUFFIX = 818

AUTH_UDP_PORT = 21000 + USC_ID_SUFFIX  # 21818
PRESC_UDP_PORT = 22000 + USC_ID_SUFFIX  # 22818
APPT_UDP_PORT = 23000 + USC_ID_SUFFIX  # 23818

HOSP_UDP_PORT = 25000 + USC_ID_SUFFIX  # 25818
HOSP_TCP_PORT = 26000 + USC_ID_SUFFIX  # 26818


def sha256_hash(text: str) -> str:
    text = text.strip()
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def hash_suffix(sha256_hex: str) -> str:
    return sha256_hex[-5:]


def recv_json_line(conn: socket.socket) -> dict | None:
    buf = b""
    while b"\n" not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            return None
        buf += chunk
        if len(buf) > 1024 * 1024:
            return None
    line, _rest = buf.split(b"\n", 1)
    try:
        return json.loads(line.decode("utf-8"))
    except Exception:
        return None


def send_json_line(conn: socket.socket, obj: dict) -> None:
    conn.sendall(json.dumps(obj).encode("utf-8") + b"\n")


def udp_rpc(udp_sock: socket.socket, dest_port: int, req: dict) -> dict | None:
    udp_sock.sendto(json.dumps(req).encode("utf-8"), (HOST, dest_port))
    udp_sock.settimeout(3.0)
    try:
        data, _addr = udp_sock.recvfrom(65535)
    except TimeoutError:
        return None
    finally:
        udp_sock.settimeout(None)
    try:
        return json.loads(data.decode("utf-8"))
    except Exception:
        return None


def load_doctors_and_treatments(path: Path) -> tuple[dict[str, str], dict[str, str]]:
    """
    hospital.txt:
      [Doctors]
      <doctor_name> <hashed_doctor_name>
      ...
      [Treatments]
      <illness> <treatment>
      ...
    Returns:
      doctor_hash -> doctor_name
      illness -> treatment
    """
    doctor_hash_to_name: dict[str, str] = {}
    illness_to_treatment: dict[str, str] = {}
    if not path.exists():
        return doctor_hash_to_name, illness_to_treatment

    section = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        s = raw.strip()
        if not s:
            continue
        if s == "[Doctors]":
            section = "doctors"
            continue
        if s == "[Treatments]":
            section = "treatments"
            continue
        parts = s.split()
        if section == "doctors" and len(parts) >= 2:
            name, h = parts[0], parts[1]
            doctor_hash_to_name[h] = name
        elif section == "treatments" and len(parts) >= 2:
            illness, treatment = parts[0], parts[1]
            illness_to_treatment[illness] = treatment

    return doctor_hash_to_name, illness_to_treatment


def main() -> None:
    hospital_txt = Path("hospital.txt")

    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind((HOST, HOSP_UDP_PORT))

    tcp_listen = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_listen.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    tcp_listen.bind((HOST, HOSP_TCP_PORT))
    tcp_listen.listen(10)

    print(f"Hospital Server is up and running using UDP on port\n{HOSP_UDP_PORT}.")

    while True:
        conn, _addr = tcp_listen.accept()
        try:
            msg = recv_json_line(conn)
            if msg is None:
                continue

            mtype = msg.get("type")

            if mtype == "auth":
                u_hash = str(msg.get("u_hash", ""))
                p_hash = str(msg.get("p_hash", ""))
                suffix = hash_suffix(u_hash)

                print(f"Hospital Server received an authentication request from a\nuser with hash suffix {suffix}.")
                print("Hospital Server has sent an authentication request to the\nAuthentication Server.")

                resp = udp_rpc(udp_sock, AUTH_UDP_PORT, {"type": "auth_req", "u_hash": u_hash, "p_hash": p_hash})
                print(
                    "Hospital server has received the response from the\n"
                    f"authentication server using UDP over port {HOSP_UDP_PORT}."
                )

                ok = bool(resp and resp.get("ok"))
                if not ok:
                    send_json_line(conn, {"type": "auth_result", "ok": False})
                    continue

                print(f"User with a hash suffix {suffix} has been granted\naccess to the system. Determining the access of the user.")

                doctor_hash_to_name, _treat = load_doctors_and_treatments(hospital_txt)
                is_doctor = u_hash in doctor_hash_to_name
                if is_doctor:
                    print(f"User with hash suffix {suffix} will be granted doctor\naccess.")
                    role = "doctor"
                    doctor_name = doctor_hash_to_name[u_hash]
                else:
                    print(f"User with hash {suffix} will be granted patient\naccess.")
                    role = "patient"
                    doctor_name = None

                send_json_line(conn, {"type": "auth_result", "ok": True, "role": role, "doctor_name": doctor_name})
                print(
                    "Hospital Server has sent the response from\n"
                    f"Authentication Server to the client using TCP over port\n{HOSP_TCP_PORT}."
                )

            elif mtype == "lookup_doctors":
                u_hash = str(msg.get("u_hash", ""))
                suffix = hash_suffix(u_hash)
                print(
                    "Hospital Server received a lookup request from a user\n"
                    f"with a hash suffix {suffix} over port {HOSP_TCP_PORT}."
                )
                print("Hospital Server sent the doctor lookup request to the\nAppointment server.")
                resp = udp_rpc(udp_sock, APPT_UDP_PORT, {"type": "doctor_list_req"})
                print(
                    "Hospital Server has received the response from\n"
                    f"Appointment Server using UDP over port {HOSP_UDP_PORT}."
                )
                doctors = resp.get("doctors", []) if resp else []
                send_json_line(conn, {"type": "lookup_doctors_resp", "doctors": doctors})
                print("Hospital Server has sent the doctor lookup to the client.")

            elif mtype == "lookup_availability":
                u_hash = str(msg.get("u_hash", ""))
                doctor = str(msg.get("doctor", ""))
                suffix = hash_suffix(u_hash)
                print(
                    "Hospital Server has received a lookup request from a\n"
                    f"user with hash suffix {suffix} to lookup\n"
                    f"{doctor} availability using TCP over port\n{HOSP_TCP_PORT}."
                )
                print("Hospital Server sent the doctor lookup request to the\nAppointment server.")
                resp = udp_rpc(udp_sock, APPT_UDP_PORT, {"type": "doctor_avail_req", "doctor": doctor})
                print(
                    "Hospital Server has received the response from\n"
                    f"Appointment Server using UDP over port {HOSP_UDP_PORT}."
                )
                send_json_line(conn, {"type": "lookup_availability_resp", "doctor": doctor, "avail": (resp or {}).get("avail", [])})
                print("The Hospital Server has sent the response to the client.")

            elif mtype == "schedule":
                u_hash = str(msg.get("u_hash", ""))
                doctor = str(msg.get("doctor", ""))
                time_block = str(msg.get("time", ""))
                illness = str(msg.get("illness", ""))
                suffix = hash_suffix(u_hash)
                print(
                    "Hospital Server has received a schedule request from a\n"
                    f"user with hash suffix: {suffix} to book an\n"
                    f"appointment using TCP over port {HOSP_TCP_PORT}."
                )
                print("Hospital Server has sent the schedule request to the\nappointment server.")
                resp = udp_rpc(
                    udp_sock,
                    APPT_UDP_PORT,
                    {"type": "schedule_req", "doctor": doctor, "time": time_block, "patient_hash": u_hash, "illness": illness},
                )
                print(
                    "Hospital Server has received the response from\n"
                    f"Appointment Server using UDP over {HOSP_UDP_PORT}."
                )
                send_json_line(conn, {"type": "schedule_resp", **(resp or {"ok": False, "doctor": doctor, "time": time_block, "other_avail": []})})
                print("The hospital server has sent the response to the client.")

            elif mtype == "cancel":
                u_hash = str(msg.get("u_hash", ""))
                suffix = hash_suffix(u_hash)
                print(
                    "Hospital Server has received a cancel request from user\n"
                    f"with hash suffix: {suffix} to cancel their appointment\n"
                    f"using TCP over port {HOSP_TCP_PORT}."
                )
                print("The hospital server has sent the cancel request to the\nappointment server.")
                resp = udp_rpc(udp_sock, APPT_UDP_PORT, {"type": "cancel_req", "patient_hash": u_hash})
                print(
                    "Hospital Server has received the response from\n"
                    f"Appointment Server using UDP over port {HOSP_UDP_PORT}."
                )
                send_json_line(conn, {"type": "cancel_resp", **(resp or {"ok": False})})
                print("The hospital server has sent the response to the client.")

            elif mtype == "view_appointment":
                u_hash = str(msg.get("u_hash", ""))
                suffix = hash_suffix(u_hash)
                print(
                    "Hospital server has received a view appointment request\n"
                    f"from a user with hash suffix {suffix} to view their\n"
                    f"appointment details using TCP over port {HOSP_TCP_PORT}."
                )
                print("Hospital Server has sent the view appointments request\nto the Appointment Server.")
                resp = udp_rpc(udp_sock, APPT_UDP_PORT, {"type": "view_appt_req", "patient_hash": u_hash})
                print(
                    "Hospital Server has received the response from the\n"
                    f"appointment server using UDP over port {HOSP_UDP_PORT}."
                )
                send_json_line(conn, {"type": "view_appointment_resp", **(resp or {"ok": False})})
                print("The hospital server has sent the response to the client.")

            elif mtype == "view_appointments":
                doctor = str(msg.get("doctor", ""))
                print(
                    "Hospital Server has received a view appointments\n"
                    f"request from {doctor} to view their schedule\n"
                    f"details using TCP over port {HOSP_TCP_PORT}."
                )
                print("The hospital server has sent the view appointments\nrequest to the Appointment Server.")
                resp = udp_rpc(udp_sock, APPT_UDP_PORT, {"type": "view_appointments_req", "doctor": doctor})
                print(
                    "Hospital server has received the response from the\n"
                    f"Appointment server using UDP over port {HOSP_UDP_PORT}."
                )
                send_json_line(conn, {"type": "view_appointments_resp", **(resp or {"doctor": doctor, "times": []})})
                print("The hospital server has sent the response to the client.")

            elif mtype == "prescribe":
                doctor = str(msg.get("doctor", ""))
                patient_hash = str(msg.get("patient_hash", ""))
                frequency = str(msg.get("frequency", ""))
                print(
                    "Hospital Server has received a prescription request from\n"
                    f"{doctor} for a user with hash suffix\n"
                    f"{hash_suffix(patient_hash)} using TCP over port {HOSP_TCP_PORT}."
                )
                print(
                    "Hospital Server has sent a request to fetch patients with\n"
                    f"hash suffix {hash_suffix(patient_hash)} illness information to the\nAppointment Server."
                )
                illness_resp = udp_rpc(udp_sock, APPT_UDP_PORT, {"type": "fetch_illness_req", "doctor": doctor, "patient_hash": patient_hash})
                print(
                    "Hospital Server has received the illness response from\n"
                    f"the Appointment server using UDP over port {HOSP_UDP_PORT}."
                )
                illness = (illness_resp or {}).get("illness")
                doctor_hash_to_name, illness_to_treatment = load_doctors_and_treatments(hospital_txt)
                if not illness:
                    send_json_line(conn, {"type": "prescribe_resp", "ok": False})
                    continue
                print(f"Acquiring treatment for {illness} from the database.")
                treatment = illness_to_treatment.get(str(illness), "None")
                print(f"Hospital server has sent the prescription request to the\nprescription server to prescribe {treatment}.")
                presc_resp = udp_rpc(
                    udp_sock,
                    PRESC_UDP_PORT,
                    {"type": "prescribe_req", "doctor": doctor, "patient_hash": patient_hash, "treatment": treatment, "frequency": frequency},
                )
                print(
                    "Hospital server has received the response from the\n"
                    f"prescription server using UDP over port {HOSP_UDP_PORT}"
                )
                send_json_line(conn, {"type": "prescribe_resp", "ok": bool(presc_resp and presc_resp.get("ok")), "treatment": treatment})
                print("The hospital server has sent the response to the client.")

            elif mtype == "view_prescription":
                # Caller can be patient or doctor; we just forward to prescription server.
                requester_role = str(msg.get("role", "patient"))
                doctor = msg.get("doctor")
                patient_hash = str(msg.get("patient_hash", ""))
                if requester_role == "patient":
                    print(
                        "Hospital Server has received a prescription request from\n"
                        f"a patient with hash suffix {hash_suffix(patient_hash)} to view their\n"
                        f"prescription details using TCP over port {HOSP_TCP_PORT}."
                    )
                else:
                    print(
                        "Hospital Server has received a prescription request from\n"
                        f"{doctor} to view a patient with hash suffix\n"
                        f"{hash_suffix(patient_hash)} prescription details using TCP over port\n{HOSP_TCP_PORT}."
                    )
                print("Hospital Server has sent the prescription request to the\nPrescription Server.")
                resp = udp_rpc(udp_sock, PRESC_UDP_PORT, {"type": "view_prescription_req", "patient_hash": patient_hash})
                print(
                    "Hospital server has received the response from the\n"
                    f"prescription server using UDP over port {HOSP_UDP_PORT}."
                )
                send_json_line(conn, {"type": "view_prescription_resp", **(resp or {"ok": False})})
                print("Hospital server has sent the response to the client.")

        finally:
            try:
                conn.close()
            except Exception:
                pass


if __name__ == "__main__":
    main()

