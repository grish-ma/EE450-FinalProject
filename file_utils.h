#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <vector>

bool read_lines(const std::string &path, std::vector<std::string> &lines_out);
bool write_lines(const std::string &path, const std::vector<std::string> &lines);

#endif
