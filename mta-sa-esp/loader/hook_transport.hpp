#pragma once
#include "win.hpp"
#include "hook_protocol.hpp"
#include <bit>
#include <cstdio>

namespace hook_transport {
struct Config { DWORD window_timeout_ms=30000,timeout_ms=10000,poll_ms=200; };
inline bool config(const wchar_t* ini,Config& result) {
    result.window_timeout_ms=GetPrivateProfileIntW(L"loader",L"window_timeout_ms",30000,ini);
    result.timeout_ms=GetPrivateProfileIntW(L"loader",L"hook_timeout_ms",10000,ini);
    result.poll_ms=GetPrivateProfileIntW(L"loader",L"hook_poll_ms",200,ini);
    if (result.window_timeout_ms<1000 || result.window_timeout_ms>120000 ||
        result.timeout_ms<1000 || result.timeout_ms>60000 || result.poll_ms<10 || result.poll_ms>1000) {
        std::fwprintf(stderr,L"Valores de [loader] fora dos limites.\n"); return false;
    }
    return true;
}
class Module {
public:
    explicit Module(const wchar_t* path) : value_(LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {}
    ~Module() { if (value_) FreeLibrary(value_); }
    Module(const Module&)=delete;
    Module& operator=(const Module&)=delete;
    HMODULE get() const noexcept { return value_; }
private:
    HMODULE value_;
};
inline FARPROC exported(HMODULE module,const char* name,const char* decorated) {
    FARPROC function=GetProcAddress(module,name);
    if (!function) function=GetProcAddress(module,decorated);
    return function;
}
struct Window { DWORD pid; HWND found{}; bool visible{}; };
inline BOOL CALLBACK visit_window(HWND window,LPARAM data) {
    auto& selected=*reinterpret_cast<Window*>(data);
    DWORD pid{};
    GetWindowThreadProcessId(window,&pid);
    if (pid!=selected.pid || GetWindow(window,GW_OWNER)) return TRUE;
    const bool visible=IsWindowVisible(window)!=FALSE;
    if (!selected.found || (visible && !selected.visible)) { selected.found=window; selected.visible=visible; }
    return TRUE;
}
inline HWND window_for(DWORD pid) {
    Window selected{pid};
    EnumWindows(visit_window,reinterpret_cast<LPARAM>(&selected));
    return selected.found;
}
inline int failure(const wchar_t* operation) {
    std::fwprintf(stderr,L"%ls falhou (erro Windows %lu).\n",operation,GetLastError());
    return 1;
}
inline int load(DWORD pid,const wchar_t* dll,const Config& settings) {
    esp::Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
    if (!process.valid()) return failure(L"OpenProcess para consulta/espera");
    BOOL own_wow64{},target_wow64{};
    if (!IsWow64Process(GetCurrentProcess(),&own_wow64) || !IsWow64Process(process.get(),&target_wow64)) return failure(L"IsWow64Process");
    if (own_wow64!=target_wow64) { std::fwprintf(stderr,L"O hook exige DLL, carregador e processo com a mesma arquitetura x86.\n"); return 2; }
    HWND window{};
    const auto window_deadline=GetTickCount64()+settings.window_timeout_ms;
    while (GetTickCount64()<window_deadline) {
        window=window_for(pid);
        if (window) break;
        if (WaitForSingleObject(process.get(),settings.poll_ms)==WAIT_OBJECT_0) { std::fwprintf(stderr,L"Processo encerrou antes de criar uma janela.\n"); return 1; }
    }
    if (!window) { std::fwprintf(stderr,L"Nenhuma janela do processo encontrada no prazo configurado.\n"); return 1; }
    DWORD window_pid{};
    const DWORD thread_id=GetWindowThreadProcessId(window,&window_pid);
    if (!thread_id || window_pid!=pid) { std::fwprintf(stderr,L"A janela selecionada deixou de pertencer ao processo esperado.\n"); return 1; }
    Module module(dll);
    if (!module.get()) return failure(L"Carregamento local da DLL de hook");
    const FARPROC symbol=exported(module.get(),"EspMessageHook","EspMessageHook@12");
    if (!symbol) { std::fwprintf(stderr,L"DLL nao exporta EspMessageHook. Recompile DLL e carregador juntos.\n"); return 2; }
    wchar_t name[96]{};
    if (!esp::hook_protocol::event_name(pid,name)) return 1;
    esp::Handle confirmed(CreateEventW(nullptr,TRUE,FALSE,name));
    const DWORD event_status=GetLastError();
    if (!confirmed.valid()) { SetLastError(event_status); return failure(L"CreateEventW"); }
    if (event_status==ERROR_ALREADY_EXISTS) { std::fwprintf(stderr,L"Ja existe uma tentativa de hook para esse PID.\n"); return 1; }
    const HHOOK hook=SetWindowsHookExW(WH_GETMESSAGE,std::bit_cast<HOOKPROC>(symbol),module.get(),thread_id);
    if (!hook) return failure(L"SetWindowsHookExW WH_GETMESSAGE");
    struct ReleaseHook {
        HHOOK hook;
        HMODULE module;
        ~ReleaseHook() {
            if (!UnhookWindowsHookEx(hook)) {
                std::fwprintf(stderr,L"UnhookWindowsHookEx falhou (erro %lu).\n",GetLastError());
                HMODULE pinned{};
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                                   reinterpret_cast<LPCWSTR>(module),&pinned);
            }
        }
    } release{hook,module.get()};
    std::wprintf(L"Hook de mensagens instalado apenas na thread %lu do PID %lu.\n",thread_id,pid);
    const auto deadline=GetTickCount64()+settings.timeout_ms;
    const HANDLE waits[]={confirmed.get(),process.get()};
    while (GetTickCount64()<deadline) {
        if (!PostMessageW(window,WM_NULL,0,0)) return failure(L"PostMessageW WM_NULL");
        const DWORD wait=WaitForMultipleObjects(2,waits,FALSE,settings.poll_ms);
        if (wait==WAIT_OBJECT_0) {
            std::wprintf(L"Entrada da DLL pelo hook confirmada no processo. Consulte mta-sa-esp.log para o estado do ESP.\n");
            return 0;
        }
        if (wait==WAIT_OBJECT_0+1) { std::fwprintf(stderr,L"O processo encerrou durante o carregamento por hook.\n"); return 1; }
        if (wait==WAIT_FAILED) return failure(L"WaitForMultipleObjects");
    }
    std::fwprintf(stderr,L"O Windows aceitou o hook, mas a DLL nao confirmou entrada no prazo. O carregamento nao foi comprovado.\n");
    return 1;
}
}
