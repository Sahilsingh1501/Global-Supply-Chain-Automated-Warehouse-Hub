#ifndef PERSON_D_SUPPORT_H
#define PERSON_D_SUPPORT_H

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

}  // namespace PersonD

#endif
