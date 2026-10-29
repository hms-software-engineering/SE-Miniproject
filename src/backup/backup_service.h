#pragma once

#include <filesystem>
#include <string>

namespace hms::backup {

// Key is exactly 32 raw bytes, supplied through protected configuration.
void encrypt_file(const std::filesystem::path& input,
                  const std::filesystem::path& output,
                  const std::string& key_hex);
void decrypt_file(const std::filesystem::path& input,
                  const std::filesystem::path& output,
                  const std::string& key_hex);
bool is_backup_due(const std::filesystem::path& directory,
                   int interval_hours);
void run_backup(const std::filesystem::path& directory, const std::string& key_hex,
                int retention_days);
void restore_backup(const std::filesystem::path& encrypted_backup,
                    const std::string& target_database,
                    const std::string& confirmation,
                    const std::filesystem::path& work_directory,
                    const std::string& key_hex);
std::string status(const std::filesystem::path& directory, int interval_hours);

} // namespace hms::backup
