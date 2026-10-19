#include "visibility.hpp"
#include "memory.hpp"
#include <bit>

namespace esp {
bool matches_drawing_type(Visibility visibility,DrawingType type) noexcept {
    return type==DrawingType::All || (type==DrawingType::Visible && visibility==Visibility::Visible) ||
           (type==DrawingType::Occluded && visibility==Visibility::Occluded);
}
std::uint32_t entity_color(const ColorPair& colors,Visibility visibility) noexcept {
    if (visibility==Visibility::Visible) return colors.visible;
    if (visibility==Visibility::Occluded) return colors.occluded;
    return 0xff9ca3af;
}
VisibilityContext prepare_visibility(std::uintptr_t base,const Settings& settings) {
    const auto& v=settings.visibility;
    Vec3 camera{};
    if (!v.camera || !v.line_clear || !read(base+v.camera,camera) || !finite(camera) ||
        std::abs(camera.x)>100000 || std::abs(camera.y)>100000 || std::abs(camera.z)>100000 ||
        !executable(base+v.line_clear)) return {};
    return {camera,std::bit_cast<LineClear>(base+v.line_clear)};
}
Visibility trace_visibility(const VisibilityContext& context,const Entity& entity,const Settings& settings) {
    if (!entity.streamed || !context.trace || !finite(entity.position)) return Visibility::Unknown;
    const auto& v=settings.visibility;
    Vec3 chest=entity.position,head=entity.position;
    chest.z+=(settings.head_offset+settings.foot_offset)*.5f;
    head.z+=settings.head_offset;
    // Peds are excluded so the tested target and the local character cannot block their own rays.
    const auto clear=[&](const Vec3& target) {
        return context.trace(context.camera,target,v.buildings,v.vehicles,false,v.objects,true,true,false);
    };
    return clear(chest)||clear(head)?Visibility::Visible:Visibility::Occluded;
}
Visibility trace_visibility(std::uintptr_t base,const Entity& entity,const Settings& settings) {
    return trace_visibility(prepare_visibility(base,settings),entity,settings);
}
}
