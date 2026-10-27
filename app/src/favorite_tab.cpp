//
//  favorite_tab.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.09.2021.
//

#include "favorite_tab.hpp"
#include "UiPreview.hpp"
#include "forwarder_maker.hpp"
#include "home_screen.hpp"
#include "views/game_tile.hpp"
#include "views/footer_status.hpp"
#include "views/glyph_view.hpp"
#include <cstdlib>
#include <string>

using namespace brls;
using namespace brls::literals;

namespace {
constexpr float kSideMargin = 56.0f;
constexpr size_t kColumns = 5;

std::vector<Host> favoriteHosts() {
    if (artemis::ui_preview::enabled()) {
        std::vector<Host> hosts;
        for (const auto& mock : artemis::ui_preview::hosts())
            hosts.push_back(mock.host);
        return hosts;
    }
    return Settings::instance().hosts();
}
} // namespace

FavoriteTab::FavoriteTab() {
    setAxis(Axis::COLUMN);
    setWidth(View::AUTO);
    setHeight(View::AUTO);
    setGrow(1.0f);

    Theme theme = Application::getTheme();

    // Header: back button, star and title on the left; game count on the right.
    auto* header = new Box(Axis::ROW);
    header->setHeight(88);
    header->setAlignItems(AlignItems::CENTER);
    header->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    header->setPadding(0, kSideMargin, 0, kSideMargin);

    auto* left = new Box(Axis::ROW);
    left->setAlignItems(AlignItems::CENTER);

    backButton = new Box(Axis::ROW);
    backButton->setWidth(56);
    backButton->setHeight(56);
    backButton->setCornerRadius(28);
    backButton->setHighlightCornerRadius(28);
    backButton->setBackgroundColor(theme["artemis/card"]);
    backButton->setBorderColor(theme["artemis/card_line"]);
    backButton->setBorderThickness(2);
    backButton->setJustifyContent(JustifyContent::CENTER);
    backButton->setAlignItems(AlignItems::CENTER);
    backButton->setMarginRight(20);
    backButton->setFocusable(true);
    backButton->addView(new GlyphView("", 38, theme["brls/text"]));
    backButton->addGestureRecognizer(new TapGestureRecognizer(backButton));
    backButton->registerClickAction([](View*) {
        Application::popActivity();
        return true;
    });
    left->addView(backButton);

    auto* star = new GlyphView("", 34, theme["brls/accent"]);
    star->setMarginRight(12);
    left->addView(star);
    auto* title = new Label();
    title->setText("tabs/favorites"_i18n);
    title->setFontSize(32);
    left->addView(title);
    header->addView(left);

    countLabel = new Label();
    countLabel->setFontSize(22);
    countLabel->setTextColor(theme["artemis/muted"]);
    header->addView(countLabel);
    addView(header);

    // Groups. Centered scrolling keeps the focus free to leave the list.
    auto* scroll = new ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setScrollingBehavior(ScrollingBehavior::CENTERED);
    container = new Box(Axis::COLUMN);
    container->setHideHighlight(true);
    container->setPadding(4, kSideMargin, 48, kSideMargin);
    scroll->setContentView(container);
    addView(scroll);

    updateAppList();
}

void FavoriteTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    artemis::ui::hideFooterStatus(this);
}

void FavoriteTab::refreshIfNeeded() {
    if (isDirty) {
        isDirty = false;
        updateAppList();
    }
}

void FavoriteTab::updateNavigation(const std::vector<View*>& firstRow) {
    for (View* tile : firstRow)
        tile->setCustomNavigationRoute(FocusDirection::UP, backButton);
    if (!firstRow.empty())
        backButton->setCustomNavigationRoute(FocusDirection::DOWN,
                                             firstRow.front());
}

void FavoriteTab::updateAppList() {
    container->clearViews();
    std::vector<View*> firstRow;
    size_t total = 0;

    for (const Host& host : favoriteHosts()) {
        if (host.favorites.empty())
            continue;

        auto* header = new Box(Axis::ROW);
        header->setAlignItems(AlignItems::CENTER);
        header->setMarginBottom(16);
        if (!container->getChildren().empty())
            header->setMarginTop(36);

        auto* bar = new Box(Axis::ROW);
        bar->setWidth(6);
        bar->setHeight(28);
        bar->setCornerRadius(3);
        bar->setBackgroundColor(nvgRGB(140, 150, 163));
        bar->setMarginRight(12);
        header->addView(bar);

        auto* hostName = new Label();
        hostName->setText(host.hostname);
        hostName->setFontSize(26);
        header->addView(hostName);
        container->addView(header);

        auto* gridView = new GridView(kColumns);
        const bool isFirstGroup = container->getChildren().size() == 1;

        for (const App& app : host.favorites) {
            AppInfo info{app.name, app.app_id};
            auto* cell = new GameTile(host, info, GameTile::kGridWidth,
                                    GameTile::kGridHeight);
            gridView->addView(cell);
            total++;
            if (isFirstGroup && firstRow.size() < kColumns)
                firstRow.push_back(cell);

#if defined(PLATFORM_SWITCH) || defined(PLATFORM_IOS)
            cell->registerAction(
                "forwarder/make"_i18n, BUTTON_X,
                [host, app](View* view) {
#ifdef PLATFORM_IOS
                    makeForwarder(host, app, false);
#else
                    const int rc = makeForwarder(host, app, false);
                    const std::string message =
                        rc == EXIT_SUCCESS
                            ? "forwarder/installed"_i18n
                            : brls::getStr("forwarder/install_failed", rc);

                    auto dialog = new Dialog(message);
                    dialog->addButton("common/cancel"_i18n, [](){});
                    dialog->open();
#endif
                    return true;
            });
#endif

            cell->registerAction(
                "app_list/unstar"_i18n, BUTTON_Y,
                [this, gridView, cell, host, app](View* view) {
                    int index = gridView->getItemIndex(cell);
                    int section = *((int*)gridView->getParentUserData());

                    Settings::instance().remove_favorite(host, app.app_id);
                    this->updateAppList();

                    if (section >= container->getChildren().size())
                        section = (int)container->getChildren().size() - 2;

                    if (section <= 0) {
                        if (HomeScreen::instance())
                            HomeScreen::instance()->updateFavoritesIfNeeded();
                        // Nothing left to show: go back to the home screen.
                        if (container->getChildren().empty())
                            Application::popActivity();
                        return true;
                    }

                    int newIndex = index;
                    int items =
                        (int)((GridView*)container->getChildren()[section])
                            ->getChildren()
                            .size();
                    if (newIndex >= items)
                        newIndex = items - 1;

                    if (newIndex < 0)
                        return true;

                    View* focus = ((GridView*)container->getChildren()[section])
                                      ->getChildren()[newIndex];
                    Application::giveFocus(focus);

                    return true;
                });
        }

        container->addView(gridView);
    }

    countLabel->setText(brls::getStr("favorites/games", total));
    updateNavigation(firstRow);
}

brls::View* FavoriteTab::create() { return new FavoriteTab(); }
