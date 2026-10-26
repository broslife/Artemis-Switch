//
//  home_screen.cpp
//  Artemis
//

#include "home_screen.hpp"
#include "Settings.hpp"
#include "add_host_tab.hpp"
#include "favorite_tab.hpp"
#include "host_card.hpp"
#include "settings_screen.hpp"
#include "views/glyph_view.hpp"
#include "views/status_bar.hpp"

using namespace brls::literals;

namespace {
constexpr float kSideMargin = 56.0f;
constexpr int kCardsPerRow = 3;

void pushFramedActivity(brls::View* content, const std::string& title) {
    auto* frame = new brls::AppletFrame(content);
    if (title.empty())
        frame->setHeaderVisibility(brls::Visibility::GONE);
    else
        frame->setTitle(title);
    brls::Application::pushActivity(new brls::Activity(frame));
}
} // namespace

HomeScreen::HomeScreen() {
    current = this;
    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);

    brls::Theme theme = brls::Application::getTheme();

    // Header: logo and name on the left; Settings, battery and clock on the right.
    auto* header = new brls::Box(brls::Axis::ROW);
    header->setHeight(80);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    header->setPadding(0, kSideMargin, 0, kSideMargin);

    auto* brand = new brls::Box(brls::Axis::ROW);
    brand->setAlignItems(brls::AlignItems::CENTER);
    auto* logo = new brls::Image();
    logo->setImageFromRes("img/moonlight_96.png");
    logo->setWidth(40);
    logo->setHeight(40);
    logo->setMarginRight(14);
    brand->addView(logo);
    auto* name = new brls::Label();
    name->setText("title"_i18n);
    name->setFontSize(26);
    brand->addView(name);
    header->addView(brand);

    auto* right = new brls::Box(brls::Axis::ROW);
    right->setAlignItems(brls::AlignItems::CENTER);

    auto* gear = new brls::Box(brls::Axis::ROW);
    gear->setWidth(56);
    gear->setHeight(56);
    gear->setCornerRadius(28);
    gear->setHighlightCornerRadius(28);
    gear->setBackgroundColor(theme["artemis/card"]);
    gear->setBorderColor(theme["artemis/card_line"]);
    gear->setBorderThickness(2);
    gear->setJustifyContent(brls::JustifyContent::CENTER);
    gear->setAlignItems(brls::AlignItems::CENTER);
    gear->setMarginRight(24);
    gear->setFocusable(true);
    gear->addView(new GlyphView("", 32, theme["brls/text"]));
    gear->addGestureRecognizer(new brls::TapGestureRecognizer(gear));
    gear->registerClickAction([this](brls::View*) {
        openSettings();
        return true;
    });
    right->addView(gear);
    right->addView(new StatusBar());
    header->addView(right);
    addView(header);

    // Title block.
    auto* heading = new brls::Box(brls::Axis::COLUMN);
    heading->setPadding(8, kSideMargin, 0, kSideMargin);
    auto* title = new brls::Label();
    title->setText("home/title"_i18n);
    title->setFontSize(48);
    heading->addView(title);
    subtitle = new brls::Label();
    subtitle->setFontSize(24);
    subtitle->setMarginTop(8);
    subtitle->setTextColor(theme["artemis/muted"]);
    heading->addView(subtitle);
    addView(heading);

    // Host cards, scrollable when there are more than one row.
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setMarginTop(24);
    hostList = new brls::Box(brls::Axis::COLUMN);
    hostList->setPadding(8, kSideMargin, 8, kSideMargin);
    scroll->setContentView(hostList);
    addView(scroll);

    // Actions.
    auto* actions = new brls::Box(brls::Axis::ROW);
    actions->setPadding(12, kSideMargin, 20, kSideMargin);
    actions->addView(makeButton("", "tabs/add_host"_i18n,
                                [this] { openAddHost(); }));
    favoritesButton = makeButton("", "tabs/favorites"_i18n,
                                 [this] { openFavorites(); });
    favoritesButton->setMarginLeft(24);
    actions->addView(favoritesButton);
    addView(actions);

    registerAction("tabs/add_host"_i18n, brls::ControllerButton::BUTTON_X,
                   [this](brls::View*) {
                       openAddHost();
                       return true;
                   });
    registerAction("tabs/settings"_i18n, brls::ControllerButton::BUTTON_START,
                   [this](brls::View*) {
                       openSettings();
                       return true;
                   });

    refreshHosts();
    lastHasAnyFavorites = Settings::instance().has_any_favorite();
    favoritesButton->setVisibility(lastHasAnyFavorites
                                       ? brls::Visibility::VISIBLE
                                       : brls::Visibility::GONE);
}

HomeScreen::~HomeScreen() {
    if (current == this)
        current = nullptr;
}

brls::Box* HomeScreen::makeButton(const std::string& glyph,
                                  const std::string& text,
                                  std::function<void()> onClick) {
    brls::Theme theme = brls::Application::getTheme();
    auto* button = new brls::Box(brls::Axis::ROW);
    button->setHeight(84);
    button->setGrow(1.0f);
    button->setCornerRadius(20);
    button->setHighlightCornerRadius(20);
    button->setBorderColor(theme["artemis/card_line"]);
    button->setBorderThickness(3);
    button->setJustifyContent(brls::JustifyContent::CENTER);
    button->setAlignItems(brls::AlignItems::CENTER);
    button->setFocusable(true);
    button->addView(new GlyphView(glyph, 32, theme["brls/text"]));
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(26);
    label->setMarginLeft(14);
    button->addView(label);
    button->addGestureRecognizer(new brls::TapGestureRecognizer(button));
    button->registerClickAction([onClick](brls::View*) {
        onClick();
        return true;
    });
    return button;
}

void HomeScreen::willAppear(bool resetState) {
    Box::willAppear(resetState);
    updateFavoritesIfNeeded();
}

void HomeScreen::refreshHosts() {
    hostList->clearViews();

    const auto hosts = Settings::instance().hosts();
    subtitle->setText(hosts.empty()
                          ? "home/empty"_i18n
                          : brls::getStr("home/subtitle", hosts.size()));

    brls::Box* row = nullptr;
    for (size_t i = 0; i < hosts.size(); i++) {
        if (i % kCardsPerRow == 0) {
            row = new brls::Box(brls::Axis::ROW);
            row->setMarginBottom(24);
            hostList->addView(row);
        }
        auto* card = new HostCard(hosts[i]);
        if (i % kCardsPerRow != kCardsPerRow - 1)
            card->setMarginRight(32);
        row->addView(card);
    }
}

void HomeScreen::updateFavoritesIfNeeded() {
    const bool has = Settings::instance().has_any_favorite();
    if (has == lastHasAnyFavorites)
        return;
    lastHasAnyFavorites = has;
    favoritesButton->setVisibility(has ? brls::Visibility::VISIBLE
                                       : brls::Visibility::GONE);
}

void HomeScreen::openSettings() {
    pushFramedActivity(new SettingsScreen(), "");
}

void HomeScreen::openAddHost() {
    pushFramedActivity(new AddHostTab(), "tabs/add_host"_i18n);
}

void HomeScreen::openFavorites() {
    pushFramedActivity(new FavoriteTab(), "tabs/favorites"_i18n);
}

brls::View* HomeScreen::create() { return new HomeScreen(); }
