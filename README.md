# EE450 Project (Socket Programming) - C++ Implementation

## Student Information
- Full Name: `<FILL_YOUR_NAME>`
- Student ID: `<FILL_YOUR_STUDENT_ID>`
- USC ID last 3 digits used for ports (`xxx`): `818`
- Target Ubuntu version: `20.04` (provided course VM)

## What Has Been Implemented
- Full five-process architecture:
  - `hospital_server` (TCP with clients + UDP with backend servers)
  - `authentication_server` (UDP)
  - `appointment_server` (UDP)
  - `prescription_server` (UDP)
  - `client` (TCP, role-based commands)
- Authentication with SHA-256-hashed username/password.
- Role determination (doctor vs patient) from `hospital.txt`.
- Patient command flow:
  - `lookup`
  - `lookup <doctor>`
  - `schedule <doctor> <start_time> <illness>`
  - `cancel`
  - `view_appointment`
  - `view_prescription`
  - `help`
  - `quit`
- Doctor command flow:
  - `view_appointments`
  - `prescribe <patient> <frequency>`
  - `view_prescription <patient>`
  - `help`
  - `quit`
- File-backed persistence:
  - `users.txt`
  - `hospital.txt`
  - `appointments.txt`
  - `prescriptions.txt`

## Source Files and Purpose
- `client.cpp`: client bootstrap, authentication request, role-based command REPL, exact client output strings.
- `hospital_server.cpp`: central dispatcher; TCP with clients, UDP RPC to backend servers, role/treatment lookup from `hospital.txt`.
- `authentication_server.cpp`: validates hash pairs from `users.txt`.
- `appointment_server.cpp`: doctor list/availability, schedule, cancel, view appointment(s), illness fetch for prescribe flow.
- `prescription_server.cpp`: prescription upsert and lookup by patient hash.
- `constants.h`: host and static port definitions.
- `net_utils.h/.cpp`: socket helpers (TCP connect/listen, UDP send/recv, line framing, local port lookup).
- `text_proto.h/.cpp`: internal delimited text message protocol (`TYPE|k=v|...`) parse/serialize helpers.
- `file_utils.h/.cpp`: file read/write helpers.
- `crypto_utils.h/.cpp`: hash wrapper helpers (`sha256_hash_trimmed`, hash suffix).
- `sha256.h`, `sha256.c`: SHA-256 implementation used by all components.
- `Makefile`: builds required executables and supports `make all`.
- `requirements_matrix.md`: requirement-to-handler mapping used during implementation.

## Inter-Process Message Format
Internal protocol uses one-line delimited records:
- Format: `TYPE|k1=v1|k2=v2|...`
- Client/Hospital TCP messages are newline-terminated (`\n`) for framing.
- Hospital/backend UDP messages are single datagrams with the same text format.

Key message types include:
- Auth: `auth`, `auth_req`, `auth_resp`, `auth_result`
- Appointment: `doctor_list_req`, `doctor_avail_req`, `schedule_req`, `view_appt_req`, `cancel_req`, `view_appointments_req`, `fetch_illness_req`
- Prescription: `prescribe_req`, `view_prescription_req`

## Port Allocation
Using `xxx = 818`:
- Authentication UDP: `21818`
- Prescription UDP: `22818`
- Appointment UDP: `23818`
- Hospital UDP: `25818`
- Hospital TCP: `26818`
- Client TCP local port: dynamically assigned by OS and printed via `getsockname()`.

## Build and Run
1. Build:
   - `make all`
2. Start servers in required order:
   - `./hospital_server`
   - `./authentication_server`
   - `./appointment_server`
   - `./prescription_server`
3. Start clients:
   - `./client <USERNAME> <PASSWORD>`

## Runtime Notes / Idiosyncrasies
- All host bindings use `127.0.0.1`.
- Programs are designed to keep running until terminated (Ctrl-C), while `client` exits on `quit`.
- Scheduling enforces `HH:MM` with minute `00` and hour range `09:00` to `16:00` inclusive.
- Prescription `frequency=None` is treated as no current prescription for patient-facing lookup behavior.
- If expected data files are missing or malformed, behavior may degrade (e.g., missing users/doctors/treatments).

## Reused Code Disclosure
- `sha256.c` contains an adapted SHA-256 implementation (public-domain style reference implementation), used to provide `sha256_easy_hash_hex`.
- No additional external packages are required.

## Known Constraints
- This project is implemented for UNIX-style sockets and intended for grading on Ubuntu 20.04 as required.
- Please ensure no extra debug prints are added before submission.