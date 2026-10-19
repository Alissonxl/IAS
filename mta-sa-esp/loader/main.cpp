#include "win.hpp"
#include "diagnostics.hpp"
#include "self_test.hpp"
#include "game_start.hpp"
#include "hook_transport.hpp"
#include <tlhelp32.h>
#include <bit>
#include <cerrno>
#include <cstdio>
#include <cwchar>
#include <vector>

namespace {
DWORD single_game() {
    const auto found=game_start::scan();
    if (found.error) std::fwprintf(stderr,L"Falha ao listar processos (erro %lu).\n",found.error);
    if (found.multiple) std::fwprintf(stderr,L"Mais de um gta_sa.exe aberto. Informe o PID explicitamente.\n");
    return found.error?0:found.pid;
}
bool dll_x86(const wchar_t* path) {
    esp::Handle file(CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    DWORD count{};
    if (!file.valid() || !ReadFile(file.get(),&dos,sizeof(dos),&count,nullptr) || count!=sizeof(dos) ||
        dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0 || dos.e_lfanew>0x100000) return false;
    LARGE_INTEGER offset{};
    offset.QuadPart=dos.e_lfanew;
    return SetFilePointerEx(file.get(),offset,nullptr,FILE_BEGIN) &&
           ReadFile(file.get(),&nt,sizeof(nt),&count,nullptr) && count==sizeof(nt) && nt.Signature==IMAGE_NT_SIGNATURE &&
           nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 && nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
           (nt.FileHeader.Characteristics&IMAGE_FILE_DLL);
}
std::uintptr_t remote_load_library(DWORD pid) {
    const HMODULE kernel=GetModuleHandleW(L"kernel32.dll");
    if (!kernel) return 0;
    const FARPROC function=GetProcAddress(kernel,"LoadLibraryW");
    HMODULE owner{};
    if (!function || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                        std::bit_cast<LPCWSTR>(function),&owner)) return 0;
    wchar_t path[32768]{};
    const DWORD length=GetModuleFileNameW(owner,path,32768);
    if (!length || length>=32768) return 0;
    const wchar_t* basename=std::wcsrchr(path,L'\\');
    basename=basename?basename+1:path;
    const auto offset=std::bit_cast<std::uintptr_t>(function)-reinterpret_cast<std::uintptr_t>(owner);
    esp::Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid));
    MODULEENTRY32W entry{};
    entry.dwSize=sizeof(entry);
    if (!snapshot.valid() || !Module32FirstW(snapshot.get(),&entry)) return 0;
    do {
        if (_wcsicmp(entry.szModule,basename)==0 && offset<entry.modBaseSize)
            return reinterpret_cast<std::uintptr_t>(entry.modBaseAddr)+offset;
    } while (Module32NextW(snapshot.get(),&entry));
    return 0;
}
int failure(const wchar_t* operation,DWORD pid=0) {
    const DWORD error=GetLastError();
    std::fwprintf(stderr,L"%ls falhou. Erro Windows: %lu\n",operation,error);
    wchar_t* message{};
    if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr,error,0,reinterpret_cast<LPWSTR>(&message),0,nullptr)) {
        std::fwprintf(stderr,L"%ls\n",message);
        LocalFree(message);
    }
    if (error==ERROR_ACCESS_DENIED && pid) {
        std::wprintf(L"Diagnostico de acesso (somente consulta):\n");
        diagnostics::print(pid);
        std::wprintf(L"Acesso negado nao e erro de offsets. A causa especifica nao pode ser concluida apenas pelo codigo 5.\n");
    }
    return 1;
}
}
int wmain(int argc,wchar_t** argv) {
    if (argc==3 && std::wcscmp(argv[1],L"--verificar-inicializacao")==0) {
        game_start::Settings config{};
        if (!game_start::settings(argv[2],config)) return 2;
        std::wprintf(L"Inicializador configurado: %ls\nEspera: %lu ms; intervalo: %lu ms; estabilizacao: %lu ms.\n",
                     config.path.c_str(),config.timeout_ms,config.poll_ms,config.ready_delay_ms);
        std::wprintf(L"Configuracao verificada. Nenhum jogo aberto ou injecao executada.\n");
        return 0;
    }
    if (argc==2 && std::wcscmp(argv[1],L"--autoteste")==0) return self_test::run();
    if (argc==3 && std::wcscmp(argv[1],L"--allocation-test-child")==0) return self_test::child(argv[2]);
    if (argc>=2 && std::wcscmp(argv[1],L"--diagnosticar")==0) {
        DWORD diagnostic_pid{};
        if (argc==2) diagnostic_pid=single_game();
        else if (argc==3) {
            wchar_t* end{};
            errno=0;
            const auto value=std::wcstoul(argv[2],&end,10);
            if (!errno && end!=argv[2] && !*end && argv[2][0]!=L'-') diagnostic_pid=value;
        }
        if (!diagnostic_pid) { std::fwprintf(stderr,L"Abra uma unica instancia do GTA/MTA ou informe --diagnosticar PID.\n"); return 2; }
        const bool queried=diagnostics::print(diagnostic_pid);
        std::wprintf(L"Modo de consulta: nenhuma alocacao, escrita ou thread remota foi executada.\n");
        return queried?0:1;
    }
    const bool hook_mode=argc>=2 && std::wcscmp(argv[1],L"--hook")==0;
    if (argc!=3 && !(hook_mode && argc==4)) {
        std::fwprintf(stderr,L"Uso: mta-sa-loader.exe --hook [PID] caminho-da-DLL\nMetodo anterior: --auto|PID caminho-da-DLL\n"); return 2;
    }
    wchar_t dll_path[32768]{};
    const DWORD path_length=GetFullPathNameW(argc==4?argv[3]:argv[2],32768,dll_path,nullptr);
    if (!path_length || path_length>=32768 || !dll_x86(dll_path)) {
        std::fwprintf(stderr,L"DLL ausente, invalida ou diferente de x86.\n"); return 2;
    }
    std::wstring ini_path=dll_path;
    const auto dot=ini_path.find_last_of(L'.'),slash=ini_path.find_last_of(L"\\/");
    if (dot!=std::wstring::npos && (slash==std::wstring::npos || dot>slash)) ini_path.replace(dot,std::wstring::npos,L".ini");
    else ini_path+=L".ini";
    if (GetFileAttributesW(ini_path.c_str())==INVALID_FILE_ATTRIBUTES) {
        std::fwprintf(stderr,L"INI da DLL nao encontrado: %ls\n",ini_path.c_str()); return 2;
    }
    DWORD pid{};
    if ((hook_mode && argc==3) || std::wcscmp(argv[1],L"--auto")==0) {
        if (!game_start::start_or_get(ini_path.c_str(),pid)) return 2;
    }
    else {
        wchar_t* end{};
        errno=0;
        const wchar_t* argument=hook_mode?argv[2]:argv[1];
        const unsigned long value=std::wcstoul(argument,&end,10);
        if (errno || end==argument || *end || argument[0]==L'-') return 2;
        pid=value;
    }
    if (!pid) { std::fwprintf(stderr,L"Nenhum processo unico selecionado. Abra o GTA/MTA ou informe o PID.\n"); return 2; }
    const DWORD desired=hook_mode?PROCESS_QUERY_LIMITED_INFORMATION:loader::process_access;
    esp::Handle process(OpenProcess(desired,FALSE,pid));
    if (!process.valid()) return failure(L"OpenProcess",pid);
    wchar_t process_path[32768]{};
    DWORD process_path_size=32768;
    if (!QueryFullProcessImageNameW(process.get(),0,process_path,&process_path_size)) return failure(L"QueryFullProcessImageName");
    const wchar_t* basename=std::wcsrchr(process_path,L'\\');
    if (!basename || _wcsicmp(basename+1,L"gta_sa.exe")!=0) { std::fwprintf(stderr,L"O PID selecionado nao e gta_sa.exe.\n"); return 2; }
    if (hook_mode) {
        hook_transport::Config settings{};
        if (!hook_transport::config(ini_path.c_str(),settings)) return 2;
        return hook_transport::load(pid,dll_path,settings);
    }
    BOOL self_wow64{},target_wow64{};
    if (!IsWow64Process(GetCurrentProcess(),&self_wow64) || !IsWow64Process(process.get(),&target_wow64)) return failure(L"IsWow64Process");
    if (self_wow64!=target_wow64) { std::fwprintf(stderr,L"Arquitetura do processo diferente do carregador x86.\n"); return 2; }
    const auto access=diagnostics::print_handle_access(process.get(),loader::process_access);
    if (access.known && access.missing) {
        std::fwprintf(stderr,L"O handle nao recebeu todas as permissoes solicitadas. Operacao encerrada antes de alocar memoria.\n");
        return 3;
    }
    const auto function=remote_load_library(pid);
    if (!function) { std::fwprintf(stderr,L"Nao foi possivel resolver LoadLibraryW no processo.\n"); return 1; }
    const SIZE_T bytes=(static_cast<SIZE_T>(path_length)+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(process.get(),nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!remote) return failure(L"VirtualAllocEx",pid);
    SIZE_T written{};
    if (!WriteProcessMemory(process.get(),remote,dll_path,bytes,&written) || written!=bytes) {
        const DWORD error=GetLastError(); VirtualFreeEx(process.get(),remote,0,MEM_RELEASE); SetLastError(error);
        return failure(L"WriteProcessMemory",pid);
    }
    esp::Handle thread(CreateRemoteThread(process.get(),nullptr,0,std::bit_cast<LPTHREAD_START_ROUTINE>(function),remote,0,nullptr));
    if (!thread.valid()) {
        const DWORD error=GetLastError(); VirtualFreeEx(process.get(),remote,0,MEM_RELEASE); SetLastError(error);
        return failure(L"CreateRemoteThread",pid);
    }
    const DWORD wait=WaitForSingleObject(thread.get(),15000);
    if (wait!=WAIT_OBJECT_0) {
        std::fwprintf(stderr,L"LoadLibraryW nao terminou em 15 segundos. O argumento remoto foi preservado; consulte o log antes de repetir.\n");
        return 1;
    }
    DWORD exit_code{};
    const BOOL queried=GetExitCodeThread(thread.get(),&exit_code);
    const DWORD query_error=queried?ERROR_SUCCESS:GetLastError();
    VirtualFreeEx(process.get(),remote,0,MEM_RELEASE);
    if (!queried) { SetLastError(query_error); return failure(L"GetExitCodeThread"); }
    if (!exit_code) { std::fwprintf(stderr,L"LoadLibraryW retornou NULL. A DLL nao foi carregada.\n"); return 1; }
    hook_transport::Module local(dll_path);
    if (!local.get()) return failure(L"LoadLibraryExW local para resolver inicializador");
    const FARPROC initializer=hook_transport::exported(local.get(),"EspInitialize","EspInitialize@4");
    if (!initializer) { std::fwprintf(stderr,L"DLL sem EspInitialize; recompile DLL e carregador juntos.\n"); return 1; }
    const auto initializer_offset=std::bit_cast<std::uintptr_t>(initializer)-reinterpret_cast<std::uintptr_t>(local.get());
    const auto remote_initializer=static_cast<std::uintptr_t>(exit_code)+initializer_offset;
    esp::Handle initialization(CreateRemoteThread(process.get(),nullptr,0,std::bit_cast<LPTHREAD_START_ROUTINE>(remote_initializer),nullptr,0,nullptr));
    if (!initialization.valid()) return failure(L"CreateRemoteThread EspInitialize",pid);
    if (WaitForSingleObject(initialization.get(),15000)!=WAIT_OBJECT_0) {
        std::fwprintf(stderr,L"EspInitialize nao confirmou termino em 15 segundos.\n"); return 1;
    }
    DWORD initialized{};
    if (!GetExitCodeThread(initialization.get(),&initialized)) return failure(L"GetExitCodeThread EspInitialize");
    if (initialized!=ERROR_SUCCESS) { std::fwprintf(stderr,L"EspInitialize retornou erro Windows %lu.\n",initialized); return 1; }
    std::wprintf(L"LoadLibraryW e EspInitialize terminaram no PID %lu. Consulte mta-sa-esp.log para o estado do ESP.\n",pid);
    return 0;
}
