#pragma once
#include <cstdint>

namespace esp {
class StartupGate {
public:
    enum class State { Modules,Player,Device,Settling,Ready };
    static constexpr std::uint64_t settle_ms=1000;
    State sample(bool modules,bool player,std::uintptr_t device,std::uintptr_t table,std::uint64_t now) noexcept {
        if (!modules || !player || !device || !table) {
            device_=table_=0;
            since_=0;
            return !modules?State::Modules:!player?State::Player:State::Device;
        }
        if (device!=device_ || table!=table_ || now<since_) {
            device_=device; table_=table; since_=now;
            return State::Settling;
        }
        return now-since_>=settle_ms?State::Ready:State::Settling;
    }
private:
    std::uintptr_t device_{},table_{};
    std::uint64_t since_{};
};
}
