#include "backup/backup_service.h"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::string required(const char* name) { const char* p=std::getenv(name);if(!p||!*p)throw std::runtime_error(std::string("Required environment variable is missing: ")+name);return p; }
int number(const char* name,int fallback) { const char* p=std::getenv(name);if(!p||!*p)return fallback;int n=0;auto r=std::from_chars(p,p+std::char_traits<char>::length(p),n);if(r.ec!=std::errc{}||*r.ptr!='\0'||n<1)throw std::runtime_error(std::string("Invalid positive integer in ")+name);return n; }
std::filesystem::path directory() { auto p=std::filesystem::path(required("HMS_BACKUP_DIR"));if(!p.is_absolute())throw std::runtime_error("HMS_BACKUP_DIR must be an absolute path");return p; }
void usage() { std::cerr<<"Usage: hms_backup <backup|run-due|status|restore> [encrypted-file target-db --confirm-target target-db]\nEnvironment: HMS_BACKUP_DIR, HMS_BACKUP_KEY (64 hex chars), HMS_BACKUP_INTERVAL_HOURS (default 24), HMS_BACKUP_RETENTION_DAYS (default 30), PGHOST/PGPORT/PGDATABASE/PGUSER/PGPASSWORD\n"; }
}
int main(int argc,char** argv) {
    try {
        if(argc<2){usage();return 2;}std::string command=argv[1];auto dir=directory();int interval=number("HMS_BACKUP_INTERVAL_HOURS",24);
        if(command=="status") {std::cout<<hms::backup::status(dir,interval)<<"\n";return 0;}
        if(command=="backup") {hms::backup::run_backup(dir,required("HMS_BACKUP_KEY"),number("HMS_BACKUP_RETENTION_DAYS",30));std::cout<<"Backup completed successfully\n";return 0;}
        if(command=="run-due") {if(!hms::backup::is_backup_due(dir,interval)){std::cout<<"Most recent successful backup is within the configured interval\n";return 0;}hms::backup::run_backup(dir,required("HMS_BACKUP_KEY"),number("HMS_BACKUP_RETENTION_DAYS",30));std::cout<<"Scheduled backup completed successfully\n";return 0;}
        if(command=="restore"&&argc==6&&std::string(argv[4])=="--confirm-target") {hms::backup::restore_backup(argv[2],argv[3],argv[5],dir,required("HMS_BACKUP_KEY"));std::cout<<"Restore completed; elapsed time is recorded in restore-status.txt\n";return 0;}
        usage();return 2;
    } catch(const std::exception& e) {std::cerr<<"Backup operation failed: "<<e.what()<<"\n";return 1;}
}
