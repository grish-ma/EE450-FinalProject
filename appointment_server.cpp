#include "constants.h"
#include "crypto_utils.h"
#include "file_utils.h"
#include "net_utils.h"
#include "text_proto.h"

#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct TimeBlock {
    int hh;
    int mm;
};

static bool parse_time_block(const std::string &s, TimeBlock &tb) {
    std::vector<std::string> parts = split_char(s, ':');
    if (parts.size() != 2) {
        return false;
    }
    tb.hh = std::atoi(parts[0].c_str());
    tb.mm = std::atoi(parts[1].c_str());
    if (tb.mm != 0) {
        return false;
    }
    return true;
}

static bool allowed_time_block(const TimeBlock &tb) {
    return tb.mm == 0 && tb.hh >= 9 && tb.hh <= 16;
}

static bool is_header_line(const std::string &s) {
    return !s.empty() && s.find(':') == std::string::npos;
}

static std::map<std::string, std::vector<std::string> > parse_appointments(const std::vector<std::string> &lines) {
    std::map<std::string, std::vector<std::string> > doctors;
    std::string cur;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string s = trim_copy(lines[i]);
        if (s.empty()) {
            continue;
        }
        if (is_header_line(s)) {
            cur = s;
            if (doctors.find(cur) == doctors.end()) {
                doctors[cur] = std::vector<std::string>();
            }
            continue;
        }
        if (!cur.empty()) {
            doctors[cur].push_back(s);
        }
    }
    return doctors;
}

static bool load_appointments(const std::string &path, std::map<std::string, std::vector<std::string> > &doctors) {
    std::vector<std::string> lines;
    if (!read_lines(path, lines)) {
        doctors.clear();
        return false;
    }
    doctors = parse_appointments(lines);
    return true;
}

static bool save_appointments(const std::string &path, const std::map<std::string, std::vector<std::string> > &doctors) {
    std::vector<std::string> lines;
    for (std::map<std::string, std::vector<std::string> >::const_iterator it = doctors.begin(); it != doctors.end(); ++it) {
        lines.push_back(it->first);
        for (size_t i = 0; i < it->second.size(); ++i) {
            lines.push_back(it->second[i]);
        }
    }
    return write_lines(path, lines);
}

static std::vector<std::string> split_ws(const std::string &s) {
    std::vector<std::string> raw = split_char(s, ' ');
    std::vector<std::string> out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (!raw[i].empty()) {
            out.push_back(raw[i]);
        }
    }
    return out;
}

static std::vector<std::string> blocks_available(const std::vector<std::string> &block_lines) {
    std::vector<std::string> avail;
    for (size_t i = 0; i < block_lines.size(); ++i) {
        std::vector<std::string> p = split_ws(block_lines[i]);
        if (p.size() == 1) {
            avail.push_back(p[0]);
        }
    }
    return avail;
}

static bool find_patient_appointment(
    const std::map<std::string, std::vector<std::string> > &doctors,
    const std::string &patient_hash,
    std::string &doctor_out,
    std::string &time_out,
    std::string &illness_out) {
    for (std::map<std::string, std::vector<std::string> >::const_iterator it = doctors.begin(); it != doctors.end(); ++it) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            std::vector<std::string> p = split_ws(it->second[i]);
            if (p.size() >= 3 && p[1] == patient_hash) {
                doctor_out = it->first;
                time_out = p[0];
                illness_out = p[2];
                return true;
            }
        }
    }
    return false;
}

static bool cancel_patient(
    std::map<std::string, std::vector<std::string> > &doctors,
    const std::string &patient_hash,
    std::string &doctor_out,
    std::string &time_out) {
    for (std::map<std::string, std::vector<std::string> >::iterator it = doctors.begin(); it != doctors.end(); ++it) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            std::vector<std::string> p = split_ws(it->second[i]);
            if (p.size() >= 3 && p[1] == patient_hash) {
                doctor_out = it->first;
                time_out = p[0];
                it->second[i] = p[0];
                return true;
            }
        }
    }
    return false;
}

