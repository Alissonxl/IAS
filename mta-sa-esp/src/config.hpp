#pragma once
#include "win.hpp"

namespace esp {
struct Layout {
    std::uint32_t device{}, pool{}, local{}, view{};
    std::uint32_t objects{}, flags{}, capacity{}, stride{}, maximum{};
    std::uint32_t matrix{}, position{}, matrix_position{}, player_data{}, health{}, max_health{}, armor{};
};
struct BoneLayout {
    std::uint32_t clump{}, skin_plugin{}, clump_list{}, atomic_link{}, maximum{}, max_atomics{};
};
struct MtaLayout {
    std::wstring client_sha256, game_sha256;
    std::uint32_t game{}, manager{}, begin{}, end{}, player_vtable{}, player_ped{}, name{},dimension{},cached_position{};
};
struct ColorPair { std::uint32_t visible{0xff5de2ff},occluded{0xfff29b57}; };
enum class DrawingType : std::uint32_t { All,Visible,Occluded };
enum class BoxStyle : std::uint32_t { Default,Corners };
struct VisibilityLayout { std::uint32_t camera{},line_clear{}; bool buildings{true},vehicles{true},objects{true}; };
struct AimSettings {
    bool enabled{},players{true},peds{},lock{true},show_target{true},in_car{},visible_check{true},show_fov{true};
    float fov{70},smooth{2},prediction_ms{},input_gain{1},max_step{24};
    float tap_boost{2},tap_tolerance{.75f},tap_resume{1.5f};
    std::uint32_t bone{},key{VK_LBUTTON},mode{},switch_key{};
    std::uint32_t vehicle_byte{},vehicle_mask{1};
    std::uint32_t target_color{0xff72d7ff};
    std::uint32_t crosshair_x{},crosshair_y{},active_camera{},camera_modes{},camera_stride{},crosshair_probe{};
};
struct Settings {
    std::wstring sha256;
    Layout layout;
    BoneLayout bones;
    MtaLayout mta;
    float max_distance{}, head_offset{}, foot_offset{}, box_ratio{}, thickness{}, near_clip{};
    std::uint32_t color{}, reload_key{}, stop_key{}, wait_ms{}, poll_ms{};
    float bone_radius{}, skeleton_thickness{};
    bool players_only{}, health_bar{}, labels{}, skeleton{}, names{}, boxes{};
    bool separate_overlay{true};
    std::uint32_t frame_timeout_ms{250};
    std::uint32_t collect_interval_ms{25},module_check_ms{1000};
    bool visual_enabled{true},armor_bar{true},distance_label{true},snaplines{},all_dimensions{true};
    float max_armor{100},snapline_thickness{1};
    DrawingType drawing_type{DrawingType::All};
    BoxStyle box_style{BoxStyle::Default};
    ColorPair name_colors{0xffa7cf52,0xffe4a455},distance_colors{0xffffffff,0xffb6bac5};
    ColorPair box_colors{},snapline_colors{},skeleton_colors{0xffeadb50,0xffd68d43};
    std::uint32_t health_color{0xff55ee77},armor_color{0xff559fff},panel_key{VK_INSERT};
    VisibilityLayout visibility;
    AimSettings aim;
};
bool load_settings(const std::wstring& path, Settings& result, std::wstring& error);
bool save_visual_settings(const std::wstring& path,const Settings& settings,std::wstring& error);
}
