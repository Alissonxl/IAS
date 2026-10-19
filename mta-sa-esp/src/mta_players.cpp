#include "mta_players.hpp"
#include "memory.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <vector>

namespace esp {
namespace {
constexpr std::size_t maximum_players=4096,maximum_name_bytes=128;
// These adjustments reproduce GetGameEntity and the CEntitySA GetPosition thunk
// in the fingerprinted Win32 modules. No foreign C++ objects cross an ABI boundary.
bool native_ped(std::uintptr_t wrapper,std::uintptr_t& ped) {
    ped=0;
    constexpr std::uintptr_t wrapper_vbptr=4,entity_entry=4,entity_vbptr_back=0x14;
    constexpr std::uintptr_t entity_vtordisp_back=4,sa_entity_entry=0x14;
    constexpr std::int64_t sa_adjustment=0x10,interface_back=0x1c;
    std::uintptr_t table{};
    std::int32_t offset{},vtordisp{};
    if (!read(wrapper+wrapper_vbptr,table) || !table || !read(table+entity_entry,offset) || std::abs(static_cast<std::int64_t>(offset))>4096) return false;
    auto entity=static_cast<std::int64_t>(wrapper)+wrapper_vbptr+offset;
    if (entity<0x10000 || entity>std::numeric_limits<std::uint32_t>::max()) return false;
    if (!read(static_cast<std::uintptr_t>(entity)-entity_vtordisp_back,vtordisp) || std::abs(static_cast<std::int64_t>(vtordisp))>4096) return false;
    entity-=vtordisp;
    if (entity<0x10000 || !read(static_cast<std::uintptr_t>(entity)-entity_vbptr_back,table) || !table ||
        !read(table+sa_entity_entry,offset) || std::abs(static_cast<std::int64_t>(offset))>4096) return false;
    const auto field=entity+offset+sa_adjustment-interface_back;
    return field>=0x10000 && field<=std::numeric_limits<std::uint32_t>::max()-4 && read(static_cast<std::uintptr_t>(field),ped) && ped;
}
struct Module {
    HMODULE value{};
    explicit Module(const wchar_t* name) { GetModuleHandleExW(0,name,&value); }
    ~Module() { if (value) FreeLibrary(value); }
};
}
bool read_msvc_name(std::uintptr_t address,std::wstring& name) {
    name.clear();
    struct String { std::array<char,16> storage; std::uint32_t size,capacity; } value{};
    static_assert(sizeof(String)==24);
    if (!read(address,value) || !value.size || value.size>maximum_name_bytes || value.capacity<value.size || value.capacity>0x100000) return false;
    std::array<char,maximum_name_bytes+1> bytes{};
    std::uintptr_t data=address;
    if (value.capacity>=value.storage.size()) std::memcpy(&data,value.storage.data(),sizeof(data));
    if (!read_bytes(data,bytes.data(),value.size+1) || bytes[value.size]!='\0') return false;
    for (std::size_t i=0;i<value.size;++i) if (static_cast<unsigned char>(bytes[i])<32 || bytes[i]==127) return false;
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(value.size),nullptr,0);
    if (count<=0) return false;
    std::wstring decoded(static_cast<std::size_t>(count),L'\0');
    if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(value.size),decoded.data(),count)!=count) return false;
    // Nametags can contain MTA #RRGGBB markup; it must not appear as six extra letters.
    for (std::size_t i=0;i<decoded.size();) {
        if (decoded[i]==L'#' && i+6<decoded.size() &&
            std::all_of(decoded.begin()+static_cast<std::ptrdiff_t>(i+1),decoded.begin()+static_cast<std::ptrdiff_t>(i+7),[](wchar_t c){return std::iswxdigit(c)!=0;})) { i+=7; continue; }
        name.push_back(decoded[i++]);
    }
    return !name.empty();
}
MtaPlayers read_mta_players(std::uintptr_t base,const MtaLayout& layout) {
    MtaPlayers result{};
    const auto reject=[&](const wchar_t* message) {
        result.available=false; result.names.clear(); result.dimensions.clear(); result.cached.clear(); result.status=message;
    };
    std::uintptr_t game{},manager{},begin{},end{};
    if (!read(base+layout.game,game) || !game || !read(game+layout.manager,manager) || !manager ||
        !read(manager+layout.begin,begin) || !read(manager+layout.end,end)) { result.status=L"MTA: aguardando lista de jogadores"; return result; }
    if (end<begin || (end-begin)%sizeof(std::uintptr_t) || (end-begin)/sizeof(std::uintptr_t)>maximum_players) {
        result.status=L"MTA: lista de jogadores invalida"; return result;
    }
    std::vector<std::uintptr_t> players((end-begin)/sizeof(std::uintptr_t));
    if (!players.empty() && (!begin || !read_bytes(begin,players.data(),players.size()*sizeof(std::uintptr_t)))) {
        result.status=L"MTA: falha ao ler jogadores"; return result;
    }
    unsigned failures{};
    for (auto player:players) {
        std::uintptr_t vtable{},wrapper{},ped{};
        if (!player || !read(player,vtable) || vtable!=base+layout.player_vtable ||
            !read(player+layout.player_ped,wrapper)) { ++failures; continue; }
        std::uint16_t dimension{};
        if (layout.dimension && !read(player+layout.dimension,dimension)) { ++failures; continue; }
        if (!wrapper) {
            Vec3 position{};
            if (layout.cached_position && read(player+layout.cached_position,position) && finite(position) &&
                std::abs(position.x)<100000 && std::abs(position.y)<100000 && std::abs(position.z)<100000) {
                std::wstring name;
                read_msvc_name(player+layout.name,name);
                result.cached.push_back({position,std::move(name),dimension});
            }
            continue;
        }
        if (!native_ped(wrapper,ped)) { ++failures; continue; }
        std::wstring name;
        read_msvc_name(player+layout.name,name);
        if (!result.names.emplace(ped,std::move(name)).second) { reject(L"MTA: personagem duplicado na lista"); return result; }
        result.dimensions.emplace(ped,dimension);
    }
    std::uintptr_t current{};
    if (!read(manager+layout.begin,current) || current!=begin || !read(manager+layout.end,current) || current!=end ||
        !read(base+layout.game,current) || current!=game || !read(game+layout.manager,current) || current!=manager) {
        reject(L"MTA: lista mudou durante a leitura"); return result;
    }
    if (failures) { reject(L"MTA: vinculo de jogadores invalido"); return result; }
    result.available=true;
    result.status=L"MTA: nomes "+std::to_wstring(result.names.size());
    return result;
}
bool MtaReader::ready(const MtaLayout& layout) {
    std::lock_guard lock(mutex_);
    return identity_ && identity_->verified && identity_->client_hash==layout.client_sha256 &&
        identity_->game_hash==layout.game_sha256 &&
        identity_->client_base==reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"client.dll")) &&
        identity_->game_base==reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"game_sa.dll"));
}
MtaPlayers MtaReader::collect(const MtaLayout& layout) {
    Module client(L"client.dll"),game(L"game_sa.dll");
    if (!client.value || !game.value) return {false,{},L"MTA: aguardando mod"};
    const auto cb=reinterpret_cast<std::uintptr_t>(client.value),gb=reinterpret_cast<std::uintptr_t>(game.value);
    std::shared_ptr<const Identity> identity;
    { std::lock_guard lock(mutex_); identity=identity_; }
    if (!identity || cb!=identity->client_base || gb!=identity->game_base || layout.client_sha256!=identity->client_hash ||
        layout.game_sha256!=identity->game_hash) return {false,{},L"MTA: aguardando verificacao dos modulos"};
    return identity->verified?read_mta_players(cb,layout):MtaPlayers{false,{},identity->status};
}
void MtaReader::refresh(const MtaLayout& layout) {
    Module client(L"client.dll"),game(L"game_sa.dll");
    if (!client.value || !game.value) { std::lock_guard lock(mutex_); identity_.reset(); return; }
    const auto cb=reinterpret_cast<std::uintptr_t>(client.value),gb=reinterpret_cast<std::uintptr_t>(game.value);
    {
        std::lock_guard lock(mutex_);
        if (identity_ && identity_->checked && cb==identity_->client_base && gb==identity_->game_base &&
            layout.client_sha256==identity_->client_hash && layout.game_sha256==identity_->game_hash) return;
    }
    auto next=std::make_shared<Identity>();
    next->client_base=cb; next->game_base=gb;
    next->client_hash=layout.client_sha256; next->game_hash=layout.game_sha256;
    std::wstring ch,gh;
    // Disk IO and hashing run only on the worker. Publishing holds the mutex briefly.
    next->checked=sha256_file(module_path(client.value),ch) && sha256_file(module_path(game.value),gh);
    next->verified=next->checked && ch==next->client_hash && gh==next->game_hash;
    next->status=!next->checked?L"MTA: falha ao ler hash; nova tentativa pendente":
                 next->verified?L"Modulos MTA confirmados para nomes e vinculos de personagens":L"MTA: hash dos modulos diferente; nomes desativados";
    log_line(next->status);
    std::lock_guard lock(mutex_);
    identity_=std::move(next);
}
}
