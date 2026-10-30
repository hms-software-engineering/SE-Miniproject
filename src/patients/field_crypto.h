#pragma once
#include <string>

namespace hms::patients {

// AES-256-GCM. Output: hex(nonce || ciphertext || tag). `context` is the AAD.
// Both throw on a bad key size, malformed input, or a failed authentication tag.
std::string encrypt_patient_field(const std::string& plaintext,
                                  const std::string& key,
                                  const std::string& context);
std::string decrypt_patient_field(const std::string& encoded,
                                  const std::string& key,
                                  const std::string& context);

} // namespace hms::patients