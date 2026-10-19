#include "../loader/hook_transport.hpp"
#include <cstdio>
#include <string>
#include <vector>

int wmain() {
    wchar_t self[32768]{};
    const DWORD size=GetModuleFileNameW(nullptr,self,32768);
    if (!size || size>=32768) return 1;
    std::wstring folder=self;
    folder.resize(folder.find_last_of(L"\\/")+1);
    const auto host=folder+L"mta-sa-hook-host.exe",probe=folder+L"mta-sa-hook-probe.dll";
    const auto command_text=L"\""+host+L"\"";
    std::vector<wchar_t> command(command_text.begin(),command_text.end());
    command.push_back(0);
    STARTUPINFOW startup{};
    startup.cb=sizeof(startup);
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(host.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&info)) {
        std::fwprintf(stderr,L"FAIL CreateProcessW auxiliar: %lu\n",GetLastError()); return 1;
    }
    esp::Handle process(info.hProcess),thread(info.hThread);
    hook_transport::Config settings{};
    settings.window_timeout_ms=3000;
    settings.timeout_ms=5000;
    settings.poll_ms=50;
    const int result=hook_transport::load(info.dwProcessId,probe.c_str(),settings);
    if (const HWND window=hook_transport::window_for(info.dwProcessId)) PostMessageW(window,WM_CLOSE,0,0);
    const DWORD ended=WaitForSingleObject(process.get(),5000);
    if (result || ended!=WAIT_OBJECT_0) { std::fwprintf(stderr,L"FAIL transporte de hook ou encerramento do auxiliar\n"); return 1; }
    std::wprintf(L"PASS: DLL de teste executou hook na thread GUI auxiliar e confirmou entrada por evento.\n");
    std::wprintf(L"Nenhum processo GTA/MTA foi utilizado neste teste.\n");
    return 0;
}
