//
//  app_list_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 26.05.2021.
//

#pragma once

#include "grid_view.hpp"
#include "loading_overlay.hpp"
#include "views/game_carousel.hpp"
#include "views/game_tile.hpp"
#include <Settings.hpp>
#include <borealis.hpp>
#include "GameStreamClient.hpp"

#include <optional>
#include <string>
#include <vector>

using namespace brls;

// Games of a host, as a carousel (default) or a grid, plus the Host settings
// pane. Y switches between carousel and grid.
class AppListView : public Box {
  public:
    AppListView(const Host& host);

    void onLayout() override;
    void willAppear(bool resetState) override;
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              Style style, FrameContext* ctx) override;

  private:
    enum class Pane { Applications, Host };
    enum class ViewMode { Carousel, Grid };

    Host host;
    std::string hostProfileKey;
    DetailCell* hostIntegration = nullptr;
    DetailCell* webConfig = nullptr;
    DetailCell* streamProfile = nullptr;
    DetailCell* virtualDisplay = nullptr;
    DetailCell* loadMoreApps = nullptr;

    // Header.
    Box* backButton = nullptr;
    Label* searchLabel = nullptr;
    Box* viewSwitch = nullptr;
    Box* carouselSwitchCell = nullptr;
    Box* gridSwitchCell = nullptr;
    GlyphView* carouselSwitchIcon = nullptr;
    GlyphView* gridSwitchIcon = nullptr;
    Box* hostSettingsButton = nullptr;

    // Applications pane.
    Box* appsContainer = nullptr;
    Box* carouselContainer = nullptr;
    GameCarousel* carousel = nullptr;
    Label* infoName = nullptr;
    Label* infoMeta = nullptr;
    Box* ctaButton = nullptr;
    Label* ctaLabel = nullptr;
    Box* gridContainer = nullptr;
    Box* appsScrollContent = nullptr;
    ScrollingFrame* appsScroll = nullptr;
    GridView* gridView = nullptr;

    // Host settings pane.
    Box* hostContainer = nullptr;

    Pane activePane = Pane::Applications;
    std::optional<ActionIdentifier> searchAction;
    ViewMode viewMode = ViewMode::Carousel;
    std::optional<AppInfo> currentApp;
    AppInfoList cachedApps;
    std::vector<AppInfo> shownApps;
    std::string appSearchQuery;
    size_t visibleAppLimit = 28;
    size_t displayedAppCount = 0;
    size_t filteredAppCount = 0;
    int cachedCurrentGame = 0;
    bool loading = false;
    bool inputBlocked = false;
    bool streamWasActive = false;
    bool pendingPostStreamRefresh = false;
    artemis::host::HostIdentity detectedHostIdentity;
    std::string detectedHostAddress;
    LoadingOverlay* loader = nullptr;
    void blockInput(bool block);

    void showPane(Pane pane, bool focusContent);
    void applyViewMode();
    void toggleViewMode();
    void clearAppViews();
    void updateCarouselInfo(size_t index);
    void setCurrentApp(const AppInfo& app);
    void terninateApp();
    void updateAppList();
    void loadPreviewApps();
    void rebuildAppGrid();
    void loadNextAppPage();
    void resetAppPagination();
    void promptAppSearch();
    void refreshAppSearchLabel();
    void updateFavoriteAction(GameTile* tile, Host host, const AppInfo& app);
    void refreshStreamProfileLabel();
    void refreshWebConfigVisibility();
    void refreshHostIntegration(const SERVER_DATA& server);
    void refreshVirtualDisplayLabel();
};
