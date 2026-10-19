#include "visual_options.hpp"
#include "aim_options.hpp"
#include <cwchar>

namespace esp {
bool save_visual_settings(const std::wstring& path,const Settings& settings,std::wstring& error) {
    error.clear();
    const auto slash=path.find_last_of(L"\\/");
    if (slash==std::wstring::npos) { error=L"Salvar exige caminho absoluto do INI"; return false; }
    wchar_t temporary[MAX_PATH]{};
    const auto directory=path.substr(0,slash+1);
    if (!GetTempFileNameW(directory.c_str(),L"esp",0,temporary)) {
        error=L"Criar perfil temporario: "+std::to_wstring(GetLastError()); return false;
    }
    struct Cleanup { const wchar_t* path; ~Cleanup(){ DeleteFileW(path); } } cleanup{temporary};
    if (!CopyFileW(path.c_str(),temporary,FALSE)) { error=L"Copiar INI: "+std::to_wstring(GetLastError()); return false; }
    bool ok=true;
    const auto write=[&](const wchar_t* section,const wchar_t* key,const std::wstring& value) {
        if (ok && !WritePrivateProfileStringW(section,key,value.c_str(),temporary)) {
            ok=false; error=L"Salvar chave "+std::wstring(key)+L": "+std::to_wstring(GetLastError());
        }
    };
    for (const auto& toggle:visual_toggles) write(L"visual",toggle.key,settings.*(toggle.member)?L"1":L"0");
    write(L"visual",L"max_distance",std::to_wstring(settings.max_distance));
    write(L"visual",L"drawing_type",std::to_wstring(static_cast<std::uint32_t>(settings.drawing_type)));
    write(L"visual",L"box_style",std::to_wstring(static_cast<std::uint32_t>(settings.box_style)));
    const auto color=[&](const wchar_t* key,std::uint32_t value) {
        wchar_t text[16]{};
        std::swprintf(text,16,L"0x%08lX",static_cast<unsigned long>(value));
        write(L"colors",key,text);
    };
    for (const auto& option:visual_pairs) {
        const auto& pair=settings.*(option.member);
        color((std::wstring(option.key)+L"_visible").c_str(),pair.visible);
        color((std::wstring(option.key)+L"_hidden").c_str(),pair.occluded);
    }
    color(L"health",settings.health_color);
    color(L"armor",settings.armor_color);
    write(L"aim",L"enabled",settings.aim.enabled?L"1":L"0");
    for (const auto& option:aim_toggles) write(L"aim",option.key,settings.aim.*(option.member)?L"1":L"0");
    const auto& aim=settings.aim;
    write(L"aim",L"fov_px",std::to_wstring(aim.fov));
    write(L"aim",L"smooth",std::to_wstring(aim.smooth));
    write(L"aim",L"input_gain",std::to_wstring(aim.input_gain));
    write(L"aim",L"prediction_ms",std::to_wstring(aim.prediction_ms));
    write(L"aim",L"bone",std::to_wstring(aim.bone));
    write(L"aim",L"key",std::to_wstring(aim.key));
    write(L"aim",L"mode",std::to_wstring(aim.mode));
    write(L"aim",L"switch_key",std::to_wstring(aim.switch_key));
    write(L"aim",L"target_color",std::to_wstring(aim.target_color));
    if (!ok) return false;
    // The profile API also returns zero after a successful cache flush.
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary);
    {
        Handle file(CreateFileW(temporary,GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        if (!file.valid() || !FlushFileBuffers(file.get())) { error=L"Falha ao sincronizar INI temporario"; return false; }
    }
    Settings validation{};
    if (!load_settings(temporary,validation,error)) return false;
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary);
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
    if (!MoveFileExW(temporary,path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error=L"Substituir INI: "+std::to_wstring(GetLastError()); return false;
    }
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
    return true;
}
}
