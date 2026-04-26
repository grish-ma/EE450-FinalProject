EE450 C++ Implementation Matrix
================================

This matrix maps the project requirements from `EE450-Project-Spring26.pdf` to concrete handlers/files in this implementation.

Core Topology
-------------

- Host fixed to `127.0.0.1` in all processes.
- Static ports with `xxx=818`:
  - Authentication UDP: `21818`
  - Prescription UDP: `22818`
  - Appointment UDP: `23818`
  - Hospital UDP: `25818`
  - Hospital TCP: `26818`
- Client uses dynamic local TCP port and prints it via `getsockname()`.
- Processes: `hospital_server`, `authentication_server`, `appointment_server`, `prescription_server`, `client`.

File Formats
------------

- `users.txt`:
  - Parsed in `authentication_server.cpp`
  - Format: `<hashed_username> <hashed_password>`
- `hospital.txt`:
  - Parsed in `hospital_server.cpp`
  - `[Doctors]`: `<doctor_name> <hashed_doctor_name>`
  - `[Treatments]`: `<illness> <treatment>`
- `appointments.txt`:
  - Parsed/updated in `appointment_server.cpp`
  - Doctor header followed by time slots:
    - `HH:MM`
    - or `HH:MM <hashed_patient> <illness>`
- `prescriptions.txt`:
  - Parsed/updated in `prescription_server.cpp`
  - `<doctor_name> <hashed_patient> <treatment> <frequency>`

Hashing & Privacy
-----------------

- SHA-256 implementation in `sha256.c`/`sha256.h` (API `sha256_easy_hash_hex`).
- C++ wrapper in `crypto_utils.h`:
  - trim input
  - hash to 64-char hex
  - hash suffix helper (last 5 chars)
- Client hashes username/password before auth request.
- For doctor prescription commands, client hashes plaintext patient username before sending.
- Servers print only hash suffix where required.

Protocol Mapping (`TYPE|k=v|...`)
---------------------------------

- Client -> Hospital TCP:
  - `auth`
  - `lookup_doctors`
  - `lookup_availability`
  - `schedule`
  - `cancel`
  - `view_appointment`
  - `view_appointments`
  - `prescribe`
  - `view_prescription`
- Hospital -> Authentication UDP:
  - `auth_req` / `auth_resp`
- Hospital -> Appointment UDP:
  - `doctor_list_req` / `doctor_list_resp`
  - `doctor_avail_req` / `doctor_avail_resp`
  - `schedule_req` / `schedule_resp`
  - `view_appt_req` / `view_appt_resp`
  - `cancel_req` / `cancel_resp`
  - `view_appointments_req` / `view_appointments_resp`
  - `fetch_illness_req` / `fetch_illness_resp`
- Hospital -> Prescription UDP:
  - `prescribe_req` / `prescribe_resp`
  - `view_prescription_req` / `view_prescription_resp`

Command-to-Handler Matrix
-------------------------

- Patient commands in `client.cpp`:
  - `help`: patient command list (exact text)
  - `lookup`: TCP `lookup_doctors` -> Hospital -> Appointment list
  - `lookup <doctor>`: TCP `lookup_availability` -> Hospital -> Appointment availability
  - `schedule <doctor> <start_time> <illness>`: TCP `schedule` -> Hospital -> Appointment schedule
  - `cancel`: TCP `cancel` -> Hospital -> Appointment cancel
  - `view_appointment`: TCP `view_appointment` -> Hospital -> Appointment lookup by patient hash
  - `view_prescription`: TCP `view_prescription` (patient mode) -> Hospital -> Prescription lookup
  - `quit`: exact logout message
- Doctor commands in `client.cpp`:
  - `help`: doctor command list (exact text)
  - `view_appointments`: TCP `view_appointments` -> Hospital -> Appointment doctor schedule
  - `prescribe <patient> <frequency>`: hash patient -> TCP `prescribe` -> Hospital fetch illness from Appointment then save treatment/frequency in Prescription
  - `view_prescription <patient>`: hash patient -> TCP `view_prescription` (doctor mode) -> Hospital -> Prescription lookup
  - `quit`: exact logout message

Server Responsibility Matrix
----------------------------

- `authentication_server.cpp`:
  - Boot UDP socket
  - Validate `auth_req` against `users.txt`
  - Print authentication server table messages
- `appointment_server.cpp`:
  - Doctor list and availability
  - Time range validation (`09:00` to `16:00` inclusive, minute `00`)
  - Schedule/cancel/view appointment
  - Doctor view appointments
  - Fetch illness for prescribe flow and free slot
  - Persist `appointments.txt`
- `prescription_server.cpp`:
  - Upsert prescription rows
  - View prescription by patient hash
  - Special handling for `frequency=None`
- `hospital_server.cpp`:
  - TCP endpoint for client requests
  - UDP RPC dispatch to backend servers
  - Doctor/patient role determination from `hospital.txt`
  - Treatment lookup from illness
  - Print hospital server table messages

On-Screen Message Compliance
----------------------------

- Implemented as explicit print blocks in each command branch in:
  - `client.cpp`
  - `hospital_server.cpp`
  - `authentication_server.cpp`
  - `appointment_server.cpp`
  - `prescription_server.cpp`
- No extra debug prints.
- Required dynamic values are injected:
  - TCP/UDP port numbers
  - doctor/patient usernames
  - hash suffix
  - time block, illness, treatment, frequency

Submission Requirements Mapping
-------------------------------

- Build system:
  - `Makefile` with `make all` and required executable outputs.
- Required executable names:
  - `client`
  - `hospital_server`
  - `authentication_server`
  - `appointment_server`
  - `prescription_server`
- Documentation:
  - `README.md` includes files, message format, reused code note, Ubuntu target.
