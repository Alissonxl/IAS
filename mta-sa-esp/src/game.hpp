#pragma once
#include "config.hpp"
#include "math.hpp"
#include "bones.hpp"
#include "mta_players.hpp"
#include <vector>

namespace esp {
enum class Visibility { Unknown,Visible,Occluded };
struct Entity {
    Vec3 position; float health{}, max_health{}, distance{};
    Skeleton skeleton; std::wstring name;
    float armor{}; bool armor_known{},health_known{true},streamed{true};
    Visibility visibility{Visibility::Unknown};
    std::uint64_t identity{};
    bool is_player{true},in_vehicle{},vehicle_known{},aim_eligible{};
};
struct AimVisual { Vec3 world{}; bool selected{}; Vec2 center{.5f,.5f}; bool center_valid{true}; bool aligned{}; };
struct Frame { ViewMatrix view{}; std::vector<Entity> entities; std::wstring status; std::vector<Entity> aim_entities; };
Frame collect_frame(std::uintptr_t base, const Settings& settings,const MtaPlayers* players=nullptr);
}
