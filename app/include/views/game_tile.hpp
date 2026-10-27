//
//  game_tile.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

#include <functional>

#include "GameStreamClient.hpp"
#include "Settings.hpp"
#include "views/glyph_view.hpp"

// Portrait game tile (3:4): box art with the name over the bottom edge, a
// "Running" badge and a star for favorites. Selecting it starts the stream.
// Used by the Favorites screen, the games grid and the games carousel.
class GameTile : public brls::Box {
  public:
    // Five tiles plus four 12 px GridView gaps fill the 1168 px content width.
    static constexpr float kGridWidth = 224.0f;
    static constexpr float kGridHeight = 298.0f;

    // currentApp is the app running on the host (0 if none): that tile shows
    // "Running" and every other tile is dimmed and cannot be started.
    GameTile(const Host& host, const AppInfo& app, float width, float height,
             int currentApp = 0);

    void setSize(float width, float height);
    void setFavorite(bool favorite);
    void setLabelVisible(bool visible);
    // Called when the tile gets the focus (the carousel follows it).
    void setOnFocus(std::function<void()> callback) {
        onFocus = std::move(callback);
    }

    void onFocusGained() override;

  private:
    brls::Image* image = nullptr;
    brls::Box* labelBox = nullptr;
    brls::Box* badge = nullptr;
    GlyphView* favoriteIcon = nullptr;
    brls::Box* dimLayer = nullptr;
    std::function<void()> onFocus;
};
