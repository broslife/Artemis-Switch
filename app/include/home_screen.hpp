//
//  home_screen.hpp
//  Artemis
//
//  First screen of the app: the list of hosts, with access to Settings,
//  Add host and Favorites. It replaces the former MainTabs side menu.
//

#pragma once

#include <borealis.hpp>

#include <vector>

class HomeScreen : public brls::Box {
  public:
    HomeScreen();
    ~HomeScreen() override;

    static brls::View* create();
    static HomeScreen* instance() { return current; }

    void willAppear(bool resetState) override;
    brls::View* getDefaultFocus() override;

    // Rebuilds the host cards from the saved hosts.
    void refreshHosts();
    // Shows or hides the Favorites star after favorites changed.
    void updateFavoritesIfNeeded();

  private:
    inline static HomeScreen* current = nullptr;

    brls::Label* subtitle = nullptr;
    brls::Box* hostList = nullptr;
    brls::Box* gearButton = nullptr;
    brls::Box* addHostButton = nullptr;
    brls::Box* favoritesIcon = nullptr;
    std::vector<brls::View*> cards;
    bool lastHasAnyFavorites = false;

    // The scrolling list traps the focus, so the way out of it (up to the
    // Settings button, down to the action buttons) is set explicitly.
    void updateNavigation();
    void hideFooterStatus();
    void openSettings();
    void openAddHost();
    void openFavorites();
    brls::Box* makeRoundButton(const std::string& glyph,
                               std::function<void()> onClick);
    brls::Box* makeButton(const std::string& glyph, const std::string& text,
                          std::function<void()> onClick);
};
