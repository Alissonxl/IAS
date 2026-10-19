#include "bones.hpp"
#include "memory.hpp"
#include <algorithm>

namespace esp {
namespace {
struct Hierarchy {
    std::int32_t flags,count;
    std::uintptr_t matrices,unaligned,nodes;
};
struct Node { std::int32_t id,index,flags; std::uintptr_t frame; };
struct Matrix { std::array<float,12> rotation; Vec3 position; std::uint32_t pad; };
static_assert(sizeof(Hierarchy)==20 && sizeof(Node)==16 && sizeof(Matrix)==64);
constexpr std::uint8_t rw_clump=2;
constexpr std::size_t maximum_nodes=256;
}
Skeleton read_skeleton(std::uintptr_t base,std::uintptr_t ped,const Vec3& origin,const Settings& settings) {
    Skeleton result{};
    const auto& layout=settings.bones;
    std::uintptr_t clump{},link{},hierarchy{};
    std::int32_t plugin{};
    std::uint8_t type{};
    if (!read(ped+layout.clump,clump) || !clump || !read(clump,type) || type!=rw_clump ||
        !read(base+layout.skin_plugin,plugin) || plugin<static_cast<std::int32_t>(layout.atomic_link+8) || plugin>0x2000 ||
        !read(clump+layout.clump_list,link)) return result;
    const auto sentinel=clump+layout.clump_list;
    // The skin plugin stores a hierarchy pointer in each RpAtomic. A bounded list walk
    // replaces the engine callback, so a damaged clump cannot execute or loop indefinitely.
    for (std::uint32_t i=0;i<layout.max_atomics && link && link!=sentinel;++i) {
        if (link<layout.atomic_link) return result;
        std::uintptr_t next{};
        if (!read(link,next) || !read(link-layout.atomic_link+static_cast<std::uintptr_t>(plugin),hierarchy)) return result;
        if (hierarchy) break;
        if (next==link) return result;
        link=next;
    }
    Hierarchy header{};
    if (!hierarchy || !read(hierarchy,header) || header.count<=0 ||
        static_cast<std::uint32_t>(header.count)>layout.maximum || static_cast<std::size_t>(header.count)>maximum_nodes ||
        !header.nodes || !header.matrices) return result;
    std::array<Node,maximum_nodes> nodes;
    std::array<Matrix,maximum_nodes> matrices;
    const auto count=static_cast<std::size_t>(header.count);
    if (!read_bytes(header.nodes,nodes.data(),count*sizeof(Node)) ||
        !read_bytes(header.matrices,matrices.data(),count*sizeof(Matrix))) return result;
    for (std::size_t n=0;n<count;++n) {
        const auto found=std::find(bone_ids.begin(),bone_ids.end(),nodes[n].id);
        if (found==bone_ids.end()) continue;
        const auto slot=static_cast<std::size_t>(found-bone_ids.begin());
        const auto& p=matrices[n].position;
        if (!finite(p) || distance(origin,p)>settings.bone_radius) continue;
        result.positions[slot]=p;
        result.valid[slot]=true;
    }
    std::uintptr_t current{};
    Hierarchy current_header{};
    if (!read(ped+layout.clump,current) || current!=clump || !read(hierarchy,current_header) ||
        current_header.count!=header.count || current_header.nodes!=header.nodes || current_header.matrices!=header.matrices) return {};
    return result;
}
}
