
#include "validator.h"

#include <algorithm>

namespace hms::validation {

bool valid_text(const std::string& value, std::size_t max_length)
{
    if (value.empty() || value.size() > max_length) return false;

    for (const unsigned char ch : value) {
        if (ch == 0 || ch < 0x20 || ch == 0x7f) return false;
    }

    return true;
}


bool valid_name_address(const std::string& value, std::size_t max_length)
{
    if (!valid_text(value, max_length)) return false;

    for (const unsigned char ch : value) {
        const bool allowed =
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == ' ' || ch == '.' || ch == ',' ||
            ch == '\'' || ch == '-' || ch == '/';

        if (!allowed) return false;
    }

    return true;
}

bool valid_identifier(const std::string& value,
                      const std::string& prefix,
                      std::size_t digits)
{
    if (value.size() != prefix.size() + digits ||
        value.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    return std::all_of(
        value.begin() + static_cast<std::string::difference_type>(
                            prefix.size()),
        value.end(),
        [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        });
}

bool valid_username(const std::string& value)
{
    if (value.empty() || value.size() > 64) return false;

    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= 'A' && ch <= 'Z') ||
               (ch >= 'a' && ch <= 'z') ||
               (ch >= '0' && ch <= '9') ||
               ch == '_' || ch == '.' || ch == '-';
    });
}

bool valid_phone(const std::string& value)
{
    return value.size() == 10 &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        });
}

bool valid_positive_integer(const std::string& value)
{
    if (value.empty() || value.size() > 18) return false;

    if (!std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        })) {
        return false;
    }

    return value.find_first_not_of('0') != std::string::npos;
}

}  // namespace hms::validation
