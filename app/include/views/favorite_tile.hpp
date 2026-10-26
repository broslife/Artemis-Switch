//
//  favorite_tile.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

#include "GameStreamClient.hpp"
#include "Settings.hpp"

// Portrait game tile of the Favorites screen: box art with the name over the
// bottom edge. Selecting it starts the stream, like AppCell does.
class FavoriteTile : public brls::Box {
  public:
    // Five tiles plus four 12 px GridView gaps fill the 1168 px content width.
    static constexpr float kWidth = 224.0f;
    static constexpr float kHeight = 298.0f;

    FavoriteTile(const Host& host, const AppInfo& app);

  private:
    brls::Image* image = nullptr;
};
