
#include "password_security.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using hms::security::hash_password;
    using hms::security::is_password_strong;
    using hms::security::verify_password;

    // Password strength rules
    assert(!is_password_strong("abc"));
    assert(!is_password_strong("abcdefgh"));
    assert(!is_password_strong("ABCDEFG1!"));
    assert(!is_password_strong("abcdefg1!"));
    assert(!is_password_strong("Abcdefgh!"));
    assert(!is_password_strong("Abcdefg1"));

    assert(is_password_strong("Abcdef1!"));

    // Weak passwords must not be hashed.
    std::string weak_hash = "should-be-cleared";
    assert(!hash_password("abc", weak_hash));
    assert(weak_hash.empty());

    // Strong passwords must produce encoded Argon2id hashes.
    std::string hash1;
    std::string hash2;

    assert(hash_password("Abcdef1!", hash1));
    assert(hash_password("Abcdef1!", hash2));

    assert(hash1.find("$argon2id$") == 0);
    assert(hash2.find("$argon2id$") == 0);

    // Random salts should produce different encoded hashes.
    assert(hash1 != hash2);

    // Correct password succeeds; incorrect password fails.
    assert(verify_password("Abcdef1!", hash1));
    assert(!verify_password("Wrongpass1!", hash1));
    assert(!verify_password("Abcdef1!", ""));
    assert(!verify_password("Abcdef1!", "not-a-valid-hash"));

    std::cout << "All password security tests passed.\n";
    return 0;
}
