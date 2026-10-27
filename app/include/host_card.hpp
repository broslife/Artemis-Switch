//
//  host_card.hpp
//  Artemis
//
//  Host card of the home screen. Holds the logic of the former HostTab
//  (connection state, wake up, rename, stream profiles, removal).
//

#pragma once

#include <Settings.hpp>
#include <borealis.hpp>
#include <cstdint>
#include <optional>

#include "views/glyph_view.hpp"

enum HostState { FETCHING, AVAILABLE, UNAVAILABLE };

class HostCard : public brls::Box {
  public:
    // A preview state skips the network and shows the card in that state.
    HostCard(const Host& host,
             std::optional<HostState> previewState = std::nullopt);

    void reloadHost();

    void onFocusGained() override;
    void onFocusLost() override;

  private:
    Host host;
    std::optional<HostState> previewState;
    HostState state = HostState::FETCHING;
    uint64_t wakeRequestGeneration = 0;
    uint64_t canceledWakeRequestGeneration = 0;

    GlyphView* icon = nullptr;
    brls::Box* badge = nullptr;
    brls::Label* badgeLabel = nullptr;
    brls::Label* nameLabel = nullptr;
    brls::Label* subLabel = nullptr;
    brls::Box* pill = nullptr;
    brls::Label* pillLabel = nullptr;
    bool focused = false;

    void buildLayout();
    void applyStyle();
    void connectOrWake();
    void showOptions();
    void quickEditProfile();
    void quickDeleteProfile();
};
