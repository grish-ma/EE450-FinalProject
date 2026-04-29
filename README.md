# EE450 Project (Socket Programming) - C++ Implementation

- Full Name: Grishma Shukla
- Student ID: '8028101818'
- USC ID last 3 digits used for ports ('xxx'): '818'
- Target Ubuntu version: '20.04'


## What I implemented
- 5 processes:
  - 'hospital_server' (TCP with client, UDP with backend servers)
  - 'authentication_server'
  - 'appointment_server'
  - 'prescription_server'
  - 'client'
- Authentication with SHA-256 (username and password are hashed in client).
- Role check (doctor/patient) based on 'hospital.txt'.
- Patient commands: 'lookup', 'lookup <doctor>', 'schedule', 'cancel', 'view_appointment', 'view_prescription', 'help', 'quit'.
- Doctor commands: 'view_appointments', 'prescribe', 'view_prescription <patient>', 'help', 'quit'.

## Files and what they do
- 'client.cpp': client login and command loop.
- 'hospital_server.cpp': main dispatcher between client and backend servers.
- 'authentication_server.cpp': checks hashed user credentials from 'users.txt'.
- 'appointment_server.cpp': handles doctor lookup, schedule, cancel, and appointments data.
- 'prescription_server.cpp': handles prescription save/view.
- 'constants.h': host and port constants.
- 'net_utils.h', 'net_utils.cpp': socket helper functions.
- 'text_proto.h', 'text_proto.cpp': parse/build internal text messages.
- 'file_utils.h', 'file_utils.cpp': read/write text files.
- 'crypto_utils.h', 'crypto_utils.cpp': hashing helpers and hash suffix helper.
- 'sha256.h', 'sha256.c': SHA-256 implementation used by this projet
- 'Makefile': builds all executables.

## Ports use USC suffix '818':
  - Auth UDP '21818'
  - Prescription UDP '22818'
  - Appointment UDP '23818'
  - Hospital UDP '25818'
  - Hospital TCP '26818'
- Client local TCP port is dynamic and printed using 'getsockname()'.
- 'appointment_server' loads 'appointments.txt' on startup.
- Time input must match timeslot formatting in 'appointments.txt' (for example '9:00').

## Reused code
- 'sha256.c' / 'sha256.h' are reused/adapted SHA-256 implementation code.
- Everything else is written for this project.

## Build and run
1. 'make all'
2. Start servers:
   - './hospital_server'
   - './authentication_server'
   - './appointment_server'
   - './prescription_server'
3. Start clients:
   - './client <USERNAME> <PASSWORD>'