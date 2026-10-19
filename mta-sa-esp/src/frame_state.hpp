#pragma once
#include "game.hpp"
#include <memory>

namespace esp {
struct PublishedFrame {
    std::shared_ptr<const Frame> frame;
    ViewMatrix view{};
    std::shared_ptr<const Settings> settings;
    HWND target{};
    ULONGLONG timestamp{};
    ULONGLONG collected_at{};
    bool windowed{};
    AimVisual aim;
    bool matches(const std::shared_ptr<const Settings>& current,ULONGLONG now,std::uint32_t maximum_age) const noexcept {
        return settings && current && settings==current && now>=timestamp && now-timestamp<=maximum_age;
    }
    bool needs_collection(const std::shared_ptr<const Settings>& current,ULONGLONG now) const noexcept {
        return !current || !current->collect_interval_ms || !settings || settings!=current ||
            now<collected_at || now-collected_at>=current->collect_interval_ms;
    }
};
}
