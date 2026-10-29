#include "backup/backup_service.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <thread>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace hms::backup {
namespace {
constexpr std::array<unsigned char, 8> magic{{'H','M','S','B','K','0','1',0}};
constexpr std::size_t nonce_size = 12, tag_size = 16, chunk_size = 64 * 1024;
using Key = std::array<unsigned char, 32>;

Key parse_key(const std::string& s) {
    if (s.size() != 64) throw std::runtime_error("HMS_BACKUP_KEY must contain 64 hexadecimal characters");
    Key key{};
    auto nibble=[](char c)->int { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; };
    for (std::size_t i=0;i<key.size();++i) { int a=nibble(s[i*2]), b=nibble(s[i*2+1]); if(a<0||b<0) throw std::runtime_error("HMS_BACKUP_KEY is not valid hexadecimal"); key[i]=static_cast<unsigned char>((a<<4)|b); }
    return key;
}
std::string env(const char* name, const std::string& fallback={}) { const char* p=std::getenv(name); return p&&*p?p:fallback; }
std::string unique_suffix() {
    std::array<unsigned char, 8> b{}; if(RAND_bytes(b.data(), static_cast<int>(b.size()))!=1) throw std::runtime_error("Unable to generate a temporary filename");
    std::ostringstream o; for(auto c:b)o<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(c); return o.str();
}
void checked_write(std::ofstream& out,const void* p,std::size_t n) { out.write(static_cast<const char*>(p),static_cast<std::streamsize>(n)); if(!out) throw std::runtime_error("Backup file write failed"); }
void checked_read(std::ifstream& in,void* p,std::size_t n) { in.read(static_cast<char*>(p),static_cast<std::streamsize>(n)); if(static_cast<std::size_t>(in.gcount())!=n) throw std::runtime_error("Encrypted backup is truncated"); }

void transform(const std::filesystem::path& input,const std::filesystem::path& output,const std::string& hex,bool encrypt) {
    const Key key=parse_key(hex);
    std::ifstream in(input,std::ios::binary); if(!in) throw std::runtime_error("Could not open backup input");
    auto temporary=output; temporary += ".partial-"+unique_suffix();
    struct Cleanup { std::filesystem::path path; bool keep=false; ~Cleanup(){if(!keep){std::error_code ec;std::filesystem::remove(path,ec);}} } cleanup{temporary};
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc); if(!out) throw std::runtime_error("Could not create backup output");
    std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
    if(!ctx) throw std::runtime_error("Unable to initialize AES-GCM");
    std::array<unsigned char,nonce_size> nonce{}; std::array<unsigned char,tag_size> tag{};
    std::array<unsigned char,chunk_size+EVP_MAX_BLOCK_LENGTH> buf{}; int n=0;
    if(encrypt) {
        if(RAND_bytes(nonce.data(),static_cast<int>(nonce.size()))!=1 || EVP_EncryptInit_ex(ctx.get(),EVP_aes_256_gcm(),nullptr,nullptr,nullptr)!=1 || EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_SET_IVLEN,static_cast<int>(nonce.size()),nullptr)!=1 || EVP_EncryptInit_ex(ctx.get(),nullptr,nullptr,key.data(),nonce.data())!=1) throw std::runtime_error("Unable to initialize AES-GCM encryption");
        checked_write(out,magic.data(),magic.size()); checked_write(out,nonce.data(),nonce.size());
        while(in) { in.read(reinterpret_cast<char*>(buf.data()),chunk_size); auto got=in.gcount(); if(got>0) { if(EVP_EncryptUpdate(ctx.get(),buf.data(),&n,buf.data(),static_cast<int>(got))!=1) throw std::runtime_error("Backup encryption failed"); checked_write(out,buf.data(),static_cast<std::size_t>(n)); } }
        if(!in.eof() || EVP_EncryptFinal_ex(ctx.get(),buf.data(),&n)!=1 || EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_GET_TAG,tag_size,tag.data())!=1) throw std::runtime_error("Backup encryption did not finish");
        checked_write(out,buf.data(),static_cast<std::size_t>(n)); checked_write(out,tag.data(),tag.size());
    } else {
        std::array<unsigned char,magic.size()> gotmagic{}; checked_read(in,gotmagic.data(),gotmagic.size()); if(gotmagic!=magic) throw std::runtime_error("Unrecognized encrypted backup format");
        checked_read(in,nonce.data(),nonce.size());
        if(EVP_DecryptInit_ex(ctx.get(),EVP_aes_256_gcm(),nullptr,nullptr,nullptr)!=1 || EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_SET_IVLEN,static_cast<int>(nonce.size()),nullptr)!=1 || EVP_DecryptInit_ex(ctx.get(),nullptr,nullptr,key.data(),nonce.data())!=1) throw std::runtime_error("Unable to initialize AES-GCM decryption");
        // Keep the final 16 bytes (the GCM tag) out of the ciphertext stream.
        std::vector<unsigned char> pending; pending.reserve(chunk_size+tag_size);
        while(in) { std::array<unsigned char,chunk_size> block{}; in.read(reinterpret_cast<char*>(block.data()),block.size()); auto got=static_cast<std::size_t>(in.gcount()); pending.insert(pending.end(),block.begin(),block.begin()+got); if(pending.size()>tag_size) { auto take=pending.size()-tag_size; if(take>chunk_size)take=chunk_size; if(EVP_DecryptUpdate(ctx.get(),buf.data(),&n,pending.data(),static_cast<int>(take))!=1) throw std::runtime_error("Encrypted backup decryption failed"); checked_write(out,buf.data(),static_cast<std::size_t>(n)); pending.erase(pending.begin(),pending.begin()+take); } }
        if(pending.size()!=tag_size) throw std::runtime_error("Encrypted backup is truncated"); std::copy(pending.begin(),pending.end(),tag.begin());
        if(EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_SET_TAG,tag_size,tag.data())!=1 || EVP_DecryptFinal_ex(ctx.get(),buf.data(),&n)!=1) throw std::runtime_error("Encrypted backup authentication failed");
        checked_write(out,buf.data(),static_cast<std::size_t>(n));
    }
    out.flush(); if(!out) throw std::runtime_error("Backup file flush failed"); out.close();
    if(!encrypt) { in.clear(); in.peek(); if(in.good()) throw std::runtime_error("Encrypted backup has trailing data"); }
    std::error_code ec;std::filesystem::rename(temporary,output,ec);if(ec)throw std::runtime_error("Could not atomically publish backup artifact");cleanup.keep=true;
}

