#pragma once
#include "config.hpp"
#include <array>
namespace esp {
struct AimToggle { const wchar_t* key; const wchar_t* label; bool AimSettings::* member; };
inline constexpr std::array<AimToggle,7> aim_toggles{{
    {L"players",L"Jogadores",&AimSettings::players},
    {L"peds",L"NPCs",&AimSettings::peds},
    {L"lock",L"Manter alvo",&AimSettings::lock},
    {L"show_target",L"Destacar alvo",&AimSettings::show_target},
    {L"in_car",L"Alvos em ve\u00edculos",&AimSettings::in_car},
    {L"visible_check",L"Checar visibilidade",&AimSettings::visible_check},
    {L"show_fov",L"Mostrar FOV",&AimSettings::show_fov}
}};
}
