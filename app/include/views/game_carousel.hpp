//
//  game_carousel.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

#include <functional>
#include <vector>

#include "views/game_tile.hpp"

// Row of game tiles where the focused one is large and in the middle, with two
// smaller, fading tiles on each side. Moving the focus slides the row.
class GameCarousel : public brls::Box {
  public:
    // Focused tile 300x400, the others 210x280 (both 3:4), as in the prototype.
    static constexpr float kFocusedWidth = 300.0f;
    static constexpr float kFocusedHeight = 400.0f;
    static constexpr float kSideWidth = 210.0f;
    static constexpr float kSideHeight = 280.0f;
    static constexpr float kGap = 28.0f;
    static constexpr float kStageWidth = 1280.0f;
    static constexpr float kStageHeight = 432.0f;

    GameCarousel();

    void addTile(GameTile* tile);
    void clearTiles();

    size_t size() const { return tiles.size(); }
    GameTile* tileAt(size_t index) const { return tiles[index]; }
    size_t selected() const { return selectedIndex; }

    // Called with the index of the tile that just got the focus.
    void setOnSelected(std::function<void(size_t)> callback) {
        onSelected = std::move(callback);
    }

    // The tile that should take the focus when the carousel is entered.
    brls::View* getDefaultFocus() override;

  private:
    std::vector<GameTile*> tiles;
    size_t selectedIndex = 0;
    std::function<void(size_t)> onSelected;

    void select(size_t index);
    void layoutTiles();
};