// All arguments are passed as argv entries; no command shell sees credentials or paths.
int run_process(const std::vector<std::string>& args) {
    const auto timeout_setting=env("HMS_BACKUP_PROCESS_TIMEOUT_SECONDS","43200");char* end=nullptr;errno=0;const long timeout_seconds=std::strtol(timeout_setting.c_str(),&end,10);
    if(errno!=0||end==timeout_setting.c_str()||*end!='\0'||timeout_seconds<1||timeout_seconds>604800)throw std::runtime_error("HMS_BACKUP_PROCESS_TIMEOUT_SECONDS must be between 1 and 604800");
#ifdef _WIN32
    auto wide=[](const std::string& s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(n<=0)throw std::runtime_error("Invalid UTF-8 process argument");std::wstring w(static_cast<std::size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n);return w; };
    auto quote=[](const std::wstring& w) { std::wstring q=L"\""; unsigned slashes=0; for(wchar_t c:w){ if(c==L'\\'){++slashes;continue;} if(c==L'\"'){q.append(slashes*2+1,L'\\');q+=c;slashes=0;continue;} q.append(slashes,L'\\');slashes=0;q+=c;} q.append(slashes*2,L'\\');q+=L'\"'; return q; };
    std::wstring cmd; for(const auto& a:args){ if(!cmd.empty())cmd+=L' '; cmd+=quote(wide(a)); } STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) return -1;
    const DWORD wait=WaitForSingleObject(pi.hProcess,static_cast<DWORD>(timeout_seconds*1000));if(wait==WAIT_TIMEOUT){TerminateProcess(pi.hProcess,124);WaitForSingleObject(pi.hProcess,5000);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 124;}DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return static_cast<int>(code);
