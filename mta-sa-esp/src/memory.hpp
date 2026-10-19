#pragma once
#include "win.hpp"
#include <cstring>
#include <limits>
#include <type_traits>

namespace esp {
inline bool read_bytes(std::uintptr_t address, void* destination, std::size_t size) noexcept {
    if (!destination || size == 0) return false;
    std::memset(destination, 0, size);
    if (!address || size > (std::numeric_limits<std::uintptr_t>::max)() - address) return false;
    SIZE_T copied{};
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                           destination, size, &copied) || copied != size) {
        std::memset(destination, 0, size);
        return false;
    }
    return true;
}
template<class T> bool read(std::uintptr_t address, T& result) noexcept {
    static_assert(std::is_trivially_copyable_v<T>);
    return read_bytes(address, &result, sizeof(result));
}
inline bool executable(std::uintptr_t address) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const DWORD protection = info.Protect & 0xffU;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}
}
