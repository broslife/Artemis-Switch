//
//  game_carousel.cpp
//  Artemis
//

#include "views/game_carousel.hpp"

#include <algorithm>
#include <cstdlib>

GameCarousel::GameCarousel() : brls::Box(brls::Axis::ROW) {
    setWidth(kStageWidth);
    setHeight(kStageHeight);
}

void GameCarousel::addTile(GameTile* tile) {
    tile->setPositionType(brls::PositionType::ABSOLUTE);
    tile->setOnFocus([this, tile] {
        const auto found = std::find(tiles.begin(), tiles.end(), tile);
        if (found != tiles.end())
            select(static_cast<size_t>(found - tiles.begin()));
    });
    tiles.push_back(tile);
    addView(tile);
    layoutTiles();
}

void GameCarousel::clearTiles() {
    clearViews();
    tiles.clear();
    selectedIndex = 0;
}

brls::View* GameCarousel::getDefaultFocus() {
    if (tiles.empty() || getVisibility() != brls::Visibility::VISIBLE)
        return nullptr;
    return tiles[std::min(selectedIndex, tiles.size() - 1)]->getDefaultFocus();
}

void GameCarousel::select(size_t index) {
    if (index == selectedIndex)
        return;
    selectedIndex = index;
    layoutTiles();
    if (onSelected)
        onSelected(index);
}

void GameCarousel::layoutTiles() {
    const float focusedTop = 12.0f;
    for (size_t i = 0; i < tiles.size(); i++) {
        const int d = static_cast<int>(i) - static_cast<int>(selectedIndex);
        const int distance = std::abs(d);
        const bool focused = d == 0;

        const float w = focused ? kFocusedWidth : kSideWidth;
        const float h = focused ? kFocusedHeight : kSideHeight;

        // Centre of the tile, measured from the middle of the stage. Tiles more
        // than two steps away stay stacked behind the second one, invisible.
        float offset = 0.0f;
        if (distance == 1)
            offset = kFocusedWidth / 2 + kGap + kSideWidth / 2;
        else if (distance >= 2)
            offset = kFocusedWidth / 2 + kGap + kSideWidth + kGap +
                     kSideWidth / 2;
        const float centre = kStageWidth / 2 + (d < 0 ? -offset : offset);

        GameTile* tile = tiles[i];
        tile->setSize(w, h);
        tile->setCornerRadius(focused ? 24 : 18);
        tile->setHighlightCornerRadius(focused ? 24 : 18);
        tile->setPositionLeft(centre - w / 2);
        tile->setPositionTop(focused ? focusedTop
                                     : focusedTop + (kFocusedHeight - kSideHeight) / 2);
        tile->setAlpha(focused ? 1.0f : distance == 1 ? 0.55f
                                      : distance == 2 ? 0.3f
                                                      : 0.0f);
        // The focused tile's name is shown under the carousel instead.
        tile->setLabelVisible(!focused);
    }
}
