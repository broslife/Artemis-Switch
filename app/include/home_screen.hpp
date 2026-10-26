//
//  home_screen.hpp
//  Artemis
//
//  First screen of the app: the list of hosts, with access to Settings,
//  Add host and Favorites. It replaces the former MainTabs side menu.
//

#pragma once

#include <borealis.hpp>

class HomeScreen : public brls::Box {
  public:
    HomeScreen();
    ~HomeScreen() override;

    static brls::View* create();
    static HomeScreen* instance() { return current; }

    void willAppear(bool resetState) override;

    // Rebuilds the host cards from the saved hosts.
    void refreshHosts();
    // Shows or hides the Favorites button after favorites changed.
    void updateFavoritesIfNeeded();

  private:
    inline static HomeScreen* current = nullptr;

    brls::Label* subtitle = nullptr;
    brls::Box* hostList = nullptr;
    brls::Box* favoritesButton = nullptr;
    bool lastHasAnyFavorites = false;

    void openSettings();
    void openAddHost();
    void openFavorites();
    brls::Box* makeButton(const std::string& glyph, const std::string& text,
                          std::function<void()> onClick);
};
