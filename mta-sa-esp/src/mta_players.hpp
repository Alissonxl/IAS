#pragma once
#include "config.hpp"
#include <unordered_map>
#include "math.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace esp {
struct MtaPlayers {
    bool available{};
    std::unordered_map<std::uintptr_t,std::wstring> names;
    std::wstring status;
    std::unordered_map<std::uintptr_t,std::uint16_t> dimensions{};
    struct Cached { Vec3 position; std::wstring name; std::uint16_t dimension{}; };
    std::vector<Cached> cached{};
};
MtaPlayers read_mta_players(std::uintptr_t client_base,const MtaLayout& layout);
bool read_msvc_name(std::uintptr_t address,std::wstring& name);
class MtaReader {
public:
    MtaPlayers collect(const MtaLayout& layout);
    void refresh(const MtaLayout& layout);
    bool ready(const MtaLayout& layout);
private:
    struct Identity {
        std::uintptr_t client_base{},game_base{};
        std::wstring client_hash,game_hash,status;
        bool checked{},verified{};
    };
    std::mutex mutex_;
    std::shared_ptr<const Identity> identity_;
};
}
