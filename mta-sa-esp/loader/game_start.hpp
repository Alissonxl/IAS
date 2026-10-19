#pragma once
#include "win.hpp"
#include <shellapi.h>
#include <tlhelp32.h>
#include <cerrno>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

namespace game_start {
struct Scan { DWORD pid{},error{}; bool multiple{}; };
inline Scan scan() {
    Scan result{};
    esp::Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));
    if (!snapshot.valid()) { result.error=GetLastError(); return result; }
    PROCESSENTRY32W process{};
    process.dwSize=sizeof(process);
    if (!Process32FirstW(snapshot.get(),&process)) {
        result.error=GetLastError();
        if (result.error==ERROR_NO_MORE_FILES) result.error=0;
        return result;
    }
    do {
        if (_wcsicmp(process.szExeFile,L"gta_sa.exe")!=0) continue;
        if (result.pid) { result.pid=0; result.multiple=true; return result; }
        result.pid=process.th32ProcessID;
    } while (Process32NextW(snapshot.get(),&process));
    const DWORD error=GetLastError();
    if (error!=ERROR_NO_MORE_FILES) result.error=error;
    return result;
}
struct Settings { std::wstring path; DWORD timeout_ms{},poll_ms{},ready_delay_ms{}; };
inline bool integer(const wchar_t* ini,const wchar_t* key,DWORD low,DWORD high,DWORD& output) {
    wchar_t value[64]{};
    if (!GetPrivateProfileStringW(L"launcher",key,L"",value,64,ini)) {
        std::fwprintf(stderr,L"INI: falta [launcher] %ls.\n",key); return false;
    }
    wchar_t* end{};
    errno=0;
    const auto number=std::wcstoull(value,&end,10);
    if (errno || end==value || *end || value[0]==L'-' || number<low || number>high) {
        std::fwprintf(stderr,L"INI: [launcher] %ls fora do limite.\n",key); return false;
    }
    output=static_cast<DWORD>(number);
    return true;
}
inline bool settings(const wchar_t* ini,Settings& result) {
    result={};
    std::vector<wchar_t> buffer(32768);
    const DWORD size=GetPrivateProfileStringW(L"launcher",L"path",L"",buffer.data(),static_cast<DWORD>(buffer.size()),ini);
    if (!size || size>=buffer.size()-1) { std::fwprintf(stderr,L"INI: informe [launcher] path com o caminho de Multi Theft Auto.exe.\n"); return false; }
    result.path.assign(buffer.data(),size);
    const bool drive_rooted=result.path.size()>=4 && std::iswalpha(result.path[0]) && result.path[1]==L':' &&
                            (result.path[2]==L'\\' || result.path[2]==L'/');
    if (!drive_rooted && result.path.rfind(L"\\\\",0)!=0) {
        std::fwprintf(stderr,L"INI: [launcher] path deve ser absoluto.\n"); return false;
    }
    const auto slash=result.path.find_last_of(L"\\/");
    if (slash==std::wstring::npos || _wcsicmp(result.path.c_str()+slash+1,L"Multi Theft Auto.exe")!=0) {
        std::fwprintf(stderr,L"INI: selecione Multi Theft Auto.exe, o inicializador do MTA.\n"); return false;
    }
    const DWORD attributes=GetFileAttributesW(result.path.c_str());
    if (attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_DIRECTORY)) {
        std::fwprintf(stderr,L"Inicializador do MTA nao encontrado: %ls\n",result.path.c_str()); return false;
    }
    return integer(ini,L"timeout_ms",1000,300000,result.timeout_ms) &&
           integer(ini,L"poll_ms",50,2000,result.poll_ms) &&
           integer(ini,L"ready_delay_ms",1000,30000,result.ready_delay_ms);
}
inline bool start_or_get(const wchar_t* ini,DWORD& pid) {
    pid=0;
    auto found=scan();
    if (found.error) { std::fwprintf(stderr,L"Falha ao listar processos (erro %lu).\n",found.error); return false; }
    if (found.multiple) { std::fwprintf(stderr,L"Mais de um gta_sa.exe aberto. Feche as instancias extras ou informe o PID.\n"); return false; }
    if (found.pid) { pid=found.pid; std::wprintf(L"Jogo ja aberto, PID %lu.\n",pid); return true; }
    Settings config{};
    if (!settings(ini,config)) return false;
    const auto directory=config.path.substr(0,config.path.find_last_of(L"\\/"));
    SHELLEXECUTEINFOW execute{};
    execute.cbSize=sizeof(execute);
    execute.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    execute.lpVerb=L"open";
    execute.lpFile=config.path.c_str();
    execute.lpDirectory=directory.c_str();
    execute.nShow=SW_SHOWNORMAL;
    std::wprintf(L"Abrindo MTA: %ls\n",config.path.c_str());
    std::fflush(stdout);
    if (!ShellExecuteExW(&execute)) { std::fwprintf(stderr,L"Falha ao abrir MTA (erro %lu).\n",GetLastError()); return false; }
    esp::Handle launched(execute.hProcess);
    const auto deadline=GetTickCount64()+config.timeout_ms;
    std::wprintf(L"Aguardando gta_sa.exe por ate %lu segundos...\n",config.timeout_ms/1000);
    while (GetTickCount64()<deadline) {
        found=scan();
        if (found.error || found.multiple) {
            std::fwprintf(stderr,L"Selecao do jogo interrompida: erro %lu, instancias multiplas=%ls.\n",found.error,found.multiple?L"sim":L"nao");
            return false;
        }
        if (found.pid) {
            const DWORD selected=found.pid;
            std::wprintf(L"GTA iniciado, PID %lu. Aguardando %lu ms de inicializacao...\n",selected,config.ready_delay_ms);
            Sleep(config.ready_delay_ms);
            found=scan();
            if (found.error || found.multiple || found.pid!=selected) {
                std::fwprintf(stderr,L"O processo do jogo mudou ou encerrou durante a inicializacao.\n"); return false;
            }
            pid=selected;
            return true;
        }
        Sleep(config.poll_ms);
    }
    std::fwprintf(stderr,L"Tempo esgotado: MTA foi aberto, mas nenhum gta_sa.exe foi encontrado. Confira a janela do MTA.\n");
    return false;
}
}
