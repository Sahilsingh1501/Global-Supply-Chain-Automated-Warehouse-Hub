#ifndef PERSON_D_SUPPORT_H
#define PERSON_D_SUPPORT_H

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace PersonD {

const std::size_t MAX_RECORDS = 100000;
const std::size_t MAX_TEXT_LENGTH = 100000;

inline bool writeString(std::ostream& output, const std::string& value) {
    output << value.size() << '\n';
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output.put('\n');
    return static_cast<bool>(output);
}

inline bool readString(std::istream& input, std::string& value) {
    std::size_t length = 0;
    if (!(input >> length) || length > MAX_TEXT_LENGTH) return false;
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    value.assign(length, '\0');
    if (length && !input.read(&value[0], static_cast<std::streamsize>(length))) return false;
    return input.get() == '\n';
}

inline bool readCount(std::istream& input, std::size_t& count) {
    return static_cast<bool>(input >> count) && count <= MAX_RECORDS;
}

inline std::string cleanField(const std::string& value) {
    std::string result = value;
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (result[i] == '\r' || result[i] == '\n' || result[i] == '|')
            result[i] = ' ';
    }
    return result;
}

class AuditLog {
    std::ofstream output;

public:
    AuditLog() : output("audit.log", std::ios::app) {}
    bool isOpen() const { return output.is_open(); }

    bool record(const std::string& action, const std::string& details) {
        const std::time_t now = std::time(nullptr);
        std::tm* local = std::localtime(&now);
        if (!local) {
            std::cerr << "Audit log error: could not determine the current time.\n";
            return false;
        }
        char timestamp[32];
        if (std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local) == 0) {
            std::cerr << "Audit log error: could not format the current time.\n";
            return false;
        }
        output << timestamp << " | " << cleanField(action)
               << " | " << cleanField(details) << '\n';
        output.flush();
        if (!output) {
            std::cerr << "Audit log error: failed to write audit.log.\n";
            return false;
        }
        return true;
    }
};

}  // namespace PersonD

#endif