static bool schedule_patient(
    std::map<std::string, std::vector<std::string> > &doctors,
    const std::string &doctor,
    const std::string &time_block,
    const std::string &patient_hash,
    const std::string &illness,
    std::vector<std::string> &other_available) {
    other_available.clear();
    std::map<std::string, std::vector<std::string> >::iterator it = doctors.find(doctor);
    if (it == doctors.end()) {
        return false;
    }

    TimeBlock tb;
    if (!parse_time_block(time_block, tb) || !allowed_time_block(tb)) {
        other_available = blocks_available(it->second);
        return false;
    }

    for (size_t i = 0; i < it->second.size(); ++i) {
        std::vector<std::string> p = split_ws(it->second[i]);
        if (p.empty()) {
            continue;
        }
        if (p[0] == time_block) {
            if (p.size() == 1) {
                it->second[i] = time_block + " " + patient_hash + " " + illness;
                return true;
            }
            other_available = blocks_available(it->second);
            return false;
        }
    }

    other_available = blocks_available(it->second);
    return false;
}

static std::vector<std::string> doctor_scheduled_times(const std::vector<std::string> &block_lines) {
    std::vector<std::string> out;
    for (size_t i = 0; i < block_lines.size(); ++i) {
        std::vector<std::string> p = split_ws(block_lines[i]);
        if (p.size() >= 3) {
            out.push_back(p[0]);
        }
    }
    return out;
}