#else
    pid_t pid=fork(); if(pid==0){ std::vector<char*> av;for(const auto& a:args)av.push_back(const_cast<char*>(a.c_str()));av.push_back(nullptr);execvp(av[0],av.data());_exit(127); }
    if(pid<0)return -1;int st=0;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(timeout_seconds);for(;;){pid_t result=waitpid(pid,&st,WNOHANG);if(result==pid)return WIFEXITED(st)?WEXITSTATUS(st):-1;if(result<0&&errno!=EINTR)return -1;if(std::chrono::steady_clock::now()>=deadline){kill(pid,SIGKILL);while(waitpid(pid,&st,0)<0&&errno==EINTR){}return 124;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
#endif
}

class Lock { public: explicit Lock(const std::filesystem::path& p) {
#ifdef _WIN32
    h_=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_HIDDEN,nullptr); if(h_==INVALID_HANDLE_VALUE) throw std::runtime_error("Another backup or restore operation is active");
#else
    fd_=open(p.c_str(),O_CREAT|O_RDWR,0600);if(fd_<0||flock(fd_,LOCK_EX|LOCK_NB)!=0)throw std::runtime_error("Another backup or restore operation is active");
#endif
    } ~Lock(){
#ifdef _WIN32
    if(h_!=INVALID_HANDLE_VALUE)CloseHandle(h_);
#else
    if(fd_>=0){flock(fd_,LOCK_UN);close(fd_);}
#endif
    } private:
#ifdef _WIN32
    HANDLE h_=INVALID_HANDLE_VALUE;
#else
    int fd_=-1;
#endif
};

std::string stamp() { auto t=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm,&t);
#else
    gmtime_r(&t,&tm);
#endif
    std::ostringstream o;o<<std::put_time(&tm,"%Y%m%dT%H%M%SZ");return o.str(); }
void status_write(const std::filesystem::path& dir,const std::string& value,const std::string& filename="backup-status.txt") { auto tmp=dir/(filename+".tmp");std::ofstream f(tmp,std::ios::trunc);f<<value<<"\n";f.close();if(!f)throw std::runtime_error("Could not record operation status");std::error_code ec;std::filesystem::rename(tmp,dir/filename,ec);if(ec){std::filesystem::remove(dir/filename,ec);ec.clear();std::filesystem::rename(tmp,dir/filename,ec);} }
bool safe_identifier(const std::string& s) { if(s.empty()||s.size()>63)return false;for(unsigned char c:s)if(!(std::isalnum(c)||c=='_'||c=='-'))return false;return true; }
void protect_directory(const std::filesystem::path& dir) {
#ifndef _WIN32
    std::error_code ec;std::filesystem::permissions(dir,std::filesystem::perms::owner_all,std::filesystem::perm_options::replace,ec);if(ec)throw std::runtime_error("Could not restrict backup directory permissions");
#else
    (void)dir; // Windows ACLs are provisioned and checked by deployment operators.
#endif
}
}

