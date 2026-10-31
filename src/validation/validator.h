
#ifndef HMS_VALIDATION_VALIDATOR_H
#define HMS_VALIDATION_VALIDATOR_H

#include <string>
#include <cstddef>

namespace hms::validation {

bool valid_text(const std::string& value, std::size_t max_length);
bool valid_name_address(const std::string& value, std::size_t max_length);

bool valid_identifier(const std::string& value,
                      const std::string& prefix,
                      std::size_t digits);

bool valid_username(const std::string& value);
bool valid_phone(const std::string& value);
bool valid_positive_integer(const std::string& value);

}  // namespace hms::validation

#endif
