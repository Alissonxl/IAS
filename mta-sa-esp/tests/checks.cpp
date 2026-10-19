#include "math.hpp"
#include "memory.hpp"
#include "config.hpp"
#include "game.hpp"
#include "visibility.hpp"
#include "frame_state.hpp"
#include "startup_gate.hpp"
#include <array>
#include <algorithm>
#include <cstdio>
#include <limits>

namespace {
bool trace_result=true;
unsigned trace_calls{};
esp::Vec3 last_trace_camera{};
bool __cdecl fake_trace(const esp::Vec3& camera,const esp::Vec3&,bool,bool,bool peds,bool,bool,bool,bool) {
    ++trace_calls;
    last_trace_camera=camera;
    return !peds && trace_result;
}
int failures{};
void check(bool condition,const char* label) {
    std::printf("%s %s\n",condition?"PASS":"FAIL",label);
    if (!condition) ++failures;
}
}
int wmain(int argc,wchar_t** argv) {
    using Gate=esp::StartupGate;
    Gate startup;
    check(startup.sample(false,true,100,200,1000)==Gate::State::Modules,
          "an early GTA device cannot install callbacks before MTA is ready");
    check(startup.sample(true,false,100,200,2000)==Gate::State::Player,
          "startup waits for a local player after modules load");
    check(startup.sample(true,true,0,0,3000)==Gate::State::Device,
          "startup waits for a current device and vtable");
    check(startup.sample(true,true,100,200,48000)==Gate::State::Settling &&
          startup.sample(true,true,100,200,48999)==Gate::State::Settling &&
          startup.sample(true,true,100,200,49000)==Gate::State::Ready,
          "late MTA initialization starts a fresh stability interval");
    check(startup.sample(true,true,101,201,49001)==Gate::State::Settling,
          "a replaced device cannot reuse the earlier stability interval");
    check(startup.sample(true,true,101,202,50000)==Gate::State::Settling &&
          startup.sample(true,true,101,202,51000)==Gate::State::Ready,
          "vtable replacement restarts stabilization even on the same device");
    check(startup.sample(false,true,101,202,52000)==Gate::State::Modules &&
          startup.sample(true,true,101,202,53000)==Gate::State::Settling,
          "module loss invalidates an earlier ready state");
    Gate next_process;
    check(next_process.sample(true,true,101,202,54000)==Gate::State::Settling,
          "a new process never inherits readiness from an earlier session");
    check(next_process.sample(true,true,101,202,53000)==Gate::State::Settling,
          "backward time resets the stability interval");
    esp::MtaReader uninitialized_reader;
    check(!uninitialized_reader.ready({}),"unverified MTA identities cannot authorize the first binding");
    const auto snapshot_settings=std::make_shared<const esp::Settings>();
    esp::PublishedFrame published{};
    published.settings=snapshot_settings; published.timestamp=100; published.collected_at=100;
    check(!published.needs_collection(snapshot_settings,124) && published.needs_collection(snapshot_settings,125),
          "collection cadence reuses samples until the configured boundary");
    const auto changed_settings=std::make_shared<const esp::Settings>(*snapshot_settings);
    check(!published.matches(changed_settings,110,250) && published.needs_collection(changed_settings,110),
          "new settings invalidate previous samples immediately");
    check(!published.matches(snapshot_settings,351,250) && !published.matches(snapshot_settings,99,250) &&
          !published.matches({},110,250),"expired, backward-time and null snapshots are rejected");
    unsigned collections{};
    std::shared_ptr<esp::PublishedFrame> scheduled;
    for (ULONGLONG tick=0;tick<1000;tick+=5) {
        if (!scheduled || scheduled->needs_collection(snapshot_settings,tick)) {
            scheduled=std::make_shared<esp::PublishedFrame>();
            scheduled->settings=snapshot_settings; scheduled->timestamp=tick; scheduled->collected_at=tick; ++collections;
        }
    }
    check(collections==40,"200 simulated render callbacks perform only 40 complete collections");
    published.timestamp=124;
    check(published.needs_collection(snapshot_settings,125),"fresh camera publication does not postpone pose collection");
    published.frame=std::make_shared<const esp::Frame>();
    auto fresh_camera=published;
    fresh_camera.timestamp=125; fresh_camera.view[12]=.75f;
    check(fresh_camera.frame==published.frame && fresh_camera.view[12]!=published.view[12],
          "camera snapshots share immutable poses without copying player data");
    esp::ViewMatrix matrix{};
    matrix[0]=1; matrix[5]=1; matrix[10]=1; matrix[12]=0.5f; matrix[13]=0.5f;
    esp::Vec2 screen{99,99};
    check(esp::project({0,0,1},matrix,1920,1080,0.01f,screen) && screen.x==960 && screen.y==540,"GTA projection uses viewport bias exactly once");
    check(!esp::project({0,0,-1},matrix,1920,1080,0.01f,screen) && screen.x==0 && screen.y==0,"behind-camera projection clears output");
    check(!esp::project({0,0,0.001f},matrix,1920,1080,0.01f,screen),"near-plane rejection");
    check(!esp::project({std::numeric_limits<float>::infinity(),0,1},matrix,1920,1080,0.01f,screen),"nonfinite world position rejected");
    matrix[0]=std::numeric_limits<float>::quiet_NaN();
    check(!esp::project({1,0,1},matrix,1920,1080,0.01f,screen),"nonfinite camera coefficient rejected");
    std::uint32_t source=0,value=123;
    check(esp::read(reinterpret_cast<std::uintptr_t>(&source),value) && value==0,"successful zero distinguished from failure");
    value=123;
    check(!esp::read(1,value) && value==0,"unreadable memory clears stale output");
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    void* region=VirtualAlloc(nullptr,system.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (region) {
        DWORD old{};
        VirtualProtect(static_cast<unsigned char*>(region)+system.dwPageSize,system.dwPageSize,PAGE_NOACCESS,&old);
        value=123;
        const auto crossing=reinterpret_cast<std::uintptr_t>(region)+system.dwPageSize-2;
        check(!esp::read(crossing,value) && value==0,"partial cross-page read fails closed");
        VirtualFree(region,0,MEM_RELEASE);
    } else check(false,"allocate boundary-read fixture");
    esp::Settings settings{};
    std::wstring error;
    check(!esp::load_settings(L"Z:\\missing-mta-esp-test.ini",settings,error) && !error.empty(),"missing configuration rejected");
    if (argc==2) {
        check(esp::load_settings(argv[1],settings,error),"supplied profile parses");
        check(settings.layout.stride==0x7c4 && settings.sha256.size()==64,"profile layout and fingerprint populated");
        settings.all_dimensions=true;
        settings.visibility={};
        check(settings.separate_overlay && settings.frame_timeout_ms==250,"separate overlay enabled in supplied profile");
        wchar_t temporary_directory[MAX_PATH]{},temporary_profile[MAX_PATH]{};
        if (GetTempPathW(MAX_PATH,temporary_directory) && GetTempFileNameW(temporary_directory,L"moc",0,temporary_profile) &&
            CopyFileW(argv[1],temporary_profile,FALSE)) {
            esp::Settings parsed{};
            WritePrivateProfileStringW(L"controls",L"panel_vk",nullptr,temporary_profile);
            WritePrivateProfileStringW(L"controls",L"toggle_vk",L"0x2D",temporary_profile);
            check(esp::load_settings(temporary_profile,parsed,error) && parsed.panel_key==VK_INSERT,
                  "legacy toggle key is ignored and missing panel shortcut defaults to INSERT");
            WritePrivateProfileStringW(L"controls",L"panel_vk",L"0x74",temporary_profile);
            check(!esp::load_settings(temporary_profile,parsed,error),"panel shortcut cannot collide with reload");
            WritePrivateProfileStringW(L"controls",L"panel_vk",L"0x2D",temporary_profile);
            WritePrivateProfileStringW(L"capture",nullptr,nullptr,temporary_profile);
            check(esp::load_settings(temporary_profile,parsed,error) && parsed.separate_overlay && parsed.frame_timeout_ms==250,
                  "older profiles default to separate rendering");
            WritePrivateProfileStringW(L"capture",L"separate_overlay",L"2",temporary_profile);
            check(!esp::load_settings(temporary_profile,parsed,error),"invalid overlay mode rejected");
            WritePrivateProfileStringW(L"capture",L"separate_overlay",L"1",temporary_profile);
            WritePrivateProfileStringW(L"capture",L"frame_timeout_ms",L"0",temporary_profile);
            check(!esp::load_settings(temporary_profile,parsed,error),"zero frame freshness timeout rejected");
            WritePrivateProfileStringW(L"capture",L"frame_timeout_ms",L"2001",temporary_profile);
            check(!esp::load_settings(temporary_profile,parsed,error),"unbounded stale frame timeout rejected");
            WritePrivateProfileStringW(L"capture",L"frame_timeout_ms",L"250",temporary_profile);
            WritePrivateProfileStringW(L"runtime",L"collect_interval_ms",L"251",temporary_profile);
            check(!esp::load_settings(temporary_profile,parsed,error),"collection interval cannot exceed frame lifetime");
            WritePrivateProfileStringW(L"runtime",L"collect_interval_ms",L"25",temporary_profile);
            WritePrivateProfileStringW(L"visibility",L"camera_position_rva",L"0",temporary_profile);
            check(!esp::load_settings(temporary_profile,parsed,error),"partial visibility function profile rejected");
        } else check(false,"create capture configuration fixture");
        if (*temporary_profile) DeleteFileW(temporary_profile);
        // Collection fixtures exercise these readers independently of saved UI preferences.
        settings.armor_bar=true;
        settings.names=true;
        std::array<unsigned char,0x7c4*3> peds{};
        std::array<std::uint8_t,3> slots{};
        struct Pool { std::uintptr_t objects,flags; std::int32_t capacity; };
        Pool pool{reinterpret_cast<std::uintptr_t>(peds.data()),reinterpret_cast<std::uintptr_t>(slots.data()),3};
        std::uintptr_t pool_pointer=reinterpret_cast<std::uintptr_t>(&pool),local=reinterpret_cast<std::uintptr_t>(peds.data());
        esp::ViewMatrix view{};
        view[14]=1;
        settings.layout.pool=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&pool_pointer));
        settings.layout.local=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&local));
        settings.layout.view=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&view));
        const auto write=[&](std::size_t ped,std::uint32_t offset,const auto& data){std::memcpy(peds.data()+ped*0x7c4+offset,&data,sizeof(data));};
        for (std::size_t i=0;i<3;++i) {
            write(i,settings.layout.position,esp::Vec3{static_cast<float>(i*10),0,0});
            write(i,settings.layout.health,50.0f); write(i,settings.layout.max_health,100.0f);
            write(i,settings.layout.armor,75.0f);
            write(i,settings.layout.player_data,static_cast<std::uintptr_t>(i<2?1:0));
        }
        settings.players_only=true;
        auto frame=esp::collect_frame(0,settings);
        check(frame.entities.size()==1 && frame.entities[0].armor_known && frame.entities[0].armor==75,"armor read independently from health");
        auto no_armor=settings; no_armor.armor_bar=false; no_armor.labels=false;
        auto without_armor=esp::collect_frame(0,no_armor);
        check(without_armor.entities.size()==1 && !without_armor.entities[0].armor_known,
              "disabled armor and numeric labels skip armor sampling");
        auto dimension_guard=settings; dimension_guard.all_dimensions=false;
        esp::MtaPlayers unavailable;
        check(esp::collect_frame(0,dimension_guard,&unavailable).entities.empty() &&
              esp::collect_frame(0,dimension_guard).entities.empty(),"missing dimension metadata never falls back to showing players");
        write(1,settings.layout.armor,-1.0f);
        auto invalid_armor=esp::collect_frame(0,settings);
        check(invalid_armor.entities.size()==1 && !invalid_armor.entities[0].armor_known,"invalid armor omitted without hiding valid health");
        write(1,settings.layout.armor,75.0f);
        esp::Vec3 camera{0,0,2};
        auto visibility_settings=settings;
        visibility_settings.visibility.camera=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&camera));
        visibility_settings.visibility.line_clear=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&fake_trace));
        esp::Entity visible_entity{};
        trace_result=true; trace_calls=0;
        check(esp::trace_visibility(0,visible_entity,visibility_settings)==esp::Visibility::Visible && trace_calls==1,
              "clear world ray is visible and never tests peds");
        const auto prepared_visibility=esp::prepare_visibility(0,visibility_settings);
        camera={8,8,8};
        check(esp::trace_visibility(prepared_visibility,visible_entity,visibility_settings)==esp::Visibility::Visible &&
              last_trace_camera.x==0 && last_trace_camera.z==2,"prepared visibility shares one camera sample for the collection");
        camera={0,0,2};
        trace_result=false; trace_calls=0;
        check(esp::trace_visibility(0,visible_entity,visibility_settings)==esp::Visibility::Occluded && trace_calls==2,
              "both blocked body rays are occluded");
        visible_entity.streamed=false;
        check(esp::trace_visibility(0,visible_entity,visibility_settings)==esp::Visibility::Unknown,
              "cached position never claims current visibility");
        check(!esp::matches_drawing_type(esp::Visibility::Unknown,esp::DrawingType::Visible) &&
              !esp::matches_drawing_type(esp::Visibility::Unknown,esp::DrawingType::Occluded),"unknown visibility excluded from specific filters");
        check(frame.entities.size()==1 && frame.entities[0].distance==10 && frame.entities[0].health==50,"pool skips local ped and applies player-data filter");
        settings.players_only=false;
        check(esp::collect_frame(0,settings).entities.size()==2,"optional NPC inclusion");
        slots[1]=0x80;
        check(esp::collect_frame(0,settings).entities.size()==1,"pool skips free slots");
        pool.capacity=static_cast<std::int32_t>(settings.layout.maximum+1);
        check(esp::collect_frame(0,settings).entities.empty(),"oversized pool rejected before traversal");
        pool.capacity=3; slots[1]=0;
        struct MsvcString { std::array<char,16> storage; std::uint32_t size,capacity; } small{};
        const char utf8_name[]="#FF0000Jo\xc3\xa3o";
        std::memcpy(small.storage.data(),utf8_name,sizeof(utf8_name));
        small.size=sizeof(utf8_name)-1; small.capacity=15;
        std::wstring nickname;
        check(esp::read_msvc_name(reinterpret_cast<std::uintptr_t>(&small),nickname) && nickname==L"Jo\u00e3o","UTF-8 nametag decoded and color markup stripped");
        const char long_name[]="Jogador_Com_Nome_Longo";
        MsvcString large{};
        const auto long_pointer=reinterpret_cast<std::uintptr_t>(long_name);
        std::memcpy(large.storage.data(),&long_pointer,sizeof(long_pointer));
        large.size=sizeof(long_name)-1; large.capacity=31;
        check(esp::read_msvc_name(reinterpret_cast<std::uintptr_t>(&large),nickname) && nickname==L"Jogador_Com_Nome_Longo","heap-backed MSVC string decoded without foreign STL calls");
        small.storage[0]=static_cast<char>(0xff);
        check(!esp::read_msvc_name(reinterpret_cast<std::uintptr_t>(&small),nickname) && nickname.empty(),"invalid UTF-8 never becomes a fabricated name");
        large.size=129;
        check(!esp::read_msvc_name(reinterpret_cast<std::uintptr_t>(&large),nickname),"oversized nametag rejected");

        std::array<unsigned char,0x800> player{};
        std::array<unsigned char,0x100> wrapper{};
        std::array<std::int32_t,8> wrapper_vb{},entity_vb{};
        std::array<unsigned char,0x80> client_game{},manager{};
        const auto addr=[](const auto& value){return reinterpret_cast<std::uintptr_t>(value.data());};
        const auto store=[](auto& buffer,std::size_t offset,const auto& value){std::memcpy(buffer.data()+offset,&value,sizeof(value));};
        const auto native=addr(peds)+0x7c4;
        wrapper_vb[1]=0x3c; entity_vb[5]=0x30;
        store(wrapper,4,addr(wrapper_vb)); store(wrapper,0x2c,addr(entity_vb)); store(wrapper,0x64,native);
        auto mta=settings.mta;
        std::uintptr_t game_pointer=addr(client_game);
        mta.game=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&game_pointer));
        mta.player_vtable=0x12345000;
        store(player,0,static_cast<std::uintptr_t>(mta.player_vtable)); store(player,mta.player_ped,addr(wrapper));
        small={}; small.size=4; small.capacity=15; std::memcpy(small.storage.data(),"Nome",5);
        store(player,mta.name,small);
        std::array<std::uintptr_t,2> player_list{addr(player),addr(player)};
        store(client_game,mta.manager,addr(manager));
        store(manager,mta.begin,addr(player_list)); store(manager,mta.end,addr(player_list)+sizeof(std::uintptr_t));
        auto players=esp::read_mta_players(0,mta);
        check(players.available && players.names.size()==1 && players.names.at(native)==L"Nome","MTA player maps through virtual-base adjustments to the exact GTA ped");
        check(players.dimensions.contains(native) && players.dimensions.at(native)==0,"MTA dimension read from verified layout");
        settings.players_only=true;
        players.dimensions[local]=1;
        settings.all_dimensions=false;
        check(esp::collect_frame(0,settings,&players).entities.empty(),"different dimension excluded when all dimensions disabled");
        players.dimensions[local]=0;
        check(esp::collect_frame(0,settings,&players).entities.size()==1,"same dimension included");
        settings.all_dimensions=true;
        players.cached.push_back({{20,0,0},L"Cached",7});
        auto cached_frame=esp::collect_frame(0,settings,&players);
        check(cached_frame.entities.size()==2 && !cached_frame.entities.back().health_known && !cached_frame.entities.back().streamed,
              "all dimensions includes cached positions without fabricated health");
        players.cached.clear();
        store(player,mta.player_ped,static_cast<std::uintptr_t>(0));
        store(player,mta.cached_position,esp::Vec3{30,0,0});
        store(player,mta.dimension,static_cast<std::uint16_t>(7));
        const auto unstreamed=esp::read_mta_players(0,mta);
        check(unstreamed.available && unstreamed.names.empty() && unstreamed.cached.size()==1 &&
              unstreamed.cached[0].dimension==7 && unstreamed.cached[0].position.x==30,"unstreamed MTA record preserves cached position and dimension");
        store(player,mta.player_ped,addr(wrapper));
        store(player,mta.dimension,static_cast<std::uint16_t>(0));
        settings.players_only=true;
        frame=esp::collect_frame(0,settings,&players);
        check(frame.entities.size()==1 && frame.entities[0].name==L"Nome","real MTA membership filters entities and attaches names");
        auto aim_collection=settings;
        aim_collection.aim.enabled=true; aim_collection.all_dimensions=false;
        aim_collection.aim.vehicle_byte=0x46d;
        aim_collection.drawing_type=esp::DrawingType::Occluded;
        aim_collection.visibility=visibility_settings.visibility;
        trace_result=true;
        write(1,aim_collection.aim.vehicle_byte,static_cast<std::uint8_t>(1));
        const auto aim_frame=esp::collect_frame(0,aim_collection,&players);
        check(aim_frame.entities.empty() && aim_frame.aim_entities.size()==2,
              "aim collection keeps players and NPCs independently from Visual filters");
        check(aim_frame.aim_entities.size()==2 && aim_frame.aim_entities[0].vehicle_known &&
              aim_frame.aim_entities[0].in_vehicle && !aim_frame.aim_entities[1].is_player &&
              aim_frame.aim_entities[1].visibility==esp::Visibility::Visible,
              "aim metadata includes vehicle flag and current NPC visibility");
        slots[1]=1;
        const auto reused=esp::collect_frame(0,aim_collection,&players);
        check(!reused.aim_entities.empty() && !aim_frame.aim_entities.empty() &&
              reused.aim_entities[0].identity!=aim_frame.aim_entities[0].identity,
              "pool generation changes target identity on slot reuse");
        slots[1]=0;
        players.dimensions[native]=5;
        const auto other_dimension=esp::collect_frame(0,aim_collection,&players);
        check(std::none_of(other_dimension.aim_entities.begin(),other_dimension.aim_entities.end(),
              [](const esp::Entity& e){return e.is_player;}),"aim cannot target a player in another dimension");
        players.dimensions[native]=0;
        store(manager,mta.end,addr(player_list)+sizeof(player_list));
        const auto duplicate=esp::read_mta_players(0,mta);
        check(!duplicate.available && duplicate.names.empty() && duplicate.dimensions.empty() && duplicate.cached.empty(),
              "ambiguous MTA binding clears every output collection");
        store(manager,mta.end,addr(player_list)+sizeof(std::uintptr_t));
        store(player,0,static_cast<std::uintptr_t>(mta.player_vtable+4));
        check(!esp::read_mta_players(0,mta).available,"wrong MTA player vtable rejected");

        std::array<unsigned char,0x20> clump{};
        std::array<unsigned char,0x100> atomic{};
        struct BoneMatrix { std::array<float,12> basis; esp::Vec3 position; std::uint32_t pad; };
        struct Node { std::int32_t id,index,flags; std::uintptr_t frame; };
        std::array<Node,21> nodes{};
        std::array<BoneMatrix,21> matrices{};
        for (std::size_t i=0;i<nodes.size();++i) {
            nodes[i].id=esp::bone_ids[nodes.size()-1-i]; nodes[i].index=static_cast<std::int32_t>(i);
            matrices[i].position={10,0,static_cast<float>(nodes[i].id)/100};
        }
        struct Hierarchy { std::int32_t flags,count; std::uintptr_t matrices,unaligned,nodes; } hierarchy{0,21,addr(matrices),0,addr(nodes)};
        std::int32_t skin_plugin=0x80;
        settings.bones.skin_plugin=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&skin_plugin));
        clump[0]=2;
        store(clump,settings.bones.clump_list,addr(atomic)+settings.bones.atomic_link);
        store(atomic,settings.bones.atomic_link,addr(clump)+settings.bones.clump_list);
        store(atomic,static_cast<std::size_t>(skin_plugin),reinterpret_cast<std::uintptr_t>(&hierarchy));
        write(1,settings.bones.clump,addr(clump));
        auto skeleton=esp::read_skeleton(0,native,{10,0,0},settings);
        check(skeleton.valid[0] && std::abs(skeleton.positions[0].z-.08f)<.0001f && skeleton.valid[20],"bone IDs resolve to their actual matrices despite reversed node order");
        matrices.back().position={10000,0,0};
        skeleton=esp::read_skeleton(0,native,{10,0,0},settings);
        check(!skeleton.valid[0] && skeleton.valid[1],"bone far outside ped bounds omitted without losing valid bones");
        hierarchy.count=static_cast<std::int32_t>(settings.bones.maximum+1);
        skeleton=esp::read_skeleton(0,native,{10,0,0},settings);
        check(std::none_of(skeleton.valid.begin(),skeleton.valid.end(),[](bool v){return v;}),"invalid hierarchy size produces no skeleton");
        hierarchy.count=21;
        store(atomic,static_cast<std::size_t>(skin_plugin),static_cast<std::uintptr_t>(0));
        store(atomic,settings.bones.atomic_link,addr(atomic)+settings.bones.atomic_link);
        skeleton=esp::read_skeleton(0,native,{10,0,0},settings);
        check(std::none_of(skeleton.valid.begin(),skeleton.valid.end(),[](bool v){return v;}),"cyclic clump list terminates without bone data");
    }
    std::printf("%d failure(s)\n",failures);
    return failures?1:0;
}