int main() {
    std::map<std::string, std::vector<std::string> > doctors;
    load_appointments("appointments.txt", doctors);

    int udp_fd = create_udp_bound_socket(HOST, APPT_UDP_PORT);
    if (udp_fd < 0) {
        return 1;
    }

    std::cout << "Appointment Server is up and running using UDP on port\n" << APPT_UDP_PORT << "." << std::endl;

    while (true) {
        std::string payload;
        std::string ip;
        int port = 0;
        if (!udp_recv_from(udp_fd, payload, ip, port)) {
            continue;
        }

        ProtoMessage req;
        if (!proto_parse(payload, req)) {
            continue;
        }

        if (req.type == "doctor_list_req") {
            std::cout << "The Appointment Server has received a doctor availability\nrequest." << std::endl;
            ProtoMessage resp;
            resp.type = "doctor_list_resp";
            std::vector<std::string> names;
            for (std::map<std::string, std::vector<std::string> >::iterator it = doctors.begin(); it != doctors.end(); ++it) {
                names.push_back(it->first);
            }
            resp.fields["doctors"] = join_strings(names, ",");
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
            std::cout << "The Appointment Server has sent the lookup result to the\nHospital Server." << std::endl;
        } else if (req.type == "doctor_avail_req") {
            std::cout << "The Appointment Server has received a doctor availability\nrequest." << std::endl;
            std::string doctor = req.fields["doctor"];
            std::vector<std::string> avail;
            if (doctors.find(doctor) != doctors.end()) {
                avail = blocks_available(doctors[doctor]);
            }
            if (avail.size() == 8) {
                std::cout << "All time blocks are available for " << doctor << "." << std::endl;
            } else if (!avail.empty()) {
                std::cout << doctor << " has some time slots available." << std::endl;
            } else {
                std::cout << doctor << " has no time slots available." << std::endl;
            }
            ProtoMessage resp;
            resp.type = "doctor_avail_resp";
            resp.fields["doctor"] = doctor;
            resp.fields["avail"] = join_strings(avail, ",");
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
            std::cout << "The Appointment Server has sent the lookup result to the\nHospital Server." << std::endl;
        } else if (req.type == "schedule_req") {
            std::string doctor = req.fields["doctor"];
            std::string time_block = req.fields["time"];
            std::string patient_hash = req.fields["patient_hash"];
            std::string illness = req.fields["illness"];
            std::cout << "Appointment scheduling request received (time:\n" << time_block
                      << ", doctor: " << doctor << ", patient hash suffix:\n"
                      << hash_suffix5(patient_hash) << ", illness: " << illness << ")." << std::endl;
            std::vector<std::string> other;
            bool ok = schedule_patient(doctors, doctor, time_block, patient_hash, illness, other);
            if (ok) {
                save_appointments("appointments.txt", doctors);
                std::cout << "Appointment has been scheduled successfully for user\n" << hash_suffix5(patient_hash)
                          << " with " << doctor << "." << std::endl;
            } else {
                std::cout << "The requested appointment time is not available." << std::endl;
            }
            ProtoMessage resp;
            resp.type = "schedule_resp";
            resp.fields["ok"] = ok ? "1" : "0";
            resp.fields["doctor"] = doctor;
            resp.fields["time"] = time_block;
            resp.fields["other_avail"] = join_strings(other, ",");
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        } else if (req.type == "view_appt_req") {
            std::string patient_hash = req.fields["patient_hash"];
            std::cout << "Appointment Server has received a view appointment\ncommand for the user with hash suffix "
                      << hash_suffix5(patient_hash) << "." << std::endl;
            std::string doctor;
            std::string time;
            std::string illness;
            bool found = find_patient_appointment(doctors, patient_hash, doctor, time, illness);
            ProtoMessage resp;
            resp.type = "view_appt_resp";
            if (found) {
                std::cout << "Returning details regarding the appointment for the user\nwith hash suffix "
                          << hash_suffix5(patient_hash) << "." << std::endl;
                resp.fields["ok"] = "1";
                resp.fields["doctor"] = doctor;
                resp.fields["time"] = time;
            } else {
                std::cout << "The user with hash suffix " << hash_suffix5(patient_hash) << " has no\nappointment in the system." << std::endl;
                resp.fields["ok"] = "0";
            }
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        } else if (req.type == "cancel_req") {
            std::string patient_hash = req.fields["patient_hash"];
            std::cout << "Appointment Server has received a cancel appointment\ncommand for the user with hash suffix: "
                      << hash_suffix5(patient_hash) << "." << std::endl;
            std::string doctor;
            std::string time;
            bool ok = cancel_patient(doctors, patient_hash, doctor, time);
            ProtoMessage resp;
            resp.type = "cancel_resp";
            if (ok) {
                save_appointments("appointments.txt", doctors);
                std::cout << "Successfully cancelled appointment." << std::endl;
                resp.fields["ok"] = "1";
                resp.fields["doctor"] = doctor;
                resp.fields["time"] = time;
            } else {
                std::cout << "Error: Failed to find appointment." << std::endl;
                resp.fields["ok"] = "0";
            }
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        } else if (req.type == "view_appointments_req") {
            std::string doctor = req.fields["doctor"];
            std::cout << "Appointment Server has received a request to view\nappointments scheduled for " << doctor << "." << std::endl;
            std::vector<std::string> times;
            if (doctors.find(doctor) != doctors.end()) {
                times = doctor_scheduled_times(doctors[doctor]);
            }
            if (times.empty()) {
                std::cout << "No appointments have been made for\n" << doctor << "." << std::endl;
            } else {
                std::cout << "Returning the scheduled appointments for\n" << doctor << "." << std::endl;
            }
            ProtoMessage resp;
            resp.type = "view_appointments_resp";
            resp.fields["doctor"] = doctor;
            resp.fields["times"] = join_strings(times, ",");
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        } else if (req.type == "fetch_illness_req") {
            std::string doctor = req.fields["doctor"];
            std::string patient_hash = req.fields["patient_hash"];
            std::cout << "Appointment Server has received a request from Hospital\nServer regarding information about a user with hash suffix\n"
                      << hash_suffix5(patient_hash) << " from " << doctor << "." << std::endl;
            std::string found_doc;
            std::string found_time;
            std::string illness;
            bool found = find_patient_appointment(doctors, patient_hash, found_doc, found_time, illness);
            std::cout << "Sending back the requested information to the Hospital\nserver." << std::endl;
            if (found) {
                std::string cancel_doc;
                std::string cancel_time;
                cancel_patient(doctors, patient_hash, cancel_doc, cancel_time);
                save_appointments("appointments.txt", doctors);
                std::cout << "Successfully removed " << hash_suffix5(patient_hash) << " appointment slot,\n"
                          << cancel_time << " is now free to be scheduled for tomorrow." << std::endl;
            }
            ProtoMessage resp;
            resp.type = "fetch_illness_resp";
            resp.fields["ok"] = found ? "1" : "0";
            resp.fields["illness"] = found ? illness : "";
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        }
    }

    close(udp_fd);
    return 0;
}
