#include "config.hpp"
#include "aim_options.hpp"
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <stdexcept>

namespace esp {
namespace {
class Ini {
public:
    explicit Ini(const std::wstring& path) : path_(path) {
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("Arquivo INI ausente");
    }
    std::wstring text(const wchar_t* section, const wchar_t* key,const wchar_t* fallback=L"") const {
        wchar_t buffer[256]{};
        const DWORD count = GetPrivateProfileStringW(section,key,fallback,buffer,256,path_.c_str());
        if (!count || count >= 255) throw std::runtime_error("Chave INI ausente ou longa");
        return {buffer,count};
    }
    std::uint32_t integer(const wchar_t* section, const wchar_t* key, std::uint32_t low, std::uint32_t high,const wchar_t* fallback=L"") const {
        const auto value = text(section,key,fallback);
        if (value.front() == L'-') throw std::runtime_error("Inteiro negativo no INI");
        wchar_t* end{};
        errno = 0;
        const auto number = std::wcstoull(value.c_str(),&end,0);
        if (errno || end == value.c_str() || *end || number < low || number > high) throw std::runtime_error("Inteiro INI fora do limite");
        return static_cast<std::uint32_t>(number);
    }
    float real(const wchar_t* key, float low, float high,const wchar_t* fallback=L"") const {
        const auto value = text(L"visual",key,fallback);
        wchar_t* end{};
        errno = 0;
        const float number = std::wcstof(value.c_str(),&end);
        if (errno || end == value.c_str() || *end || !std::isfinite(number) || number < low || number > high) throw std::runtime_error("Float INI fora do limite");
        return number;
    }
private:
    const std::wstring& path_;
};
}
bool load_settings(const std::wstring& path, Settings& result, std::wstring& error) {
    result = {};
    error.clear();
    try {
        const Ini ini(path);
        Settings next{};
        next.sha256 = ini.text(L"build",L"sha256");
        if (next.sha256.size() != 64) throw std::runtime_error("SHA256 deve ter 64 caracteres");
        for (auto& c : next.sha256) {
            if (!std::iswxdigit(c)) throw std::runtime_error("SHA256 invalido");
            c = static_cast<wchar_t>(std::towupper(c));
        }
        auto& l = next.layout;
        l.device=ini.integer(L"layout",L"device_rva",1,0x10000000);
        l.pool=ini.integer(L"layout",L"ped_pool_rva",1,0x10000000);
        l.local=ini.integer(L"layout",L"local_ped_rva",1,0x10000000);
        l.view=ini.integer(L"layout",L"view_matrix_rva",1,0x10000000);
        l.objects=ini.integer(L"layout",L"pool_objects",0,64);
        l.flags=ini.integer(L"layout",L"pool_flags",0,64);
        l.capacity=ini.integer(L"layout",L"pool_capacity",0,64);
        l.stride=ini.integer(L"layout",L"ped_stride",0x100,0x4000);
        l.maximum=ini.integer(L"layout",L"max_slots",1,8192);
        l.matrix=ini.integer(L"layout",L"ped_matrix",0,l.stride-sizeof(std::uintptr_t));
        l.position=ini.integer(L"layout",L"ped_position",0,l.stride-12);
        l.matrix_position=ini.integer(L"layout",L"matrix_position",0,128);
        l.player_data=ini.integer(L"layout",L"ped_player_data",0,l.stride-sizeof(std::uintptr_t));
        l.health=ini.integer(L"layout",L"ped_health",0,l.stride-4);
        l.max_health=ini.integer(L"layout",L"ped_max_health",0,l.stride-4);
        l.armor=ini.integer(L"layout",L"ped_armor",0,l.stride-4,L"0");
        next.max_distance=ini.real(L"max_distance",1,3000);
        next.head_offset=ini.real(L"head_offset",-5,5);
        next.foot_offset=ini.real(L"foot_offset",-5,5);
        if (next.head_offset <= next.foot_offset) throw std::runtime_error("head_offset deve superar foot_offset");
        next.box_ratio=ini.real(L"box_ratio",0.1f,2);
        next.thickness=ini.real(L"thickness",1,8);
        next.near_clip=ini.real(L"near_clip",0.001f,10);
        next.color=ini.integer(L"visual",L"argb",0,0xffffffffU);
        next.players_only=ini.integer(L"visual",L"players_only",0,1)!=0;
        next.health_bar=ini.integer(L"visual",L"health_bar",0,1)!=0;
        next.labels=ini.integer(L"visual",L"labels",0,1)!=0;
        next.skeleton=ini.integer(L"visual",L"skeleton",0,1)!=0;
        next.names=ini.integer(L"visual",L"names",0,1)!=0;
        next.boxes=ini.integer(L"visual",L"boxes",0,1)!=0;
        next.bone_radius=ini.real(L"max_bone_distance",1,32);
        next.skeleton_thickness=ini.real(L"skeleton_thickness",1,8);
        auto& b=next.bones;
        b.clump=ini.integer(L"bones",L"ped_clump",0,l.stride-4);
        b.skin_plugin=ini.integer(L"bones",L"skin_atomic_plugin_rva",1,0x10000000);
        b.clump_list=ini.integer(L"bones",L"clump_atomic_list",0,256);
        b.atomic_link=ini.integer(L"bones",L"atomic_clump_link",0,256);
        b.maximum=ini.integer(L"bones",L"max_nodes",1,256);
        b.max_atomics=ini.integer(L"bones",L"max_atomics",1,128);
        auto& m=next.mta;
        m.client_sha256=ini.text(L"mta",L"client_sha256");
        m.game_sha256=ini.text(L"mta",L"game_sa_sha256");
        for (auto* hash:{&m.client_sha256,&m.game_sha256}) {
            if (hash->size()!=64) throw std::runtime_error("SHA256 MTA invalido");
            for (auto& c:*hash) {
                if (!std::iswxdigit(c)) throw std::runtime_error("SHA256 MTA invalido");
                c=static_cast<wchar_t>(std::towupper(c));
            }
        }
        m.game=ini.integer(L"mta",L"client_game_rva",1,0x10000000);
        m.player_vtable=ini.integer(L"mta",L"player_vtable_rva",1,0x10000000);
        m.manager=ini.integer(L"mta",L"game_player_manager",0,0x1000);
        m.begin=ini.integer(L"mta",L"players_begin",0,0x1000);
        m.end=ini.integer(L"mta",L"players_end",0,0x1000);
        m.player_ped=ini.integer(L"mta",L"player_game_ped",0,0x10000);
        m.name=ini.integer(L"mta",L"player_nametag",0,0x10000);
        m.dimension=ini.integer(L"mta",L"element_dimension",0,0x10000,L"0");
        m.cached_position=ini.integer(L"mta",L"ped_cached_position",0,0x10000,L"0");
        next.visual_enabled=ini.integer(L"visual",L"enabled",0,1,L"1")!=0;
        next.armor_bar=ini.integer(L"visual",L"armor_bar",0,1,L"1")!=0;
        next.distance_label=ini.integer(L"visual",L"distance",0,1,L"1")!=0;
        next.snaplines=ini.integer(L"visual",L"snaplines",0,1,L"0")!=0;
        next.all_dimensions=ini.integer(L"visual",L"all_dimensions",0,1,L"1")!=0;
        next.max_armor=ini.real(L"max_armor",1,100000,L"100");
        next.snapline_thickness=ini.real(L"snapline_thickness",1,8,L"1");
        next.drawing_type=static_cast<DrawingType>(ini.integer(L"visual",L"drawing_type",0,2,L"0"));
        next.box_style=static_cast<BoxStyle>(ini.integer(L"visual",L"box_style",0,1,L"0"));
        const auto color_pair=[&](const wchar_t* key,ColorPair fallback) {
            const auto visible=std::wstring(key)+L"_visible",hidden=std::wstring(key)+L"_hidden";
            const auto v=std::to_wstring(fallback.visible),h=std::to_wstring(fallback.occluded);
            return ColorPair{ini.integer(L"colors",visible.c_str(),0,0xffffffffU,v.c_str()),
                             ini.integer(L"colors",hidden.c_str(),0,0xffffffffU,h.c_str())};
        };
        next.name_colors=color_pair(L"name",next.name_colors);
        next.distance_colors=color_pair(L"distance",next.distance_colors);
        next.box_colors=color_pair(L"box",{next.color,next.color});
        next.snapline_colors=color_pair(L"snapline",next.snapline_colors);
        next.skeleton_colors=color_pair(L"skeleton",{next.color,next.color});
        next.health_color=ini.integer(L"colors",L"health",0,0xffffffffU,L"0xff55ee77");
        next.armor_color=ini.integer(L"colors",L"armor",0,0xffffffffU,L"0xff559fff");
        next.panel_key=ini.integer(L"controls",L"panel_vk",1,254,L"45");
        next.visibility.camera=ini.integer(L"visibility",L"camera_position_rva",0,0x10000000,L"0");
        next.visibility.line_clear=ini.integer(L"visibility",L"line_clear_rva",0,0x10000000,L"0");
        next.visibility.buildings=ini.integer(L"visibility",L"buildings",0,1,L"1")!=0;
        next.visibility.vehicles=ini.integer(L"visibility",L"vehicles",0,1,L"1")!=0;
        next.visibility.objects=ini.integer(L"visibility",L"objects",0,1,L"1")!=0;
        next.reload_key=ini.integer(L"controls",L"reload_vk",1,254);
        next.stop_key=ini.integer(L"controls",L"stop_vk",1,254);
        if (next.reload_key==next.stop_key || next.panel_key==next.reload_key || next.panel_key==next.stop_key)
            throw std::runtime_error("Atalhos devem ser distintos");
        next.wait_ms=ini.integer(L"runtime",L"device_timeout_ms",1000,300000);
        next.poll_ms=ini.integer(L"runtime",L"poll_ms",1,1000);
        next.separate_overlay=ini.integer(L"capture",L"separate_overlay",0,1,L"1")!=0;
        next.frame_timeout_ms=ini.integer(L"capture",L"frame_timeout_ms",50,2000,L"250");
        next.collect_interval_ms=ini.integer(L"runtime",L"collect_interval_ms",5,1000,L"25");
        next.module_check_ms=ini.integer(L"runtime",L"module_check_ms",250,10000,L"1000");
        if (next.collect_interval_ms>next.frame_timeout_ms) throw std::runtime_error("collect_interval_ms supera frame_timeout_ms");
        if ((!next.visibility.camera)!=(!next.visibility.line_clear)) throw std::runtime_error("Camera e linha de visao devem ser configuradas juntas");
        auto& aim=next.aim;
        aim.enabled=ini.integer(L"aim",L"enabled",0,1,L"0")!=0;
        for (const auto& option:aim_toggles) {
            const auto fallback=aim.*(option.member)?L"1":L"0";
            aim.*(option.member)=ini.integer(L"aim",option.key,0,1,fallback)!=0;
        }
        const auto aim_real=[&](const wchar_t* key,float low,float high,const wchar_t* fallback) {
            const auto value=ini.text(L"aim",key,fallback); wchar_t* end{}; errno=0;
            const float number=std::wcstof(value.c_str(),&end);
            if (errno || end==value.c_str() || *end || !std::isfinite(number) || number<low || number>high)
                throw std::runtime_error("Valor de mira fora do limite");
            return number;
        };
        aim.fov=aim_real(L"fov_px",1,600,L"70");
        aim.smooth=aim_real(L"smooth",1,30,L"2");
        aim.prediction_ms=aim_real(L"prediction_ms",0,200,L"0");
        aim.input_gain=aim_real(L"input_gain",0.01f,5,L"1");
        aim.max_step=aim_real(L"max_step",1,100,L"24");
        aim.tap_boost=aim_real(L"tap_boost",1,4,L"2");
        aim.tap_tolerance=aim_real(L"tap_tolerance_px",.1f,4,L"0.75");
        aim.tap_resume=aim_real(L"tap_resume_px",aim.tap_tolerance,8,L"1.5");
        aim.bone=ini.integer(L"aim",L"bone",0,2,L"0");
        aim.mode=ini.integer(L"aim",L"mode",0,2,L"0");
        aim.key=ini.integer(L"aim",L"key",1,254,L"1");
        aim.switch_key=ini.integer(L"aim",L"switch_key",0,254,L"0");
        aim.target_color=ini.integer(L"aim",L"target_color",0,0xffffffffU,L"0xff72d7ff");
        aim.vehicle_byte=ini.integer(L"aim",L"ped_in_vehicle_byte",0,l.stride-1,L"0");
        aim.vehicle_mask=ini.integer(L"aim",L"ped_in_vehicle_mask",1,255,L"1");
        aim.crosshair_x=ini.integer(L"aim",L"crosshair_x_rva",0,0x10000000,L"0");
        aim.crosshair_y=ini.integer(L"aim",L"crosshair_y_rva",0,0x10000000,L"0");
        aim.active_camera=ini.integer(L"aim",L"active_camera_rva",0,0x10000000,L"0");
        aim.camera_modes=ini.integer(L"aim",L"camera_modes_rva",0,0x10000000,L"0");
        aim.camera_stride=ini.integer(L"aim",L"camera_stride",0,4096,L"0");
        aim.crosshair_probe=ini.integer(L"aim",L"crosshair_probe_rva",0,0x10000000,L"0");
        for (const auto key:{aim.key,aim.switch_key})
            if (key && (key==next.panel_key || key==next.reload_key || key==next.stop_key))
                throw std::runtime_error("Tecla da mira conflita com atalho do painel");
        if (aim.switch_key && aim.switch_key==aim.key) throw std::runtime_error("Teclas da mira devem ser distintas");
        result=std::move(next);
        return true;
    } catch (const std::exception& exception) {
        const std::string message=exception.what();
        error.assign(message.begin(),message.end());
        return false;
    }
}
}
