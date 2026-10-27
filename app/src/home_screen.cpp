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
#include "views/footer_status.hpp"
#include "views/glyph_view.hpp"
#include "views/status_bar.hpp"
#include "UiPreview.hpp"

using namespace brls::literals;

namespace {
bool hasAnyFavorite() {
    return artemis::ui_preview::enabled() ||
           Settings::instance().has_any_favorite();
}

constexpr float kSideMargin = 56.0f;
constexpr size_t kCardsPerRow = 3;

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

    favoritesIcon = makeRoundButton("", [this] { openFavorites(); });
    right->addView(favoritesIcon);
    gearButton = makeRoundButton("", [this] { openSettings(); });
    right->addView(gearButton);
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
    addHostButton = makeButton("", "tabs/add_host"_i18n,
                               [this] { openAddHost(); });
    actions->addView(addHostButton);
    addView(actions);

    registerAction("tabs/settings"_i18n, brls::ControllerButton::BUTTON_START,
                   [this](brls::View*) {
                       openSettings();
                       return true;
                   });

    refreshHosts();
    lastHasAnyFavorites = hasAnyFavorite();
    favoritesIcon->setVisibility(lastHasAnyFavorites
                                       ? brls::Visibility::VISIBLE
                                       : brls::Visibility::GONE);
}

HomeScreen::~HomeScreen() {
    if (current == this)
        current = nullptr;
}

brls::Box* HomeScreen::makeRoundButton(const std::string& glyph,
                                       std::function<void()> onClick) {
    brls::Theme theme = brls::Application::getTheme();
    auto* button = new brls::Box(brls::Axis::ROW);
    button->setWidth(56);
    button->setHeight(56);
    button->setCornerRadius(28);
    button->setHighlightCornerRadius(28);
    button->setBackgroundColor(theme["artemis/card"]);
    button->setBorderColor(theme["artemis/card_line"]);
    button->setBorderThickness(2);
    button->setJustifyContent(brls::JustifyContent::CENTER);
    button->setAlignItems(brls::AlignItems::CENTER);
    button->setMarginRight(24);
    button->setFocusable(true);
    button->addView(new GlyphView(glyph, 32, theme["brls/text"]));
    button->addGestureRecognizer(new brls::TapGestureRecognizer(button));
    button->registerClickAction([onClick](brls::View*) {
        onClick();
        return true;
    });
    return button;
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
    hideFooterStatus();
    updateFavoritesIfNeeded();
}

brls::View* HomeScreen::getDefaultFocus() {
    if (!cards.empty())
        return cards.front()->getDefaultFocus();
    return addHostButton->getDefaultFocus();
}

void HomeScreen::hideFooterStatus() { artemis::ui::hideFooterStatus(this); }

void HomeScreen::updateNavigation() {
    using brls::FocusDirection;

    gearButton->setCustomNavigationRoute(
        FocusDirection::DOWN, cards.empty() ? addHostButton : cards.front());

    // First row goes up to Settings; the last row goes down to the buttons.
    brls::View* lastRowStart =
        cards.empty() ? gearButton
                      : cards[(cards.size() - 1) / kCardsPerRow * kCardsPerRow];
    for (size_t i = 0; i < cards.size(); i++) {
        if (i < kCardsPerRow)
            cards[i]->setCustomNavigationRoute(FocusDirection::UP, gearButton);
        if (i >= (cards.size() - 1) / kCardsPerRow * kCardsPerRow)
            cards[i]->setCustomNavigationRoute(FocusDirection::DOWN,
                                               addHostButton);
    }
    addHostButton->setCustomNavigationRoute(FocusDirection::UP, lastRowStart);
    favoritesIcon->setCustomNavigationRoute(
        FocusDirection::DOWN, cards.empty() ? addHostButton : cards.front());
}

void HomeScreen::refreshHosts() {
    hostList->clearViews();
    cards.clear();

    std::vector<Host> hosts;
    std::vector<std::optional<HostState>> previewStates;
    if (artemis::ui_preview::enabled()) {
        for (const auto& mock : artemis::ui_preview::hosts()) {
            hosts.push_back(mock.host);
            using State = artemis::ui_preview::State;
            previewStates.push_back(mock.state == State::Available
                                        ? AVAILABLE
                                    : mock.state == State::Unavailable
                                        ? UNAVAILABLE
                                        : FETCHING);
        }
    } else {
        hosts = Settings::instance().hosts();
        previewStates.assign(hosts.size(), std::nullopt);
    }

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
        auto* card = new HostCard(hosts[i], previewStates[i]);
        if (i % kCardsPerRow != kCardsPerRow - 1)
            card->setMarginRight(32);
        row->addView(card);
        cards.push_back(card);
    }
    updateNavigation();
}

void HomeScreen::updateFavoritesIfNeeded() {
    const bool has = hasAnyFavorite();
    if (has == lastHasAnyFavorites)
        return;
    lastHasAnyFavorites = has;
    favoritesIcon->setVisibility(has ? brls::Visibility::VISIBLE
                                       : brls::Visibility::GONE);
}

void HomeScreen::openSettings() {
    SettingsScreen::open();
}

void HomeScreen::openAddHost() {
    pushFramedActivity(new AddHostTab(), "tabs/add_host"_i18n);
}

void HomeScreen::openFavorites() {
    pushFramedActivity(new FavoriteTab(), "");
}

brls::View* HomeScreen::create() { return new HomeScreen(); }
