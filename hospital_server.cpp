#include "constants.h"
#include "crypto_utils.h"
#include "file_utils.h"
#include "net_utils.h"
#include "text_proto.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

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

static void load_hospital_data(
    const std::string &path,
    std::map<std::string, std::string> &doctor_hash_to_name,
    std::map<std::string, std::string> &illness_to_treatment) {
    doctor_hash_to_name.clear();
    illness_to_treatment.clear();
    std::vector<std::string> lines;
    if (!read_lines(path, lines)) {
        return;
    }
    std::string section;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string s = trim_copy(lines[i]);
        if (s.empty()) {
            continue;
        }
        if (s == "[Doctors]") {
            section = "doctors";
            continue;
        }
        if (s == "[Treatments]") {
            section = "treatments";
            continue;
        }
        std::vector<std::string> p = split_ws(s);
        if (section == "doctors" && p.size() >= 2) {
            doctor_hash_to_name[p[1]] = p[0];
        } else if (section == "treatments" && p.size() >= 2) {
            illness_to_treatment[p[0]] = p[1];
        }
    }
}

static bool udp_rpc(int udp_fd, int dest_port, const ProtoMessage &req, ProtoMessage &resp) {
    std::string encoded = proto_serialize(req);
    if (!udp_send_to(udp_fd, HOST, dest_port, encoded)) {
        return false;
    }
    std::string payload;
    if (!udp_recv_with_timeout(udp_fd, 3000, payload)) {
        return false;
    }
    return proto_parse(payload, resp);
}

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

