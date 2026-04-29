#include "constants.h"
#include "crypto_utils.h"
#include "net_utils.h"
#include "text_proto.h"

#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

static std::string getf(const ProtoMessage &m, const std::string &key, const std::string &fallback = "") {
    std::map<std::string, std::string>::const_iterator it = m.fields.find(key);
    if (it == m.fields.end()) {
        return fallback;
    }
    return it->second;
}

static bool is_true(const std::string &v) {
    return v == "1" || v == "true" || v == "True";
}

// Help text for patient users.
static void patient_help() {
    std::cout << "Please enter the command:\n"
              << "<lookup>,\n"
              << "<lookup <doctor>>,\n"
              << "<schedule <doctor> <start_time> <illness>>,\n"
              << "<cancel>,\n"
              << "<view_appointment>,\n"
              << "<view_prescription>,\n"
              << "<quit>" << std::endl;
}

// Help text for doctor users.
static void doctor_help() {
    std::cout << "Please enter the command:\n"
              << "<view_appointments>,\n"
              << "<prescribe <patient> <frequency>>,\n"
              << "<view_prescription <patient>>,\n"
              << "<quit>" << std::endl;
}

// Open TCP connection to hospital, send one request, receive one response.
static bool send_req(const ProtoMessage &req, ProtoMessage &resp, int &client_port) {
    int fd = connect_tcp(HOST, HOSP_TCP_PORT);
    if (fd < 0) {
        return false;
    }
    client_port = get_local_port(fd);
    bool ok = send_all(fd, proto_serialize(req) + "\n");
    if (!ok) {
        close(fd);
        return false;
    }
    std::string line;
    if (!recv_line_tcp(fd, line)) {
        close(fd);
        return false;
    }
    close(fd);
    return proto_parse(line, resp);
}

