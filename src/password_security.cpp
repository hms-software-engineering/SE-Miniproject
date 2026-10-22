
#include "password_security.h"

#include <argon2.h>

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "Bcrypt.lib")
#elif defined(__linux__)
#include <sys/random.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#endif

namespace hms::security {
namespace {

constexpr std::uint32_t ITERATIONS = 3;
constexpr std::uint32_t MEMORY_KIB = 65536;
constexpr std::uint32_t PARALLELISM = 1;
constexpr std::size_t SALT_LENGTH = 16;
constexpr std::size_t HASH_LENGTH = 32;
constexpr std::size_t ENCODED_LENGTH = 256;

bool generate_salt(unsigned char* salt, std::size_t length) {
#ifdef _WIN32
    if (length > static_cast<std::size_t>(ULONG_MAX)) {
        return false;
    }

    return BCryptGenRandom(
        nullptr,
        salt,
        static_cast<ULONG>(length),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    ) == 0;

#elif defined(__linux__)
    std::size_t offset = 0;

    while (offset < length) {
        const auto result = getrandom(
            salt + offset,
            length - offset,
            0
        );

        if (result <= 0) {
            return false;
        }

        offset += static_cast<std::size_t>(result);
    }

    return true;

#elif defined(__APPLE__)
    arc4random_buf(salt, length);
    return true;

#else
    (void)salt;
    (void)length;
    return false;
#endif
}

} // namespace

bool is_password_strong(const std::string& password) {
    if (password.size() < 8) {
        return false;
    }

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;
    bool has_special = false;

    for (unsigned char ch : password) {
        if (std::isupper(ch)) {
            has_upper = true;
        } else if (std::islower(ch)) {
            has_lower = true;
        } else if (std::isdigit(ch)) {
            has_digit = true;
        } else if (std::ispunct(ch) || std::isspace(ch)) {
            has_special = true;
        }
    }

    return has_upper && has_lower && has_digit && has_special;
}

bool hash_password(
    const std::string& password,
    std::string& encoded_hash
) {
    encoded_hash.clear();

    if (!is_password_strong(password)) {
        return false;
    }

    unsigned char salt[SALT_LENGTH]{};

    if (!generate_salt(salt, sizeof(salt))) {
        return false;
    }

    std::vector<char> encoded(ENCODED_LENGTH, '\0');

    const int result = argon2id_hash_encoded(
        ITERATIONS,
        MEMORY_KIB,
        PARALLELISM,
        password.data(),
        password.size(),
        salt,
        sizeof(salt),
        HASH_LENGTH,
        encoded.data(),
        encoded.size()
    );

    if (result != ARGON2_OK) {
        return false;
    }

    encoded_hash.assign(encoded.data());
    return true;
}

bool verify_password(
    const std::string& password,
    const std::string& encoded_hash
) {
    if (encoded_hash.empty()) {
        return false;
    }

    return argon2id_verify(
        encoded_hash.c_str(),
        password.data(),
        password.size()
    ) == ARGON2_OK;
}

} // namespace hms::security
