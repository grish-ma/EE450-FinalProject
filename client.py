import hashlib
import json
import socket
import sys


HOST = "127.0.0.1"
USC_ID_SUFFIX = 818
HOSP_TCP_PORT = 26000 + USC_ID_SUFFIX  # 26818


def sha256_hash(text: str) -> str:
    text = text.strip()
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def send_req(req: dict) -> tuple[dict | None, int]:
    """
    Single-request TCP pattern:
      connect -> send JSON line -> recv JSON line -> close
    Returns (resp, local_client_port)
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, HOSP_TCP_PORT))
    local_port = sock.getsockname()[1]
    sock.sendall(json.dumps(req).encode("utf-8") + b"\n")

    buf = b""
    while b"\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            sock.close()
            return None, local_port
        buf += chunk
        if len(buf) > 1024 * 1024:
            sock.close()
            return None, local_port
    line, _rest = buf.split(b"\n", 1)
    sock.close()
    try:
        return json.loads(line.decode("utf-8")), local_port
    except Exception:
        return None, local_port


def patient_help() -> None:
    print(
        "Please enter the command:\n"
        "<lookup>,\n"
        "<lookup <doctor>>,\n"
        "<schedule <doctor> <start_time> <illness>>,\n"
        "<cancel>,\n"
        "<view_appointment>,\n"
        "<view_prescription>,\n"
        "<quit>"
    )


def doctor_help() -> None:
    print(
        "Please enter the command:\n"
        "<view_appointments>,\n"
        "<prescribe <patient> <frequency>>,\n"
        "<view_prescription <patient>>,\n"
        "<quit>"
    )


def main() -> None:
    if len(sys.argv) != 3:
        print("Usage: ./client <username> <password>")
        return

    username = sys.argv[1]
    password = sys.argv[2]

    print("The client is up and running.")

    u_hash = sha256_hash(username)
    p_hash = sha256_hash(password)

    print(f"{username} sent an authentication request to the\nhospital server.")
    resp, _port = send_req({"type": "auth", "u_hash": u_hash, "p_hash": p_hash})
    if not resp or not resp.get("ok"):
        print("The credentials are incorrect. Please try again.")
        return

    role = resp.get("role")
    doctor_name = resp.get("doctor_name")  # for doctor users, the canonical doctor name

    if role == "doctor":
        print(
            f"{username} received the authentication result.\n"
            "Authentication successful. You have been granted doctor\n"
            "access."
        )
        doctor_help()
    else:
        print(
            f"{username} received the authentication result.\n"
            "Authentication successful. You have been granted patient\n"
            "access."
        )
        patient_help()

    while True:
        try:
            cmdline = input().strip()
        except EOFError:
            return
        if not cmdline:
            continue

        parts = cmdline.split()
        cmd = parts[0]

        if cmd == "help":
            if role == "doctor":
                doctor_help()
            else:
                patient_help()
            continue

        if cmd == "quit":
            print("You have successfully been logged out.")
            return

        if role != "doctor":
            if cmd == "lookup" and len(parts) == 1:
                print(f"{username} sent a lookup request to the hospital\nserver.")
                resp, port = send_req({"type": "lookup_doctors", "u_hash": u_hash})
                doctors = (resp or {}).get("doctors", [])
                print(
                    "The client received the response from the hospital server\n"
                    f"using TCP over port {port}.\n"
                    "The following doctors are available:"
                )
                for d in doctors:
                    print(d)

            elif cmd == "lookup" and len(parts) == 2:
                doctor = parts[1]
                print(f"Patient {username} sent a lookup request to the\nhospital server for {doctor}.")
                resp, port = send_req({"type": "lookup_availability", "u_hash": u_hash, "doctor": doctor})
                avail = (resp or {}).get("avail", [])
                if len(avail) == 8:
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}.\n"
                        f"All time blocks are available for {doctor}."
                    )
                elif len(avail) == 0:
                    print(
                        "The client received the response from the Hospital Server\n"
                        f"using TCP over port {port}.\n"
                        f"{doctor} has no time slots available."
                    )
                else:
                    print(
                        "The client received the response from the Hospital Server\n"
                        f"using TCP over port {port}.\n"
                        f"{doctor} is available at times:"
                    )
                    for t in avail:
                        print(t)

            elif cmd == "schedule" and len(parts) == 4:
                doctor, start_time, illness = parts[1], parts[2], parts[3]
                print(f"{username} sent an appointment schedule\nrequest to the hospital server.")
                resp, port = send_req({"type": "schedule", "u_hash": u_hash, "doctor": doctor, "time": start_time, "illness": illness})
                ok = bool((resp or {}).get("ok"))
                if ok:
                    print(
                        "The client received the response from the Hospital Server\n"
                        f"using TCP over port {port}\n"
                        "An appointment has been successfully scheduled for\n"
                        f"patient {username} with {doctor} at\n"
                        f"{start_time}."
                    )
                else:
                    other = (resp or {}).get("other_avail", [])
                    if not other:
                        print(
                            "The client received the response from the hospital server\n"
                            f"using TCP over port {port}\n"
                            "Unable to schedule an appointment with\n"
                            f"{doctor} at this time, as all time blocks have\n"
                            "been taken up."
                        )
                    else:
                        print(
                            "The client received the response from the hospital server\n"
                            f"using TCP over port {port}\n"
                            "Unable to schedule an appointment with\n"
                            f"{doctor} at {start_time}. Other available\n"
                            "time blocks are"
                        )
                        for t in other:
                            print(t)

            elif cmd == "cancel" and len(parts) == 1:
                print(f"{username} sent a cancellation request to the\nHospital Server.")
                resp, port = send_req({"type": "cancel", "u_hash": u_hash})
                ok = bool((resp or {}).get("ok"))
                if not ok:
                    print(
                        "The client received the response from the Hospital Server\n"
                        f"using TCP over port {port}\n"
                        "You have no appointments available to cancel."
                    )
                else:
                    doctor = (resp or {}).get("doctor")
                    time = (resp or {}).get("time")
                    print(
                        "The client received the response from the Hospital Server\n"
                        f"using TCP over port {port}\n"
                        "You have successfully cancelled your appointment with\n"
                        f"{doctor} at {time}."
                    )

            elif cmd == "view_appointment" and len(parts) == 1:
                print(f"{username} sent a request to view their\nappointment to the Hospital Server.")
                resp, port = send_req({"type": "view_appointment", "u_hash": u_hash})
                ok = bool((resp or {}).get("ok"))
                if not ok:
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over client port {port}\n"
                        "You do not have an appointment today."
                    )
                else:
                    doctor = (resp or {}).get("doctor")
                    time = (resp or {}).get("time")
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}\n"
                        f"You have an appointment scheduled with {doctor}\n"
                        f"at {time}."
                    )

            elif cmd == "view_prescription" and len(parts) == 1:
                print(f"{username} sent a request to view their\nprescription to the Hospital Server.")
                resp, port = send_req({"type": "view_prescription", "role": "patient", "patient_hash": u_hash})
                ok = bool((resp or {}).get("ok"))
                if not ok:
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}\n"
                        "You do not have a prescription to look up."
                    )
                else:
                    doctor = (resp or {}).get("doctor")
                    treatment = (resp or {}).get("treatment")
                    freq = (resp or {}).get("frequency")
                    if freq == "None":
                        print(
                            "The client received the response from the hospital server\n"
                            f"using TCP over port {port}\n"
                            "You were not prescribed any treatment by\n"
                            f"{doctor} following your diagnosis."
                        )
                    else:
                        print(
                            "The client received the response from the hospital server\n"
                            f"using TCP over port {port}\n"
                            f"You have been prescribed {treatment}, to be taken\n"
                            f"{freq}, by {doctor}."
                        )
            else:
                # Ignore invalid commands (PDF only specifies required output; avoid extra prints).
                continue

        else:
            # Doctor commands
            if cmd == "view_appointments" and len(parts) == 1:
                canonical_doctor = doctor_name or username
                print(f"{canonical_doctor} sent a request to view their\nscheduled appointments to the Hospital Server.")
                resp, port = send_req({"type": "view_appointments", "doctor": canonical_doctor})
                times = (resp or {}).get("times", [])
                if not times:
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}\n"
                        "You do not have any appointments scheduled."
                    )
                else:
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}\n"
                        f"{canonical_doctor} is scheduled at times:"
                    )
                    for t in times:
                        print(t)

            elif cmd == "prescribe" and len(parts) == 3:
                patient_username = parts[1]
                frequency = parts[2]
                canonical_doctor = doctor_name or username
                print(
                    f"{canonical_doctor} sent a request to the Hospital Server\n"
                    f"to prescribe {patient_username} following their\n"
                    "diagnosis."
                )
                patient_hash = sha256_hash(patient_username)
                resp, port = send_req(
                    {"type": "prescribe", "doctor": canonical_doctor, "patient_hash": patient_hash, "frequency": frequency}
                )
                ok = bool((resp or {}).get("ok"))
                treatment = (resp or {}).get("treatment", "None")
                if ok:
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}\n"
                        f"You have successfully prescribed {patient_username} with\n"
                        f"{treatment}, to be taken {frequency}."
                    )

            elif cmd == "view_prescription" and len(parts) == 2:
                patient_username = parts[1]
                canonical_doctor = doctor_name or username
                print(
                    f"{canonical_doctor} sent a request to view\n"
                    f"{patient_username} prescription to the Hospital Server."
                )
                patient_hash = sha256_hash(patient_username)
                resp, port = send_req(
                    {"type": "view_prescription", "role": "doctor", "doctor": canonical_doctor, "patient_hash": patient_hash}
                )
                ok = bool((resp or {}).get("ok"))
                if not ok:
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}\n"
                        f"{patient_username} does not have a prescription."
                    )
                else:
                    doctor = (resp or {}).get("doctor")
                    treatment = (resp or {}).get("treatment")
                    freq = (resp or {}).get("frequency")
                    print(
                        "The client received the response from the hospital server\n"
                        f"using TCP over port {port}\n"
                        f"{patient_username} has been prescribed {treatment}, to\n"
                        f"be taken {freq}, by\n"
                        f"{doctor}."
                    )


if __name__ == "__main__":
    main()

