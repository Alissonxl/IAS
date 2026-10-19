#pragma once
#include "win.hpp"
#include <cwchar>

namespace esp::hook_protocol {
inline bool event_name(DWORD pid,wchar_t (&name)[96]) noexcept {
    return std::swprintf(name,96,L"Local\\MtaEspHookLoaded_%lu",pid)>0;
}
inline void signal_loaded() noexcept {
    wchar_t name[96]{};
    if (!event_name(GetCurrentProcessId(),name)) return;
    const HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);
    if (!event) return;
    SetEvent(event);
    CloseHandle(event);
}
}
