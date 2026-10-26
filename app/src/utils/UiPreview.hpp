//
//  UiPreview.hpp
//  Artemis
//
//  UI preview mode for desktop builds: the screens run with made-up hosts and
//  no network access, so layout, focus and navigation can be checked without
//  a Switch. It is switched on by the ARTEMIS_UI_PREVIEW environment variable
//  and is always off on the Switch.
//

#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#include "Settings.hpp"

namespace artemis::ui_preview {

enum class State { Fetching, Available, Unavailable };

struct MockHost {
    Host host;
    State state;
};

inline bool enabled() {
#ifdef __SWITCH__
    return false;
#else
    static const bool value = [] {
        const char* flag = std::getenv("ARTEMIS_UI_PREVIEW");
        return flag != nullptr && std::string(flag) == "1";
    }();
    return value;
#endif
}

inline std::vector<MockHost> hosts() {
    const auto make = [](const char* name, const char* address, State state,
                         std::vector<App> favorites = {}) {
        Host host;
        host.hostname = name;
        host.address = address;
        host.favorites = std::move(favorites);
        host.ensure_endpoints();
        return MockHost{host, state};
    };
    return {
        make("XITRIX", "192.168.0.10", State::Available,
             {{"Genshin Impact", 1},
              {"No Man's Sky", 2},
              {"Steam", 3},
              {"Elder Scrolls Online", 4},
              {"Grand Theft Auto V", 5},
              {"Sea of Thieves", 6}}),
        make("LIVING-ROOM-PC", "192.168.0.11", State::Available,
             {{"Crimson Desert", 7}, {"Minecraft", 8}}),
        make("BEDROOM-LAPTOP", "192.168.0.12", State::Unavailable),
        make("OFFICE-WORKSTATION-WITH-A-LONG-NAME", "192.168.0.13",
             State::Fetching),
        make("STEAM-DECK", "192.168.0.14", State::Available),
    };
}

} // namespace artemis::ui_preview
