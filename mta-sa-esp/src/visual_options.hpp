#pragma once
#include "config.hpp"
#include <array>

namespace esp {
struct ToggleOption { const wchar_t* label; const wchar_t* key; bool Settings::* member; int y; };
inline constexpr std::array<ToggleOption,10> visual_toggles{{
    {L"Ativado",L"enabled",&Settings::visual_enabled,44},
    {L"Nome",L"names",&Settings::names,80},
    {L"Distancia",L"distance",&Settings::distance_label,116},
    {L"Caixa",L"boxes",&Settings::boxes,152},
    {L"Snapline",L"snaplines",&Settings::snaplines,224},
    {L"Esqueleto",L"skeleton",&Settings::skeleton,260},
    {L"Todas dimensoes",L"all_dimensions",&Settings::all_dimensions,296},
    {L"Barra de vida",L"health_bar",&Settings::health_bar,332},
    {L"Barra de colete",L"armor_bar",&Settings::armor_bar,368},
    {L"Valores HP / AP",L"labels",&Settings::labels,404}
}};
struct PairOption { const wchar_t* key; ColorPair Settings::* member; int y; };
inline constexpr std::array<PairOption,5> visual_pairs{{
    {L"name",&Settings::name_colors,80},{L"distance",&Settings::distance_colors,116},
    {L"box",&Settings::box_colors,152},{L"snapline",&Settings::snapline_colors,224},
    {L"skeleton",&Settings::skeleton_colors,260}
}};
}
