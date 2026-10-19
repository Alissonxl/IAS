#pragma once
#include "config.hpp"
#include "math.hpp"
#include <array>

namespace esp {
// IDs are RenderWare bone tags, not positions in a particular skin's matrix array.
inline constexpr std::array<std::int32_t,21> bone_ids{8,5,4,3,2,22,23,24,25,32,33,34,35,41,42,43,44,51,52,53,54};
inline constexpr std::array<std::array<std::size_t,2>,20> bone_links{{
    {0,1},{1,2},{2,3},{3,4},{2,5},{5,6},{6,7},{7,8},
    {2,9},{9,10},{10,11},{11,12},{4,13},{13,14},{14,15},{15,16},
    {4,17},{17,18},{18,19},{19,20}
}};
struct Skeleton {
    std::array<Vec3,bone_ids.size()> positions{};
    std::array<bool,bone_ids.size()> valid{};
};
Skeleton read_skeleton(std::uintptr_t base,std::uintptr_t ped,const Vec3& origin,const Settings& settings);
}
