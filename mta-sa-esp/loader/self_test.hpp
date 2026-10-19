#pragma once
#include "process_access.hpp"
#include "diagnostics.hpp"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace self_test {
inline int failure(const wchar_t* step) {
    std::fwprintf(stderr,L"AUTOTESTE FALHOU: %ls (erro Windows %lu).\n",step,GetLastError());
    return 1;
}
inline int child(const wchar_t* event_name) {
    if (std::wstring(event_name).rfind(L"Local\\MtaEspAllocationTest_",0)!=0) return 2;
    esp::Handle done(OpenEventW(SYNCHRONIZE,FALSE,event_name));
    if (!done.valid()) return 2;
    const auto wait=WaitForSingleObject(done.get(),15000);
    return wait==WAIT_OBJECT_0?0:1;
}
inline int run() {
    std::wprintf(L"Autoteste isolado: usa apenas um processo auxiliar criado por este carregador.\n");
    const auto event_name=L"Local\\MtaEspAllocationTest_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetTickCount64());
    esp::Handle done(CreateEventW(nullptr,TRUE,FALSE,event_name.c_str()));
    if (!done.valid()) return failure(L"CreateEventW");
    if (GetLastError()==ERROR_ALREADY_EXISTS) { std::fwprintf(stderr,L"Evento de teste ja existente; abortando.\n"); return 1; }
    wchar_t path[32768]{};
    const DWORD length=GetModuleFileNameW(nullptr,path,32768);
    if (!length || length>=32768) return failure(L"GetModuleFileNameW");
    const auto command_text=L"\""+std::wstring(path)+L"\" --allocation-test-child "+event_name;
    std::vector<wchar_t> command(command_text.begin(),command_text.end());
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb=sizeof(startup);
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(path,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&info)) return failure(L"CreateProcessW");
    esp::Handle process(info.hProcess),thread(info.hThread);
    struct FinishChild {
        HANDLE process,done;
        ~FinishChild() { SetEvent(done); WaitForSingleObject(process,5000); }
    } finish{process.get(),done.get()};
    esp::Handle reopened(OpenProcess(loader::process_access,FALSE,info.dwProcessId));
    if (!reopened.valid()) return failure(L"OpenProcess com a mesma mascara do carregador");
    const auto access=diagnostics::print_handle_access(reopened.get(),loader::process_access);
    if (!access.known || access.missing) { std::fwprintf(stderr,L"AUTOTESTE FALHOU: nao foi possivel confirmar as permissoes do handle.\n"); return 1; }
    std::wprintf(L"PASS OpenProcess com a mesma mascara de acesso.\n");
    struct RemoteBuffer {
        HANDLE process;
        void* memory;
        ~RemoteBuffer() { if (memory) VirtualFreeEx(process,memory,0,MEM_RELEASE); }
    } buffer{reopened.get(),VirtualAllocEx(reopened.get(),nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    if (!buffer.memory) return failure(L"VirtualAllocEx PAGE_READWRITE");
    std::wprintf(L"PASS VirtualAllocEx em processo auxiliar.\n");
    const std::array<unsigned char,16> sent={0x4d,0x54,0x41,0x20,0x45,0x53,0x50,0x20,0x54,0x45,0x53,0x54,0x01,0x02,0x03,0x04};
    std::array<unsigned char,16> received{};
    SIZE_T transferred{};
    if (!WriteProcessMemory(reopened.get(),buffer.memory,sent.data(),sent.size(),&transferred) || transferred!=sent.size()) return failure(L"WriteProcessMemory de dados de teste");
    if (!ReadProcessMemory(reopened.get(),buffer.memory,received.data(),received.size(),&transferred) || transferred!=received.size()) return failure(L"ReadProcessMemory de dados de teste");
    if (sent!=received) { std::fwprintf(stderr,L"AUTOTESTE FALHOU: dados lidos diferem dos gravados.\n"); return 1; }
    std::wprintf(L"PASS escrita e leitura de dados sem executar codigo remoto.\n");
    if (!VirtualFreeEx(reopened.get(),buffer.memory,0,MEM_RELEASE)) return failure(L"VirtualFreeEx");
    buffer.memory=nullptr;
    std::wprintf(L"PASS liberacao da memoria de teste.\n");
    if (!SetEvent(done.get())) return failure(L"SetEvent de encerramento");
    const DWORD wait=WaitForSingleObject(process.get(),5000);
    if (wait!=WAIT_OBJECT_0) { std::fwprintf(stderr,L"AUTOTESTE FALHOU: auxiliar nao confirmou encerramento.\n"); return 1; }
    DWORD code{};
    if (!GetExitCodeProcess(process.get(),&code)) return failure(L"GetExitCodeProcess");
    if (code) { std::fwprintf(stderr,L"AUTOTESTE FALHOU: auxiliar retornou %lu.\n",code); return 1; }
    std::wprintf(L"AUTOTESTE OK. Nenhum processo do GTA/MTA foi aberto ou modificado por este modo.\n");
    return 0;
}
}
