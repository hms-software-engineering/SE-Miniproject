#include "../src/validation/validator.h"

#include <cassert>
#include <iostream>
#include <string>

int main()
{
    using namespace hms::validation;

    assert(valid_text("Daksha", 60));
    assert(!valid_text("", 60));
    assert(!valid_text(std::string("bad\ninput"), 60));
    assert(!valid_text(std::string("bad\0input", 9), 60));
    assert(!valid_text(std::string(61, 'a'), 60));

    assert(valid_name_address("O'Brien", 60));
    assert(valid_name_address("12 Main St., Apt-2/4", 120));
    assert(!valid_name_address("<script>", 60));
    assert(!valid_name_address("bad\\nname", 60));
    assert(!valid_name_address(std::string(61, 'a'), 60));

    assert(valid_identifier("PID-12345678", "PID-", 8));
    assert(!valid_identifier("PID-123", "PID-", 8));
    assert(!valid_identifier("PID-1234567x", "PID-", 8));

    assert(valid_username("daksha.sg-24"));
    assert(!valid_username("bad username"));
    assert(!valid_username("bad;name"));
    assert(!valid_username("' OR '1'='1"));
    assert(!valid_username("<script>alert(1)</script>"));

    assert(valid_phone("9876543210"));
    assert(!valid_phone("987654321"));
    assert(!valid_phone("98765abc10"));

    assert(valid_positive_integer("12"));
    assert(!valid_positive_integer("0"));
    assert(!valid_positive_integer("-1"));
    assert(!valid_positive_integer("12x"));

    std::cout << "All validator tests passed.\n";
    return 0;
}
