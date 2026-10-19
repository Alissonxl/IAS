#include "hook_protocol.hpp"
#include <bit>

extern "C" __declspec(dllexport) LRESULT CALLBACK EspMessageHook(int code,WPARAM wparam,LPARAM lparam) {
    if (code>=0) {
        HMODULE module{};
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                               std::bit_cast<LPCWSTR>(&EspMessageHook),&module)) esp::hook_protocol::signal_loaded();
    }
    return CallNextHookEx(nullptr,code,wparam,lparam);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