int main(int argc, char **argv) {
    // Client login must be: ./client <username> <password>
    if (argc != 3) {
        std::cout << "Usage: ./client <username> <password>" << std::endl;
        return 0;
    }

    // Read plaintext login and hash before sending.
    std::string username = argv[1];
    std::string password = argv[2];
    std::string u_hash = sha256_hash_trimmed(username);
    std::string p_hash = sha256_hash_trimmed(password);

    std::cout << "The client is up and running." << std::endl;
    std::cout << username << " sent an authentication request to the\nhospital server." << std::endl;

    // Send login request to Hospital Server.
    ProtoMessage req;
    req.type = "auth";
    req.fields["u_hash"] = u_hash;
    req.fields["p_hash"] = p_hash;

    ProtoMessage resp;
    int auth_port = 0;
    // Stop if authentication failed.
    if (!send_req(req, resp, auth_port) || !is_true(getf(resp, "ok", "0"))) {
        std::cout << "The credentials are incorrect. Please try again." << std::endl;
        return 0;
    }

    // Show role-specific success message and help commands.
    std::string role = getf(resp, "role");
    std::string doctor_name = getf(resp, "doctor_name");
    if (role == "doctor") {
        std::cout << username << " received the authentication result.\n"
                  << "Authentication successful. You have been granted doctor\n"
                  << "access." << std::endl;
        doctor_help();
    } else {
        std::cout << username << " received the authentication result.\n"
                  << "Authentication successful. You have been granted patient\n"
                  << "access." << std::endl;
        patient_help();
    }

    std::string line;
    // Command loop after successful login.
    while (std::getline(std::cin, line)) {
        line = trim_copy(line);
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> parts = split_char(line, ' ');
        std::vector<std::string> p;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (!parts[i].empty()) {
                p.push_back(parts[i]);
            }
        }
        if (p.empty()) {
            continue;
        }
        std::string cmd = p[0];

        if (cmd == "help") {
            if (role == "doctor") {
                doctor_help();
            } else {
                patient_help();
            }
            continue;
        }
        if (cmd == "quit") {
            std::cout << "You have successfully been logged out." << std::endl;
            return 0;
        }

        if (role != "doctor") {
            // ---------------- Patient commands ----------------
            if (cmd == "lookup" && p.size() == 1) {
                std::cout << username << " sent a lookup request to the hospital\nserver." << std::endl;
                ProtoMessage q;
                q.type = "lookup_doctors";
                q.fields["u_hash"] = u_hash;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                std::cout << "The client received the response from the hospital server\nusing TCP over port "
                          << port << ".\nThe following doctors are available:" << std::endl;
                std::vector<std::string> doctors = split_char(getf(r, "doctors"), ',');
                for (size_t i = 0; i < doctors.size(); ++i) {
                    if (!doctors[i].empty()) {
                        std::cout << doctors[i] << std::endl;
                    }
                }
            } else if (cmd == "lookup" && p.size() == 2) {
                std::string doctor = p[1];
                std::cout << "Patient " << username << " sent a lookup request to the\nhospital server for " << doctor << "." << std::endl;
                ProtoMessage q;
                q.type = "lookup_availability";
                q.fields["u_hash"] = u_hash;
                q.fields["doctor"] = doctor;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                std::vector<std::string> avail = split_char(getf(r, "avail"), ',');
                std::vector<std::string> clean;
                for (size_t i = 0; i < avail.size(); ++i) {
                    if (!avail[i].empty()) {
                        clean.push_back(avail[i]);
                    }
                }
                if (clean.size() == 8) {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port " << port
                              << ".\nAll time blocks are available for " << doctor << "." << std::endl;
                } else if (clean.empty()) {
                    std::cout << "The client received the response from the Hospital Server\nusing TCP over port " << port
                              << ".\n" << doctor << " has no time slots available." << std::endl;
                } else {
                    std::cout << "The client received the response from the Hospital Server\nusing TCP over port " << port
                              << ".\n" << doctor << " is available at times:" << std::endl;
                    for (size_t i = 0; i < clean.size(); ++i) {
                        std::cout << clean[i] << std::endl;
                    }
                }
            } else if (cmd == "schedule" && p.size() == 4) {
                std::string doctor = p[1];
                std::string start_time = p[2];
                std::string illness = p[3];
                std::cout << username << " sent an appointment schedule\nrequest to the hospital server." << std::endl;
                ProtoMessage q;
                q.type = "schedule";
                q.fields["u_hash"] = u_hash;
                q.fields["doctor"] = doctor;
                q.fields["time"] = start_time;
                q.fields["illness"] = illness;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                bool ok = is_true(getf(r, "ok", "0"));
                if (ok) {
                    std::cout << "The client received the response from the Hospital Server\nusing TCP over port " << port
                              << "\nAn appointment has been successfully scheduled for\npatient " << username
                              << " with " << doctor << " at\n" << start_time << "." << std::endl;
                } else {
                    std::vector<std::string> other = split_char(getf(r, "other_avail"), ',');
                    std::vector<std::string> clean;
                    for (size_t i = 0; i < other.size(); ++i) {
                        if (!other[i].empty()) {
                            clean.push_back(other[i]);
                        }
                    }
                    if (clean.empty()) {
                        std::cout << "The client received the response from the hospital server\nusing TCP over port " << port
                                  << "\nUnable to schedule an appointment with\n" << doctor
                                  << " at this time, as all time blocks have\nbeen taken up." << std::endl;
                    } else {
                        std::cout << "The client received the response from the hospital server\nusing TCP over port " << port
                                  << "\nUnable to schedule an appointment with\n" << doctor << " at " << start_time
                                  << ". Other available\ntime blocks are" << std::endl;
                        for (size_t i = 0; i < clean.size(); ++i) {
                            std::cout << clean[i] << std::endl;
                        }
                    }
                }
            } else if (cmd == "cancel" && p.size() == 1) {
                std::cout << username << " sent a cancellation request to the\nHospital Server." << std::endl;
                ProtoMessage q;
                q.type = "cancel";
                q.fields["u_hash"] = u_hash;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                if (!is_true(getf(r, "ok", "0"))) {
                    std::cout << "The client received the response from the Hospital Server\nusing TCP over port "
                              << port << "\nYou have no appointments available to cancel." << std::endl;
                } else {
                    std::cout << "The client received the response from the Hospital Server\nusing TCP over port "
                              << port << "\nYou have successfully cancelled your appointment with\n"
                              << getf(r, "doctor") << " at " << getf(r, "time") << "." << std::endl;
                }
            } else if (cmd == "view_appointment" && p.size() == 1) {
                std::cout << username << " sent a request to view their\nappointment to the Hospital Server." << std::endl;
                ProtoMessage q;
                q.type = "view_appointment";
                q.fields["u_hash"] = u_hash;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                if (!is_true(getf(r, "ok", "0"))) {
                    std::cout << "The client received the response from the hospital server\nusing TCP over client port "
                              << port << "\nYou do not have an appointment today." << std::endl;
                } else {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port " << port
                              << "\nYou have an appointment scheduled with " << getf(r, "doctor")
                              << "\nat " << getf(r, "time") << "." << std::endl;
                }
            } else if (cmd == "view_prescription" && p.size() == 1) {
                std::cout << username << " sent a request to view their\nprescription to the Hospital Server." << std::endl;
                ProtoMessage q;
                q.type = "view_prescription";
                q.fields["role"] = "patient";
                q.fields["patient_hash"] = u_hash;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                if (!is_true(getf(r, "ok", "0"))) {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port "
                              << port << "\nYou do not have a prescription to look up." << std::endl;
                } else {
                    std::string doctor = getf(r, "doctor");
                    std::string treatment = getf(r, "treatment");
                    std::string frequency = getf(r, "frequency");
                    if (frequency == "None") {
                        std::cout << "The client received the response from the hospital server\nusing TCP over port "
                                  << port << "\nYou were not prescribed any treatment by\n"
                                  << doctor << " following your diagnosis." << std::endl;
                    } else {
                        std::cout << "The client received the response from the hospital server\nusing TCP over port "
                                  << port << "\nYou have been prescribed " << treatment << ", to be taken\n"
                                  << frequency << ", by " << doctor << "." << std::endl;
                    }
                }
            }
        } else {
            // ---------------- Doctor commands ----------------
            std::string canonical_doctor = doctor_name.empty() ? username : doctor_name;
            if (cmd == "view_appointments" && p.size() == 1) {
                std::cout << canonical_doctor << " sent a request to view their\nscheduled appointments to the Hospital Server." << std::endl;
                ProtoMessage q;
                q.type = "view_appointments";
                q.fields["doctor"] = canonical_doctor;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                std::vector<std::string> times = split_char(getf(r, "times"), ',');
                std::vector<std::string> clean;
                for (size_t i = 0; i < times.size(); ++i) {
                    if (!times[i].empty()) {
                        clean.push_back(times[i]);
                    }
                }
                if (clean.empty()) {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port "
                              << port << "\nYou do not have any appointments scheduled." << std::endl;
                } else {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port "
                              << port << "\n" << canonical_doctor << " is scheduled at times:" << std::endl;
                    for (size_t i = 0; i < clean.size(); ++i) {
                        std::cout << clean[i] << std::endl;
                    }
                }
            } else if (cmd == "prescribe" && p.size() == 3) {
                std::string patient = p[1];
                std::string frequency = p[2];
                std::cout << canonical_doctor << " sent a request to the Hospital Server\nto prescribe " << patient
                          << " following their\ndiagnosis." << std::endl;
                ProtoMessage q;
                q.type = "prescribe";
                q.fields["doctor"] = canonical_doctor;
                q.fields["patient_hash"] = sha256_hash_trimmed(patient);
                q.fields["frequency"] = frequency;
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                if (is_true(getf(r, "ok", "0"))) {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port " << port
                              << "\nYou have successfully prescribed " << patient << " with\n"
                              << getf(r, "treatment", "None") << ", to be taken " << frequency << "." << std::endl;
                }
            } else if (cmd == "view_prescription" && p.size() == 2) {
                std::string patient = p[1];
                std::cout << canonical_doctor << " sent a request to view\n" << patient
                          << " prescription to the Hospital Server." << std::endl;
                ProtoMessage q;
                q.type = "view_prescription";
                q.fields["role"] = "doctor";
                q.fields["doctor"] = canonical_doctor;
                q.fields["patient_hash"] = sha256_hash_trimmed(patient);
                ProtoMessage r;
                int port = 0;
                send_req(q, r, port);
                if (!is_true(getf(r, "ok", "0"))) {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port " << port
                              << "\n" << patient << " does not have a prescription." << std::endl;
                } else {
                    std::cout << "The client received the response from the hospital server\nusing TCP over port " << port
                              << "\n" << patient << " has been prescribed " << getf(r, "treatment") << ", to\nbe taken "
                              << getf(r, "frequency") << ", by\n" << getf(r, "doctor") << "." << std::endl;
                }
            }
        }
    }

    return 0;
}
