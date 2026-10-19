#include "win.hpp"
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <cwchar>
#include <vector>

namespace esp {
namespace {
wchar_t log_path[32768]{};
SRWLOCK log_lock=SRWLOCK_INIT;
struct HashResources {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    ~HashResources() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0); }
};
}
std::wstring module_path(HMODULE module) {
    std::vector<wchar_t> buffer(32768);
    const DWORD size=GetModuleFileNameW(module,buffer.data(),static_cast<DWORD>(buffer.size()));
    if (!size || size>=buffer.size()) return {};
    return {buffer.data(),size};
}
void set_log_path(const std::wstring& path) {
    if (path.size()<32768) std::copy(path.c_str(),path.c_str()+path.size()+1,log_path);
}
void log_line(const std::wstring& text) noexcept {
    try {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        wchar_t prefix[64]{};
        std::swprintf(prefix,64,L"[%04u-%02u-%02u %02u:%02u:%02u] ",
                      static_cast<unsigned>(now.wYear),static_cast<unsigned>(now.wMonth),static_cast<unsigned>(now.wDay),
                      static_cast<unsigned>(now.wHour),static_cast<unsigned>(now.wMinute),static_cast<unsigned>(now.wSecond));
        const auto line=std::wstring(prefix)+text+L"\r\n";
        OutputDebugStringW(line.c_str());
        const int size=WideCharToMultiByte(CP_UTF8,0,line.c_str(),static_cast<int>(line.size()),nullptr,0,nullptr,nullptr);
        if (size<=0 || !log_path[0]) return;
        std::string utf8(static_cast<std::size_t>(size),'\0');
        WideCharToMultiByte(CP_UTF8,0,line.c_str(),static_cast<int>(line.size()),utf8.data(),size,nullptr,nullptr);
        AcquireSRWLockExclusive(&log_lock);
        {
            Handle file(CreateFileW(log_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
            if (file.valid()) { DWORD written{}; WriteFile(file.get(),utf8.data(),static_cast<DWORD>(utf8.size()),&written,nullptr); }
        }
        ReleaseSRWLockExclusive(&log_lock);
    } catch (...) { OutputDebugStringW(L"MTA ESP: falha ao registrar diagnostico\n"); }
}
bool sha256_file(const std::wstring& path,std::wstring& digest) {
    digest.clear();
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    if (!file.valid()) return false;
    std::vector<UCHAR> object;
    HashResources resources;
    if (BCryptOpenAlgorithmProvider(&resources.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return false;
    DWORD size{},copied{};
    if (BCryptGetProperty(resources.algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&copied,0)<0 || !size) return false;
    object.resize(size);
    if (BCryptCreateHash(resources.algorithm,&resources.hash,object.data(),size,nullptr,0,0)<0) return false;
    std::array<UCHAR,65536> buffer{};
    for (;;) {
        DWORD read_size{};
        if (!ReadFile(file.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&read_size,nullptr)) return false;
        if (!read_size) break;
        if (BCryptHashData(resources.hash,buffer.data(),read_size,0)<0) return false;
    }
    std::array<UCHAR,32> bytes{};
    if (BCryptFinishHash(resources.hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)<0) return false;
    constexpr wchar_t hex[]=L"0123456789ABCDEF";
    digest.reserve(64);
    for (const auto byte:bytes) { digest.push_back(hex[byte>>4]); digest.push_back(hex[byte&15]); }
    return true;
}
}
