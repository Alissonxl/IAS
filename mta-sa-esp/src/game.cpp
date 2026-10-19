#include "game.hpp"
#include "memory.hpp"
#include "visibility.hpp"
#include <algorithm>
#include <limits>

namespace esp {
namespace {
bool position_of(std::uintptr_t ped, const Layout& layout, Vec3& result) {
    result={};
    std::uintptr_t matrix{};
    if (!read(ped+layout.matrix,matrix)) return false;
    const bool ok = matrix ? read(matrix+layout.matrix_position,result) : read(ped+layout.position,result);
    return ok && finite(result) && std::abs(result.x)<100000 && std::abs(result.y)<100000 && std::abs(result.z)<100000;
}
}
Frame collect_frame(std::uintptr_t base, const Settings& settings,const MtaPlayers* players) {
    Frame frame{};
    const auto& l=settings.layout;
    std::uintptr_t local{},pool{},objects{},flags{};
    std::int32_t capacity{};
    Vec3 local_position{};
    if (!read(base+l.local,local) || !local || !position_of(local,l,local_position)) {
        frame.status=L"Aguardando personagem local";
        return frame;
    }
    std::uint16_t local_dimension{};
    bool has_local_dimension=false;
    if (players && players->available) {
        const auto found=players->dimensions.find(local);
        if (found!=players->dimensions.end()) { local_dimension=found->second; has_local_dimension=true; }
    }
    if (!settings.all_dimensions && !has_local_dimension) {
        frame.status=L"Dimensao local indisponivel; desenho suspenso";
        return frame;
    }
    if (!read(base+l.view,frame.view)) { frame.status=L"Falha na matriz de camera"; return frame; }
    if (!std::all_of(frame.view.begin(),frame.view.end(),[](float f){return std::isfinite(f);})) {
        frame.status=L"Matriz de camera invalida"; return frame;
    }
    if (!read(base+l.pool,pool) || !pool || !read(pool+l.objects,objects) || !objects ||
        !read(pool+l.flags,flags) || !flags || !read(pool+l.capacity,capacity)) {
        frame.status=L"Falha ao ler pool de personagens"; return frame;
    }
    if (capacity<=0 || static_cast<std::uint32_t>(capacity)>l.maximum ||
        static_cast<std::uintptr_t>(capacity)>(std::numeric_limits<std::uintptr_t>::max()-objects)/l.stride) {
        frame.status=L"Capacidade do pool fora do limite"; return frame;
    }
    std::vector<std::uint8_t> slots(static_cast<std::size_t>(capacity));
    if (!read_bytes(flags,slots.data(),slots.size())) { frame.status=L"Falha no mapa do pool"; return frame; }
    const auto live_slots=static_cast<std::size_t>(std::count_if(slots.begin(),slots.end(),[](std::uint8_t flag){return !(flag&0x80U);}));
    const auto loaded_bound=settings.players_only && players && players->available?std::min(live_slots,players->names.size()):live_slots;
    const auto cached_bound=settings.all_dimensions && players && players->available && settings.drawing_type==DrawingType::All?players->cached.size():0;
    frame.entities.reserve(loaded_bound+cached_bound);
    unsigned failures{};
    const auto visibility=prepare_visibility(base,settings);
    const bool need_armor=settings.armor_bar || settings.labels;
    const bool packed_vitals=l.max_health==l.health+sizeof(float) && (!need_armor || !l.armor || l.armor==l.health+2*sizeof(float));
    for (std::size_t index=0;index<slots.size();++index) {
        if (slots[index]&0x80U) continue;
        const auto ped=objects+index*l.stride;
        if (ped==local) continue;
        Entity entity{};
        std::uintptr_t player_data{};
        if (!settings.aim.enabled && settings.players_only && players && players->available) {
            if (!players->names.contains(ped)) continue;
        } else if (!settings.aim.enabled && settings.players_only) {
            if (!read(ped+l.player_data,player_data)) { ++failures; continue; }
            if (!player_data) continue;
        }
        entity.identity=(static_cast<std::uint64_t>(ped)<<8)|slots[index];
        if (players && players->available) entity.is_player=players->names.contains(ped);
        else { if (!read(ped+l.player_data,player_data)) { ++failures; continue; } entity.is_player=player_data!=0; }
        if (!position_of(ped,l,entity.position)) { ++failures; continue; }
        entity.distance=distance(entity.position,local_position);
        if (!std::isfinite(entity.distance) || entity.distance>settings.max_distance) continue;
        bool same_dimension=false;
        if (players && players->available && has_local_dimension) {
            const auto ed=players->dimensions.find(ped);
            same_dimension=ed!=players->dimensions.end() && local_dimension==ed->second;
        }
        if (!settings.all_dimensions && !same_dimension && !(settings.aim.enabled && !entity.is_player)) continue;
        if (packed_vitals) {
            std::array<float,3> vitals{};
            const auto count=need_armor && l.armor?3U:2U;
            if (!read_bytes(ped+l.health,vitals.data(),count*sizeof(float))) {
                // A failed optional armor read must not suppress valid health fields.
                if (!read(ped+l.health,entity.health) || !read(ped+l.max_health,entity.max_health)) { ++failures; continue; }
            } else {
                entity.health=vitals[0]; entity.max_health=vitals[1]; entity.armor=vitals[2];
                entity.armor_known=count==3;
            }
        } else {
            if (!read(ped+l.health,entity.health) || !read(ped+l.max_health,entity.max_health)) { ++failures; continue; }
            entity.armor_known=need_armor && l.armor && read(ped+l.armor,entity.armor);
        }
        if (!std::isfinite(entity.health) || !std::isfinite(entity.max_health) ||
            entity.health<=0 || entity.max_health<=0 || entity.max_health>100000) continue;
        entity.armor_known=entity.armor_known && std::isfinite(entity.armor) && entity.armor>=0 && entity.armor<=100000;
        if (!entity.armor_known) entity.armor=0;
        entity.visibility=(!players || !players->available || same_dimension || !entity.is_player)?trace_visibility(visibility,entity,settings):Visibility::Unknown;
        if (settings.skeleton || settings.aim.enabled) entity.skeleton=read_skeleton(base,ped,entity.position,settings);
        if (settings.names && players && players->available) {
            const auto found=players->names.find(ped);
            if (found!=players->names.end()) entity.name=found->second;
        }
        if (settings.aim.enabled) {
            entity.aim_eligible=players && players->available && has_local_dimension &&
                (same_dimension || !entity.is_player);
            std::uint8_t vehicle{};
            entity.vehicle_known=settings.aim.vehicle_byte && read(ped+settings.aim.vehicle_byte,vehicle);
            entity.in_vehicle=entity.vehicle_known && (vehicle&settings.aim.vehicle_mask)!=0;
        }
        std::uint8_t current_flag{};
        if (!read(flags+index,current_flag) || current_flag!=slots[index]) { ++failures; continue; }
        if (settings.aim.enabled && entity.aim_eligible) frame.aim_entities.push_back(entity);
        if ((!settings.all_dimensions && !same_dimension) || (settings.players_only && !entity.is_player) ||
            !matches_drawing_type(entity.visibility,settings.drawing_type)) continue;
        frame.entities.push_back(std::move(entity));
    }
    if (settings.all_dimensions && players && players->available && settings.drawing_type==DrawingType::All) {
        for (const auto& cached:players->cached) {
            Entity entity{};
            entity.position=cached.position;
            entity.distance=distance(entity.position,local_position);
            if (!std::isfinite(entity.distance) || entity.distance>settings.max_distance) continue;
            entity.health_known=false;
            entity.streamed=false;
            if (settings.names) entity.name=cached.name;
            frame.entities.push_back(std::move(entity));
        }
    }
    frame.status=failures ? L"Algumas leituras falharam: "+std::to_wstring(failures) : L"Ativo";
    if (settings.names && players) frame.status+=L" | "+players->status;
    return frame;
}
}
