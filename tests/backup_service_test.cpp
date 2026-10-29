#include "backup/backup_service.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <cstdlib>
#include <chrono>
#include <sstream>

#define CHECK(x) do { if(!(x)){std::cerr<<"FAILED: " #x " (line "<<__LINE__<<")\n";std::exit(1);} } while(false)

int main() {
    namespace fs=std::filesystem;
    auto dir=fs::temp_directory_path()/("hms-nf006-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;fs::create_directories(dir);
    const std::string key="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    const std::string wrong="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    auto source=dir/"source.dump", encrypted=dir/"archive.hmsbak", encrypted2=dir/"archive2.hmsbak", restored=dir/"restored.dump";
    const std::string sample="PostgreSQL custom archive test data with recognizable plaintext";
    {std::ofstream f(source,std::ios::binary);f<<sample;}
    hms::backup::encrypt_file(source,encrypted,key);
    hms::backup::encrypt_file(source,encrypted2,key);
    std::ifstream ef(encrypted,std::ios::binary),sf(source,std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(ef)),{}),plain((std::istreambuf_iterator<char>(sf)),{});
    CHECK(bytes.find(sample)==std::string::npos);
    CHECK(bytes!=plain);
    std::ifstream ef2(encrypted2,std::ios::binary);std::string bytes2((std::istreambuf_iterator<char>(ef2)),{});CHECK(bytes!=bytes2);
    hms::backup::decrypt_file(encrypted,restored,key);
    std::ifstream rf(restored,std::ios::binary);CHECK(std::string((std::istreambuf_iterator<char>(rf)),{})==sample);

    auto reject=[&](const fs::path& input,const std::string& candidate_key){bool failed=false;try{hms::backup::decrypt_file(input,dir/"must-not-exist.dump",candidate_key);}catch(...){failed=true;}CHECK(failed);CHECK(!fs::exists(dir/"must-not-exist.dump"));};
    reject(encrypted,wrong);
    auto corrupt=dir/"corrupt.hmsbak";bytes[bytes.size()/2]^=1;{std::ofstream f(corrupt,std::ios::binary);f.write(bytes.data(),bytes.size());}reject(corrupt,key);
    auto truncated=dir/"truncated.hmsbak";{std::ofstream f(truncated,std::ios::binary);f.write(bytes.data(),10);}reject(truncated,key);
    bool bad_key=false;try{hms::backup::encrypt_file(source,dir/"bad", "short");}catch(...){bad_key=true;}CHECK(bad_key);
    CHECK(hms::backup::is_backup_due(dir,24));
    {std::ofstream f(dir/"backup-last-success.txt");f<<"SUCCESS "<<std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())<<" hms-recent.hmsbak\n";}
    CHECK(!hms::backup::is_backup_due(dir,24));
    {std::ofstream f(dir/"backup-status.txt");f<<"FAILURE 1\n";}
    CHECK(!hms::backup::is_backup_due(dir,24)); // A failure must not erase the last successful run.
    {std::ofstream f(dir/"backup-last-success.txt");f<<"SUCCESS "<<std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()-std::chrono::hours(25))<<" hms-old.hmsbak\n";}
    CHECK(hms::backup::is_backup_due(dir,24));
    bool bad_interval=false;try{(void)hms::backup::is_backup_due(dir,0);}catch(...){bad_interval=true;}CHECK(bad_interval);

    const char* postgres_test = std::getenv("HMS_BACKUP_TEST_POSTGRES");
    if (postgres_test != nullptr && std::string(postgres_test) == "1") {
        const char* restore_database =
            std::getenv("HMS_BACKUP_TEST_RESTORE_DATABASE");
        CHECK(restore_database != nullptr && *restore_database != '\0');

        const auto backup_dir = dir / "service-run";
        fs::create_directories(backup_dir);
        const auto expired = backup_dir / "expired.hmsbak";
        { std::ofstream f(expired, std::ios::binary); f << "expired"; }
        fs::last_write_time(
            expired, fs::file_time_type::clock::now() - std::chrono::hours(24 * 31));

        hms::backup::run_backup(backup_dir, key, 30);
        CHECK(!fs::exists(expired));
        CHECK(!hms::backup::is_backup_due(backup_dir, 24));
        CHECK(hms::backup::status(backup_dir, 24).find("backup_due=false") !=
              std::string::npos);

        fs::path artifact;
        for (const auto& entry : fs::directory_iterator(backup_dir)) {
            if (entry.path().extension() == ".hmsbak") artifact = entry.path();
        }
        CHECK(!artifact.empty());

        const auto restore_work = dir / "restore-work";
        const auto restore_status = [&restore_work] {
            std::ifstream status_file(restore_work / "restore-status.txt");
            return std::string(std::istreambuf_iterator<char>(status_file),
                               std::istreambuf_iterator<char>());
        };
        hms::backup::restore_backup(
            artifact, restore_database, restore_database, restore_work, key);
        CHECK(restore_status().find("RESTORE_SUCCESS") != std::string::npos);

        bool nonempty_target_rejected = false;
        try {
            hms::backup::restore_backup(
                artifact, restore_database, restore_database, restore_work, key);
        } catch (const std::runtime_error&) {
            nonempty_target_rejected = true;
        }
        CHECK(nonempty_target_rejected);
        CHECK(restore_status().find("RESTORE_FAILURE") != std::string::npos);

        const char* previous_database = std::getenv("PGDATABASE");
        const bool had_previous_database = previous_database != nullptr;
        const std::string saved_database =
            had_previous_database ? previous_database : "";
#ifdef _WIN32
        CHECK(_putenv_s("PGDATABASE", "hms_database_that_does_not_exist") == 0);
#else
        CHECK(setenv("PGDATABASE", "hms_database_that_does_not_exist", 1) == 0);
#endif
        bool backup_failure_reported = false;
        try {
            hms::backup::run_backup(backup_dir, key, 30);
        } catch (const std::runtime_error&) {
            backup_failure_reported = true;
        }
#ifdef _WIN32
        if (had_previous_database) CHECK(_putenv_s("PGDATABASE", saved_database.c_str()) == 0);
        else CHECK(_putenv_s("PGDATABASE", "") == 0);
#else
        if (had_previous_database) CHECK(setenv("PGDATABASE", saved_database.c_str(), 1) == 0);
        else CHECK(unsetenv("PGDATABASE") == 0);
#endif
        CHECK(backup_failure_reported);
        CHECK(hms::backup::status(backup_dir, 24).find("current=FAILURE") !=
              std::string::npos);
        CHECK(!hms::backup::is_backup_due(backup_dir, 24));
    }

    fs::remove_all(dir,ec);
    std::cout<<"Backup encryption tests passed\n";
}