int main() {
    int udp_fd = create_udp_bound_socket(HOST, HOSP_UDP_PORT);
    if (udp_fd < 0) {
        return 1;
    }
    int listener = create_tcp_listener(HOST, HOSP_TCP_PORT, 10);
    if (listener < 0) {
        close(udp_fd);
        return 1;
    }

    std::cout << "Hospital Server is up and running using UDP on port\n" << HOSP_UDP_PORT << "." << std::endl;

    while (true) {
        int conn = accept(listener, NULL, NULL);
        if (conn < 0) {
            continue;
        }

        std::string line;
        if (!recv_line_tcp(conn, line)) {
            close(conn);
            continue;
        }

        ProtoMessage req;
        if (!proto_parse(line, req)) {
            close(conn);
            continue;
        }

        if (req.type == "auth") {
            std::string u_hash = getf(req, "u_hash");
            std::string p_hash = getf(req, "p_hash");
            std::string suffix = hash_suffix5(u_hash);
            std::cout << "Hospital Server received an authentication request from a\nuser with hash suffix " << suffix << "." << std::endl;
            std::cout << "Hospital Server has sent an authentication request to the\nAuthentication Server." << std::endl;
            ProtoMessage a_req;
            a_req.type = "auth_req";
            a_req.fields["u_hash"] = u_hash;
            a_req.fields["p_hash"] = p_hash;
            ProtoMessage a_resp;
            bool ok_rpc = udp_rpc(udp_fd, AUTH_UDP_PORT, a_req, a_resp);
            std::cout << "Hospital server has received the response from the\nauthentication server using UDP over port "
                      << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "auth_result";
            bool ok = ok_rpc && is_true(getf(a_resp, "ok", "0"));
            if (!ok) {
                out.fields["ok"] = "0";
                send_all(conn, proto_serialize(out) + "\n");
                close(conn);
                continue;
            }

            std::cout << "User with a hash suffix " << suffix << " has been granted\naccess to the system. Determining the access of the user." << std::endl;
            std::map<std::string, std::string> doctor_hash_to_name;
            std::map<std::string, std::string> illness_to_treatment;
            load_hospital_data("hospital.txt", doctor_hash_to_name, illness_to_treatment);
            std::map<std::string, std::string>::iterator dit = doctor_hash_to_name.find(u_hash);
            if (dit != doctor_hash_to_name.end()) {
                std::cout << "User with hash suffix " << suffix << " will be granted doctor\naccess." << std::endl;
                out.fields["ok"] = "1";
                out.fields["role"] = "doctor";
                out.fields["doctor_name"] = dit->second;
            } else {
                std::cout << "User with hash " << suffix << " will be granted patient\naccess." << std::endl;
                out.fields["ok"] = "1";
                out.fields["role"] = "patient";
                out.fields["doctor_name"] = "";
            }
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "Hospital Server has sent the response from\nAuthentication Server to the client using TCP over port\n"
                      << HOSP_TCP_PORT << "." << std::endl;
        } else if (req.type == "lookup_doctors") {
            std::string u_hash = getf(req, "u_hash");
            std::string suffix = hash_suffix5(u_hash);
            std::cout << "Hospital Server received a lookup request from a user\nwith a hash suffix " << suffix << " over port " << HOSP_TCP_PORT << "." << std::endl;
            std::cout << "Hospital Server sent the doctor lookup request to the\nAppointment server." << std::endl;
            ProtoMessage a_req;
            a_req.type = "doctor_list_req";
            ProtoMessage a_resp;
            udp_rpc(udp_fd, APPT_UDP_PORT, a_req, a_resp);
            std::cout << "Hospital Server has received the response from\nAppointment Server using UDP over port " << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "lookup_doctors_resp";
            out.fields["doctors"] = getf(a_resp, "doctors");
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "Hospital Server has sent the doctor lookup to the client." << std::endl;
        } else if (req.type == "lookup_availability") {
            std::string u_hash = getf(req, "u_hash");
            std::string doctor = getf(req, "doctor");
            std::string suffix = hash_suffix5(u_hash);
            std::cout << "Hospital Server has received a lookup request from a\nuser with hash suffix " << suffix
                      << " to lookup\n" << doctor << " availability using TCP over port\n" << HOSP_TCP_PORT << "." << std::endl;
            std::cout << "Hospital Server sent the doctor lookup request to the\nAppointment server." << std::endl;
            ProtoMessage a_req;
            a_req.type = "doctor_avail_req";
            a_req.fields["doctor"] = doctor;
            ProtoMessage a_resp;
            udp_rpc(udp_fd, APPT_UDP_PORT, a_req, a_resp);
            std::cout << "Hospital Server has received the response from\nAppointment Server using UDP over port " << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "lookup_availability_resp";
            out.fields["doctor"] = doctor;
            out.fields["avail"] = getf(a_resp, "avail");
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "The Hospital Server has sent the response to the client." << std::endl;
        } else if (req.type == "schedule") {
            std::string u_hash = getf(req, "u_hash");
            std::string doctor = getf(req, "doctor");
            std::string time = getf(req, "time");
            std::string illness = getf(req, "illness");
            std::string suffix = hash_suffix5(u_hash);
            std::cout << "Hospital Server has received a schedule request from a\nuser with hash suffix: " << suffix
                      << " to book an\nappointment using TCP over port " << HOSP_TCP_PORT << "." << std::endl;
            std::cout << "Hospital Server has sent the schedule request to the\nappointment server." << std::endl;
            ProtoMessage a_req;
            a_req.type = "schedule_req";
            a_req.fields["doctor"] = doctor;
            a_req.fields["time"] = time;
            a_req.fields["patient_hash"] = u_hash;
            a_req.fields["illness"] = illness;
            ProtoMessage a_resp;
            udp_rpc(udp_fd, APPT_UDP_PORT, a_req, a_resp);
            std::cout << "Hospital Server has received the response from\nAppointment Server using UDP over "
                      << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "schedule_resp";
            out.fields["ok"] = getf(a_resp, "ok", "0");
            out.fields["doctor"] = doctor;
            out.fields["time"] = time;
            out.fields["other_avail"] = getf(a_resp, "other_avail");
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "The hospital server has sent the response to the client." << std::endl;
        } else if (req.type == "cancel") {
            std::string u_hash = getf(req, "u_hash");
            std::string suffix = hash_suffix5(u_hash);
            std::cout << "Hospital Server has received a cancel request from user\nwith hash suffix: " << suffix
                      << " to cancel their appointment\nusing TCP over port " << HOSP_TCP_PORT << "." << std::endl;
            std::cout << "The hospital server has sent the cancel request to the\nappointment server." << std::endl;
            ProtoMessage a_req;
            a_req.type = "cancel_req";
            a_req.fields["patient_hash"] = u_hash;
            ProtoMessage a_resp;
            udp_rpc(udp_fd, APPT_UDP_PORT, a_req, a_resp);
            std::cout << "Hospital Server has received the response from\nAppointment Server using UDP over port "
                      << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "cancel_resp";
            out.fields["ok"] = getf(a_resp, "ok", "0");
            out.fields["doctor"] = getf(a_resp, "doctor");
            out.fields["time"] = getf(a_resp, "time");
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "The hospital server has sent the response to the client." << std::endl;
        } else if (req.type == "view_appointment") {
            std::string u_hash = getf(req, "u_hash");
            std::string suffix = hash_suffix5(u_hash);
            std::cout << "Hospital server has received a view appointment request\nfrom a user with hash suffix " << suffix
                      << " to view their\nappointment details using TCP over port " << HOSP_TCP_PORT << "." << std::endl;
            std::cout << "Hospital Server has sent the view appointments request\nto the Appointment Server." << std::endl;
            ProtoMessage a_req;
            a_req.type = "view_appt_req";
            a_req.fields["patient_hash"] = u_hash;
            ProtoMessage a_resp;
            udp_rpc(udp_fd, APPT_UDP_PORT, a_req, a_resp);
            std::cout << "Hospital Server has received the response from the\nappointment server using UDP over port "
                      << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "view_appointment_resp";
            out.fields["ok"] = getf(a_resp, "ok", "0");
            out.fields["doctor"] = getf(a_resp, "doctor");
            out.fields["time"] = getf(a_resp, "time");
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "The hospital server has sent the response to the client." << std::endl;
        } else if (req.type == "view_appointments") {
            std::string doctor = getf(req, "doctor");
            std::cout << "Hospital Server has received a view appointments\nrequest from " << doctor
                      << " to view their schedule\ndetails using TCP over port " << HOSP_TCP_PORT << "." << std::endl;
            std::cout << "The hospital server has sent the view appointments\nrequest to the Appointment Server." << std::endl;
            ProtoMessage a_req;
            a_req.type = "view_appointments_req";
            a_req.fields["doctor"] = doctor;
            ProtoMessage a_resp;
            udp_rpc(udp_fd, APPT_UDP_PORT, a_req, a_resp);
            std::cout << "Hospital server has received the response from the\nAppointment server using UDP over port "
                      << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "view_appointments_resp";
            out.fields["doctor"] = doctor;
            out.fields["times"] = getf(a_resp, "times");
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "The hospital server has sent the response to the client." << std::endl;
        } else if (req.type == "prescribe") {
            std::string doctor = getf(req, "doctor");
            std::string patient_hash = getf(req, "patient_hash");
            std::string frequency = getf(req, "frequency");
            std::cout << "Hospital Server has received a prescription request from\n" << doctor
                      << " for a user with hash suffix\n" << hash_suffix5(patient_hash)
                      << " using TCP over port " << HOSP_TCP_PORT << "." << std::endl;
            std::cout << "Hospital Server has sent a request to fetch patients with\nhash suffix " << hash_suffix5(patient_hash)
                      << " illness information to the\nAppointment Server." << std::endl;

            ProtoMessage fetch_req;
            fetch_req.type = "fetch_illness_req";
            fetch_req.fields["doctor"] = doctor;
            fetch_req.fields["patient_hash"] = patient_hash;
            ProtoMessage fetch_resp;
            bool got_fetch = udp_rpc(udp_fd, APPT_UDP_PORT, fetch_req, fetch_resp);
            std::cout << "Hospital Server has received the illness response from\nthe Appointment server using UDP over port "
                      << HOSP_UDP_PORT << "." << std::endl;

            std::string illness = got_fetch ? getf(fetch_resp, "illness") : "";
            std::map<std::string, std::string> doctor_hash_to_name;
            std::map<std::string, std::string> illness_to_treatment;
            load_hospital_data("hospital.txt", doctor_hash_to_name, illness_to_treatment);
            std::string treatment = "None";

            ProtoMessage out;
            out.type = "prescribe_resp";
            if (illness.empty()) {
                out.fields["ok"] = "0";
                send_all(conn, proto_serialize(out) + "\n");
                close(conn);
                continue;
            }

            std::cout << "Acquiring treatment for " << illness << " from the database." << std::endl;
            if (illness_to_treatment.find(illness) != illness_to_treatment.end()) {
                treatment = illness_to_treatment[illness];
            }
            std::cout << "Hospital server has sent the prescription request to the\nprescription server to prescribe "
                      << treatment << "." << std::endl;
            ProtoMessage p_req;
            p_req.type = "prescribe_req";
            p_req.fields["doctor"] = doctor;
            p_req.fields["patient_hash"] = patient_hash;
            p_req.fields["treatment"] = treatment;
            p_req.fields["frequency"] = frequency;
            ProtoMessage p_resp;
            bool got_p = udp_rpc(udp_fd, PRESC_UDP_PORT, p_req, p_resp);
            std::cout << "Hospital server has received the response from the\nprescription server using UDP over port "
                      << HOSP_UDP_PORT << std::endl;
            out.fields["ok"] = (got_p && is_true(getf(p_resp, "ok", "0"))) ? "1" : "0";
            out.fields["treatment"] = treatment;
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "The hospital server has sent the response to the client." << std::endl;
        } else if (req.type == "view_prescription") {
            std::string role = getf(req, "role", "patient");
            std::string doctor = getf(req, "doctor");
            std::string patient_hash = getf(req, "patient_hash");
            if (role == "patient") {
                std::cout << "Hospital Server has received a prescription request from\na patient with hash suffix "
                          << hash_suffix5(patient_hash) << " to view their\nprescription details using TCP over port "
                          << HOSP_TCP_PORT << "." << std::endl;
            } else {
                std::cout << "Hospital Server has received a prescription request from\n" << doctor
                          << " to view a patient with hash suffix\n" << hash_suffix5(patient_hash)
                          << " prescription details using TCP over port\n" << HOSP_TCP_PORT << "." << std::endl;
            }
            std::cout << "Hospital Server has sent the prescription request to the\nPrescription Server." << std::endl;
            ProtoMessage p_req;
            p_req.type = "view_prescription_req";
            p_req.fields["patient_hash"] = patient_hash;
            ProtoMessage p_resp;
            udp_rpc(udp_fd, PRESC_UDP_PORT, p_req, p_resp);
            std::cout << "Hospital server has received the response from the\nprescription server using UDP over port "
                      << HOSP_UDP_PORT << "." << std::endl;
            ProtoMessage out;
            out.type = "view_prescription_resp";
            out.fields["ok"] = getf(p_resp, "ok", "0");
            out.fields["doctor"] = getf(p_resp, "doctor");
            out.fields["treatment"] = getf(p_resp, "treatment");
            out.fields["frequency"] = getf(p_resp, "frequency");
            send_all(conn, proto_serialize(out) + "\n");
            std::cout << "Hospital server has sent the response to the client." << std::endl;
        }

        close(conn);
    }

    close(listener);
    close(udp_fd);
    return 0;
}
