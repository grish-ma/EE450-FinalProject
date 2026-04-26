#include "file_utils.h"

#include <fstream>

bool read_lines(const std::string &path, std::vector<std::string> &lines_out) {
    lines_out.clear();
    std::ifstream fin(path.c_str());
    if (!fin.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(fin, line)) {
        lines_out.push_back(line);
    }
    return true;
}

bool write_lines(const std::string &path, const std::vector<std::string> &lines) {
    std::ofstream fout(path.c_str(), std::ios::out | std::ios::trunc);
    if (!fout.is_open()) {
        return false;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        fout << lines[i];
        if (i + 1 < lines.size()) {
            fout << "\n";
        } else {
            fout << "\n";
        }
    }
    return true;
}