void encrypt_file(const std::filesystem::path& i,const std::filesystem::path& o,const std::string& k){transform(i,o,k,true);}
void decrypt_file(const std::filesystem::path& i,const std::filesystem::path& o,const std::string& k){transform(i,o,k,false);}
bool is_backup_due(const std::filesystem::path& d,int hours){if(hours<1)throw std::runtime_error("Backup interval must be positive");std::ifstream f(d/"backup-last-success.txt");std::string line;std::getline(f,line);if(line.rfind("SUCCESS ",0)!=0)return true;std::time_t t=0;std::istringstream(line.substr(8))>>t;if(!f||t<=0)return true;return std::chrono::system_clock::now()-std::chrono::system_clock::from_time_t(t)>=std::chrono::hours(hours);}
std::string status(const std::filesystem::path& d,int interval){std::ifstream current(d/"backup-status.txt"),last(d/"backup-last-success.txt");std::string s,l;std::getline(current,s);std::getline(last,l);if(s.empty())s="NEVER";if(l.empty())l="none";return "current="+s+"\nlast_success="+l+"\nbackup_due="+(is_backup_due(d,interval)?"true":"false");}
void run_backup(const std::filesystem::path& dir,const std::string& key,int retention) {
    parse_key(key);if(retention<1)throw std::runtime_error("Retention days must be positive");std::filesystem::create_directories(dir);protect_directory(dir);Lock lock(dir/".hms-backup.lock");std::error_code cleanup_ec;for(const auto& e:std::filesystem::directory_iterator(dir)){auto n=e.path().filename().string();if(n.rfind(".partial-",0)==0||n.rfind(".restore-",0)==0)std::filesystem::remove(e.path(),cleanup_ec);}auto plain=dir/(".partial-"+unique_suffix()+".dump");auto encrypted_tmp=dir/(".partial-"+unique_suffix()+".hmsbak");auto final=dir/("hms-"+stamp()+"-"+unique_suffix()+".hmsbak");
    auto verified=dir/(".partial-"+unique_suffix()+".verify.dump");
    try { if(run_process({"pg_dump","--no-password","--format=custom","--file",plain.string()})!=0)throw std::runtime_error("pg_dump failed; check PostgreSQL availability and configuration");if(!std::filesystem::exists(plain)||std::filesystem::file_size(plain)==0)throw std::runtime_error("pg_dump produced an empty artifact");if(run_process({"pg_restore","--list",plain.string()})!=0)throw std::runtime_error("pg_restore could not validate the dump archive");encrypt_file(plain,encrypted_tmp,key);decrypt_file(encrypted_tmp,verified,key);if(run_process({"pg_restore","--list",verified.string()})!=0)throw std::runtime_error("Encrypted backup failed authenticated archive validation");std::filesystem::rename(encrypted_tmp,final);std::error_code ec;std::filesystem::remove(plain,ec);std::filesystem::remove(verified,ec);auto success="SUCCESS "+std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))+" "+final.filename().string();status_write(dir,success,"backup-last-success.txt");status_write(dir,success);
      for(const auto& e:std::filesystem::directory_iterator(dir)){if(!e.is_regular_file()||e.path().extension()!=".hmsbak")continue;auto age=std::filesystem::file_time_type::clock::now()-e.last_write_time();if(age>std::chrono::hours(24LL*retention))std::filesystem::remove(e.path(),ec);}
    } catch(...) {std::error_code ec;std::filesystem::remove(plain,ec);std::filesystem::remove(encrypted_tmp,ec);std::filesystem::remove(verified,ec);try{status_write(dir,"FAILURE "+std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())));}catch(...){}throw;}
}
void restore_backup(const std::filesystem::path& backup,const std::string& target,const std::string& confirm,const std::filesystem::path& work,const std::string& key) {
    if(!safe_identifier(target)||confirm!=target)throw std::runtime_error("Restore requires a valid database identifier and --confirm-target matching it");std::filesystem::create_directories(work);protect_directory(work);Lock lock(work/".hms-backup.lock");std::error_code cleanup_ec;for(const auto& e:std::filesystem::directory_iterator(work)){if(e.path().filename().string().rfind(".restore-",0)==0)std::filesystem::remove(e.path(),cleanup_ec);}auto plain=work/(".restore-"+unique_suffix()+".dump");auto begin=std::chrono::system_clock::now();
    try {decrypt_file(backup,plain,key);if(run_process({"pg_restore","--list",plain.string()})!=0)throw std::runtime_error("Backup archive validation failed");const std::string empty_check="SELECT 1 / CASE WHEN EXISTS (SELECT 1 FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname='public' AND c.relkind IN ('r','p','v','m','S','f')) THEN 0 ELSE 1 END";if(run_process({"psql","--no-password","--set","ON_ERROR_STOP=1","--dbname","dbname="+target,"--command",empty_check})!=0)throw std::runtime_error("Restore target is unavailable or its public schema is not empty");if(run_process({"pg_restore","--no-password","--single-transaction","--exit-on-error","--no-owner","--no-privileges","--dbname","dbname="+target,plain.string()})!=0)throw std::runtime_error("Restore failed; target database remains unchanged when transaction rollback succeeds");if(run_process({"psql","--no-password","--set","ON_ERROR_STOP=1","--dbname","dbname="+target,"--command","SELECT 1"})!=0)throw std::runtime_error("Post-restore database connectivity check failed");std::error_code ec;std::filesystem::remove(plain,ec);auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-begin).count();status_write(work,"RESTORE_SUCCESS "+std::to_string(elapsed)+"s target="+target,"restore-status.txt");
    } catch(...) {std::error_code ec;std::filesystem::remove(plain,ec);try{status_write(work,"RESTORE_FAILURE","restore-status.txt");}catch(...){}throw;}
}
} // namespace hms::backup
