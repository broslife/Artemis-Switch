//
//  favorite_tab.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.09.2021.
//

#pragma once

#include "grid_view.hpp"
#include <Settings.hpp>
#include <borealis.hpp>

// Favorites screen: games grouped by host, with a header that goes back.
class FavoriteTab : public brls::Box {
  public:
    FavoriteTab();

    void willAppear(bool resetState) override;
    void refreshIfNeeded();
    void setRefreshNeeded() { isDirty = true; }
    static brls::View* create();

  private:
    brls::Box* container = nullptr;
    brls::Box* backButton = nullptr;
    brls::Label* countLabel = nullptr;

    bool isDirty = false;

    void updateAppList();
    // The scrolling list can trap the focus, so the way up to the back button
    // is set explicitly.
    void updateNavigation(const std::vector<brls::View*>& firstRow);
};
