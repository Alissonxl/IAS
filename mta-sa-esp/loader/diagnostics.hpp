#pragma once
#include "win.hpp"
#include "process_access.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <vector>

namespace diagnostics {
struct HandleAccess {
    DWORD granted{},missing{};
    LONG status{};
    bool known{};
};
inline HandleAccess handle_access(HANDLE handle,DWORD requested) {
    // ObjectBasicInformation is a query only. No process permissions are changed.
    struct BasicInformation {
        ULONG attributes;
        ACCESS_MASK granted;
        ULONG handles,references;
        std::array<ULONG,10> reserved;
    };
    static_assert(sizeof(BasicInformation)==56);
    using QueryObject=LONG(NTAPI*)(HANDLE,ULONG,void*,ULONG,ULONG*);
    const HMODULE ntdll=GetModuleHandleW(L"ntdll.dll");
    const FARPROC symbol=ntdll?GetProcAddress(ntdll,"NtQueryObject"):nullptr;
    HandleAccess result{};
    if (!symbol) { result.status=static_cast<LONG>(0xc0000002UL); return result; }
    const auto query=std::bit_cast<QueryObject>(symbol);
    BasicInformation information{};
    ULONG returned{};
    constexpr ULONG object_basic_information=0;
    result.status=query(handle,object_basic_information,&information,sizeof(information),&returned);
    if (result.status<0 || returned<sizeof(information)) return result;
    result.known=true;
    result.granted=information.granted;
    result.missing=requested&~result.granted;
    return result;
}
inline HandleAccess print_handle_access(HANDLE handle,DWORD requested) {
    const auto access=handle_access(handle,requested);
    if (!access.known) {
        std::wprintf(L"Permissoes efetivas indisponiveis (NTSTATUS 0x%08lX).\n",static_cast<ULONG>(access.status));
        return access;
    }
    std::wprintf(L"Handle: solicitado=0x%08lX concedido=0x%08lX ausente=0x%08lX\n",requested,access.granted,access.missing);
    if (access.missing) {
        const auto flag=[&](DWORD value,const wchar_t* name){if (access.missing&value) std::wprintf(L"  AUSENTE: %ls\n",name);};
        flag(PROCESS_CREATE_THREAD,L"PROCESS_CREATE_THREAD");
        flag(PROCESS_VM_OPERATION,L"PROCESS_VM_OPERATION (exigido por VirtualAllocEx)");
        flag(PROCESS_VM_WRITE,L"PROCESS_VM_WRITE");
        flag(PROCESS_VM_READ,L"PROCESS_VM_READ");
        flag(PROCESS_QUERY_INFORMATION,L"PROCESS_QUERY_INFORMATION");
    }
    return access;
}
struct TokenInfo {
    DWORD integrity{},integrity_error{},elevation_error{};
    bool integrity_known{},elevated{},elevation_known{};
};
inline TokenInfo token_info(HANDLE process) {
    TokenInfo result{};
    HANDLE raw{};
    if (!OpenProcessToken(process,TOKEN_QUERY,&raw)) {
        result.integrity_error=GetLastError();
        result.elevation_error=result.integrity_error;
        return result;
    }
    esp::Handle token(raw);
    TOKEN_ELEVATION elevation{};
    DWORD size{};
    if (GetTokenInformation(token.get(),TokenElevation,&elevation,sizeof(elevation),&size)) {
        result.elevation_known=true;
        result.elevated=elevation.TokenIsElevated!=0;
    } else result.elevation_error=GetLastError();
    GetTokenInformation(token.get(),TokenIntegrityLevel,nullptr,0,&size);
    const DWORD query_error=GetLastError();
    if (query_error!=ERROR_INSUFFICIENT_BUFFER || size<sizeof(TOKEN_MANDATORY_LABEL) || size>65536) {
        result.integrity_error=query_error;
        return result;
    }
    std::vector<unsigned char> buffer(size);
    if (!GetTokenInformation(token.get(),TokenIntegrityLevel,buffer.data(),size,&size)) {
        result.integrity_error=GetLastError();
        return result;
    }
    const auto* label=reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
    if (!label->Label.Sid || !IsValidSid(label->Label.Sid)) { result.integrity_error=ERROR_INVALID_SID; return result; }
    const UCHAR count=*GetSidSubAuthorityCount(label->Label.Sid);
    if (!count) { result.integrity_error=ERROR_INVALID_SID; return result; }
    result.integrity=*GetSidSubAuthority(label->Label.Sid,count-1);
    result.integrity_known=true;
    return result;
}
inline const wchar_t* integrity_name(DWORD level) {
    if (level>=SECURITY_MANDATORY_SYSTEM_RID) return L"sistema";
    if (level>=SECURITY_MANDATORY_HIGH_RID) return L"alto";
    if (level>=SECURITY_MANDATORY_MEDIUM_RID) return L"medio";
    if (level>=SECURITY_MANDATORY_LOW_RID) return L"baixo";
    return L"nao confiavel";
}
inline void print_token(const wchar_t* label,const TokenInfo& info) {
    std::wprintf(L"%ls: ",label);
    if (info.integrity_known) std::wprintf(L"integridade=%ls (0x%lX)",integrity_name(info.integrity),info.integrity);
    else std::wprintf(L"integridade indisponivel (erro %lu)",info.integrity_error);
    if (info.elevation_known) std::wprintf(L", elevado=%ls\n",info.elevated?L"sim":L"nao");
    else std::wprintf(L", elevacao indisponivel (erro %lu)\n",info.elevation_error);
}
inline bool print(DWORD pid) {
    const auto self=token_info(GetCurrentProcess());
    print_token(L"Carregador",self);
    esp::Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    if (!process.valid()) {
        std::wprintf(L"Consulta do PID %lu negada/falhou (erro %lu).\n",pid,GetLastError());
        return false;
    }
    wchar_t path[32768]{};
    DWORD size=32768;
    if (QueryFullProcessImageNameW(process.get(),0,path,&size)) std::wprintf(L"PID %lu: %ls\n",pid,path);
    else std::wprintf(L"PID %lu: caminho indisponivel (erro %lu)\n",pid,GetLastError());
    const auto target=token_info(process.get());
    print_token(L"Processo selecionado",target);
    esp::Handle operation_handle(OpenProcess(loader::process_access,FALSE,pid));
    if (operation_handle.valid()) print_handle_access(operation_handle.get(),loader::process_access);
    else std::wprintf(L"OpenProcess com permissoes de operacao recusado (erro %lu).\n",GetLastError());
    if (self.integrity_known && target.integrity_known) {
        if (self.integrity<target.integrity) {
            std::wprintf(L"O carregador esta abaixo do processo em integridade. Confira a opcao Executar como administrador do GTA/MTA.\n");
        } else {
            std::wprintf(L"Nao foi detectado nivel de integridade inferior no carregador.\n");
            std::wprintf(L"Se a operacao remota foi negada, este diagnostico nao identifica qual controle a bloqueou.\n");
        }
    }
    return target.integrity_known;
}
}
