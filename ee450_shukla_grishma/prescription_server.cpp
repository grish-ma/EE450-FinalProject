#include "constants.h"
#include "crypto_utils.h"
#include "file_utils.h"
#include "net_utils.h"
#include "text_proto.h"

#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

struct PrescRow {
    std::string doctor;
    std::string patient_hash;
    std::string treatment;
    std::string frequency;
};

// Split a line by spaces and remove empty pieces.
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

// Read prescriptions from prescriptions.txt into memory.
static std::vector<PrescRow> load_rows(const std::string &path) {
    std::vector<PrescRow> out;
    std::vector<std::string> lines;
    if (!read_lines(path, lines)) {
        return out;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        std::vector<std::string> p = split_ws(trim_copy(lines[i]));
        if (p.size() >= 4) {
            PrescRow r;
            r.doctor = p[0];
            r.patient_hash = p[1];
            r.treatment = p[2];
            r.frequency = p[3];
            out.push_back(r);
        }
    }
    return out;
}

// Save in-memory prescription rows back to file
static bool save_rows(const std::string &path, const std::vector<PrescRow> &rows) {
    std::vector<std::string> lines;
    for (size_t i = 0; i < rows.size(); ++i) {
        lines.push_back(rows[i].doctor + " " + rows[i].patient_hash + " " + rows[i].treatment + " " + rows[i].frequency);
    }
    return write_lines(path, lines);
}

// Replace old record for patient, then add new
static void upsert(std::vector<PrescRow> &rows, const PrescRow &row) {
    std::vector<PrescRow> filtered;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].patient_hash != row.patient_hash) {
            filtered.push_back(rows[i]);
        }
    }
    filtered.push_back(row);
    rows = filtered;
}

// Find a patient's prescription record.
static bool find_by_patient(const std::vector<PrescRow> &rows, const std::string &patient_hash, PrescRow &row_out) {
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].patient_hash == patient_hash) {
            row_out = rows[i];
            return true;
        }
    }
    return false;
}

int main() {
    // Load file data once when server starts
    std::vector<PrescRow> rows = load_rows("prescriptions.txt");

    // Create and bind UDP socket for Prescription Server
    int udp_fd = create_udp_bound_socket(HOST, PRESC_UDP_PORT);
    if (udp_fd < 0) {
        return 1;
    }

    std::cout << "Prescription Server is up and running using UDP on port " << PRESC_UDP_PORT << "." << std::endl;

    // Keep serving requests until Ctrl-C.
    while (true) {
        // Wait for one UDP message.
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

        if (req.type == "prescribe_req") {
            // Doctor is saving a prescription for a patient.
            std::string doctor = req.fields["doctor"];
            std::string patient_hash = req.fields["patient_hash"];
            std::string treatment = req.fields["treatment"];
            std::string frequency = req.fields["frequency"];
            std::cout << "Prescription Server has received a request from\n" << doctor
                      << " to prescribe the user with hash suffix\n" << hash_suffix5(patient_hash) << "." << std::endl;
            PrescRow row;
            row.doctor = doctor;
            row.patient_hash = patient_hash;
            row.treatment = treatment;
            row.frequency = frequency;
            upsert(rows, row);
            save_rows("prescriptions.txt", rows);
            std::cout << "Successfully saved the prescription details for user with\nhash suffix: "
                      << hash_suffix5(patient_hash) << "." << std::endl;
            ProtoMessage resp;
            resp.type = "prescribe_resp";
            resp.fields["ok"] = "1";
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        } else if (req.type == "view_prescription_req") {
            // Doctor or patient is asking to view prescription.
            std::string patient_hash = req.fields["patient_hash"];
            std::cout << "The prescription server has received a request to view the\nprescription for the user with hash suffix: "
                      << hash_suffix5(patient_hash) << "." << std::endl;
            PrescRow row;
            bool found = find_by_patient(rows, patient_hash, row);
            ProtoMessage resp;
            resp.type = "view_prescription_resp";
            if (!found || row.frequency == "None") {
                std::cout << "There are no current prescriptions for this user." << std::endl;
                resp.fields["ok"] = "0";
            } else {
                std::cout << "A prescription exists for this user." << std::endl;
                resp.fields["ok"] = "1";
                resp.fields["doctor"] = row.doctor;
                resp.fields["treatment"] = row.treatment;
                resp.fields["frequency"] = row.frequency;
            }
            udp_send_to(udp_fd, ip, port, proto_serialize(resp));
        }
    }

    close(udp_fd);
    return 0;
}
