#include "win.hpp"
#include "hook_protocol.hpp"
#include <bit>

namespace {
HMODULE own_module{};
volatile LONG started{};
}

extern "C" __declspec(dllexport) DWORD WINAPI EspInitialize(void*) {
    HMODULE pinned{};
    if (!own_module || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                                          reinterpret_cast<LPCWSTR>(own_module),&pinned)) return GetLastError();
    if (InterlockedCompareExchange(&started,1,0)!=0) return ERROR_SUCCESS;
    const HANDLE thread=CreateThread(nullptr,0,esp::worker,own_module,0,nullptr);
    if (!thread) {
        const DWORD error=GetLastError();
        InterlockedExchange(&started,0);
        return error;
    }
    CloseHandle(thread);
    return ERROR_SUCCESS;
}

extern "C" __declspec(dllexport) LRESULT CALLBACK EspMessageHook(int code,WPARAM wparam,LPARAM lparam) {
    if (code>=0 && EspInitialize(nullptr)==ERROR_SUCCESS) esp::hook_protocol::signal_loaded();
    return CallNextHookEx(nullptr,code,wparam,lparam);
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) {
        own_module=module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
