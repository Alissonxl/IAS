#pragma once
#include "config.hpp"
#include "math.hpp"
#include "memory.hpp"
#include <array>
namespace esp {
struct CrosshairAnchor { Vec2 ratio{.5f,.5f}; bool valid{},aiming{}; };
inline CrosshairAnchor read_crosshair(std::uintptr_t base,const AimSettings& aim) {
    if (!aim.crosshair_x || !aim.crosshair_y || !aim.active_camera || !aim.camera_modes ||
        !aim.camera_stride || !aim.crosshair_probe) return {};
    // Confirm the HUD's two multiplier operands before treating the profile as a screen anchor.
    std::array<unsigned char,30> code{};
    std::uint32_t x_operand{},y_operand{};
    if (!read_bytes(base+aim.crosshair_probe,code.data(),code.size()) ||
        code[0]!=0xdb || code[1]!=0x05 || code[6]!=0x8b || code[7]!=0xcf ||
        code[8]!=0xd8 || code[9]!=0x0d || code[14]!=0xd9 || code[15]!=0x5c || code[16]!=0x24 || code[17]!=0x14 ||
        code[18]!=0xdb || code[19]!=0x05 || code[24]!=0xd8 || code[25]!=0x0d) return {};
    std::memcpy(&x_operand,code.data()+10,sizeof(x_operand));
    std::memcpy(&y_operand,code.data()+26,sizeof(y_operand));
    if (x_operand!=base+aim.crosshair_x || y_operand!=base+aim.crosshair_y) return {};
    std::uint8_t active{}; std::uint16_t mode{};
    if (!read(base+aim.active_camera,active) || active>=3 ||
        !read(base+aim.camera_modes+active*aim.camera_stride,mode)) return {};
    // DrawCrossHairs uses the multipliers for these three shoulder-camera modes.
    if (mode!=53 && mode!=55 && mode!=65) return {{.5f,.5f},true,mode==7 || mode==8 || mode==16 || mode==34 || mode==45 || mode==46 || mode==51};
    Vec2 ratio{};
    if (!read(base+aim.crosshair_x,ratio.x) || !read(base+aim.crosshair_y,ratio.y) ||
        !std::isfinite(ratio.x) || !std::isfinite(ratio.y) || ratio.x<=0 || ratio.x>=1 || ratio.y<=0 || ratio.y>=1) return {};
    return {ratio,true,true};
}
}
