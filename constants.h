#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>

static const std::string HOST = "127.0.0.1";
static const int USC_ID_SUFFIX = 818;

static const int AUTH_UDP_PORT = 21000 + USC_ID_SUFFIX;   // 21818
static const int PRESC_UDP_PORT = 22000 + USC_ID_SUFFIX;  // 22818
static const int APPT_UDP_PORT = 23000 + USC_ID_SUFFIX;   // 23818
static const int HOSP_UDP_PORT = 25000 + USC_ID_SUFFIX;   // 25818
static const int HOSP_TCP_PORT = 26000 + USC_ID_SUFFIX;   // 26818

#endif
