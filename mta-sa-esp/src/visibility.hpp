#pragma once
#include "game.hpp"

namespace esp {
using LineClear=bool(__cdecl*)(const Vec3&,const Vec3&,bool,bool,bool,bool,bool,bool,bool);
struct VisibilityContext { Vec3 camera; LineClear trace{}; };
VisibilityContext prepare_visibility(std::uintptr_t base,const Settings& settings);
Visibility trace_visibility(const VisibilityContext& context,const Entity& entity,const Settings& settings);
Visibility trace_visibility(std::uintptr_t base,const Entity& entity,const Settings& settings);
bool matches_drawing_type(Visibility visibility,DrawingType type) noexcept;
std::uint32_t entity_color(const ColorPair& colors,Visibility visibility) noexcept;
}
