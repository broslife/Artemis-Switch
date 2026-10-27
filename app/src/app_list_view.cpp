//
//  app_list_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "app_list_view.hpp"
#include "helper.hpp"
#include "home_screen.hpp"
#include "settings_screen.hpp"
#include "UiPreview.hpp"
#include "MoonlightSession.hpp"
#include "features/ui/QrCodeView.hpp"
#include "features/ui/AppLibraryPaging.hpp"
#include "features/apollo/ApolloHostOptions.hpp"
#include "features/apollo/ApolloHostOptionsStore.hpp"
#include "host/GameStreamHostCapabilities.hpp"
#include "host/HostIdentityProbe.hpp"
#include "streaming/HostProfileKey.hpp"
#include "streaming/HostStreamProfileUi.hpp"
#include "streaming/ProfileEditorDialog.hpp"
#include "streaming/StreamConfigProfileStore.hpp"
#include "streaming/StreamDisconnectPolicy.hpp"
#include "streaming/StreamUiLifecycle.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>
#include <unordered_map>
#include <vector>

namespace {
// The carousel/grid choice lasts until the app closes.
bool lastGridMode = false;

std::string lowercaseCopy(std::string value) {
    for (char& ch : value)
        ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

bool hostActionsBlockedByActiveSession() {
    return MoonlightSession::activeSession() != nullptr;
}

const char* virtualDisplayTargetLabelKey(
    artemis::apollo::VirtualDisplayTarget target) {
    using artemis::apollo::VirtualDisplayTarget;
    switch (target) {
        case VirtualDisplayTarget::Off: return "artemis/overlay/vd_off";
        case VirtualDisplayTarget::CurrentProfile: return "artemis/overlay/vd_current";
        case VirtualDisplayTarget::Handheld: return "artemis/overlay/vd_handheld";
        case VirtualDisplayTarget::Docked: return "artemis/overlay/vd_docked";
        case VirtualDisplayTarget::PortraitHandheld: return "artemis/overlay/vd_portrait_handheld";
        case VirtualDisplayTarget::PortraitDocked: return "artemis/overlay/vd_portrait_docked";
        case VirtualDisplayTarget::Custom: return "artemis/overlay/vd_custom";
    }
    return "artemis/overlay/vd_off";
}
} // namespace

AppListView::AppListView(const Host& host) : Box(Axis::COLUMN), host(host) {
    hostProfileKey = artemis::streaming::host_profile_key(host);
    setAlignItems(AlignItems::STRETCH);
    viewMode = lastGridMode ? ViewMode::Grid : ViewMode::Carousel;

    Theme theme = Application::getTheme();

    // Header: back and host name on the left; carousel/grid switch and Host
    // settings on the right.
    auto* header = new Box(Axis::ROW);
    header->setHeight(88);
    header->setAlignItems(AlignItems::CENTER);
    header->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    header->setPadding(0, 56, 0, 56);

    auto* left = new Box(Axis::ROW);
    left->setAlignItems(AlignItems::CENTER);
    backButton = new Box(Axis::ROW);
    backButton->setAlignItems(AlignItems::CENTER);
    backButton->setHeight(56);
    backButton->setCornerRadius(16);
    backButton->setHighlightCornerRadius(16);
    backButton->setPadding(0, 16, 0, 8);
    backButton->setFocusable(true);
    backButton->addView(new GlyphView("", 38, theme["brls/text"]));
    auto* hostName = new Label();
    hostName->setText(host.hostname);
    hostName->setFontSize(26);
    hostName->setMarginLeft(8);
    backButton->addView(hostName);
    backButton->addGestureRecognizer(new TapGestureRecognizer(backButton));
    backButton->registerClickAction([this](View*) {
        this->dismiss();
        return true;
    });
    left->addView(backButton);
    searchLabel = new Label();
    searchLabel->setFontSize(22);
    searchLabel->setTextColor(theme["brls/accent"]);
    searchLabel->setMarginLeft(8);
    left->addView(searchLabel);
    header->addView(left);

    auto* right = new Box(Axis::ROW);
    right->setAlignItems(AlignItems::CENTER);

    viewSwitch = new Box(Axis::ROW);
    viewSwitch->setPadding(4);
    viewSwitch->setCornerRadius(16);
    viewSwitch->setHighlightCornerRadius(16);
    viewSwitch->setBackgroundColor(theme["artemis/card"]);
    viewSwitch->setBorderColor(theme["artemis/card_line"]);
    viewSwitch->setBorderThickness(2);
    viewSwitch->setFocusable(true);
    auto makeSwitchCell = [&](const char* glyph, GlyphView*& icon) {
        auto* cell = new Box(Axis::ROW);
        cell->setWidth(64);
        cell->setHeight(48);
        cell->setCornerRadius(12);
        cell->setJustifyContent(JustifyContent::CENTER);
        cell->setAlignItems(AlignItems::CENTER);
        icon = new GlyphView(glyph, 32, theme["artemis/muted"]);
        cell->addView(icon);
        viewSwitch->addView(cell);
        return cell;
    };
    carouselSwitchCell = makeSwitchCell("", carouselSwitchIcon);
    gridSwitchCell = makeSwitchCell("", gridSwitchIcon);
    viewSwitch->addGestureRecognizer(new TapGestureRecognizer(viewSwitch));
    viewSwitch->registerClickAction([this](View*) {
        toggleViewMode();
        return true;
    });

    auto* yBadge = new Box(Axis::ROW);
    yBadge->setPadding(2, 10, 2, 10);
    yBadge->setMarginLeft(14);
    yBadge->setCornerRadius(8);
    yBadge->setBorderColor(nvgRGB(58, 66, 78));
    yBadge->setBorderThickness(2);
    auto* yLabel = new Label();
    yLabel->setText("Y");
    yLabel->setFontSize(20);
    yLabel->setTextColor(theme["artemis/muted"]);
    yBadge->addView(yLabel);

    hostSettingsButton = new Box(Axis::ROW);
    hostSettingsButton->setWidth(56);
    hostSettingsButton->setHeight(56);
    hostSettingsButton->setCornerRadius(28);
    hostSettingsButton->setHighlightCornerRadius(28);
    hostSettingsButton->setBackgroundColor(theme["artemis/card"]);
    hostSettingsButton->setBorderColor(theme["artemis/card_line"]);
    hostSettingsButton->setBorderThickness(2);
    hostSettingsButton->setJustifyContent(JustifyContent::CENTER);
    hostSettingsButton->setAlignItems(AlignItems::CENTER);
    hostSettingsButton->setMarginRight(24);
    hostSettingsButton->setFocusable(true);
    hostSettingsButton->addView(new GlyphView("", 32, theme["brls/text"]));
    hostSettingsButton->addGestureRecognizer(
        new TapGestureRecognizer(hostSettingsButton));
    hostSettingsButton->registerClickAction([this](View*) {
        showPane(Pane::Host, true);
        return true;
    });
    right->addView(hostSettingsButton);
    right->addView(viewSwitch);
    right->addView(yBadge);
    header->addView(right);
    addView(header);

    // Applications pane: carousel (with the focused game's name and action
    // below it) or grid.
    appsContainer = new Box(Axis::COLUMN);
    appsContainer->setAlignItems(AlignItems::STRETCH);
    appsContainer->setHideHighlight(true);
    appsContainer->setGrow(1.0f);

    carouselContainer = new Box(Axis::COLUMN);
    carouselContainer->setAlignItems(AlignItems::STRETCH);
    carouselContainer->setGrow(1.0f);
    carousel = new GameCarousel();
    carousel->setOnSelected([this](size_t index) {
        updateCarouselInfo(index);
        // Reaching the last loaded game loads the next page. Deferred: the
        // tiles are rebuilt and this runs inside the focus change of one.
        if (index + 1 == carousel->size() &&
            displayedAppCount < filteredAppCount)
            brls::sync([this] { loadNextAppPage(); });
    });
    carouselContainer->addView(carousel);

    auto* info = new Box(Axis::ROW);
    info->setGrow(1.0f);
    info->setAlignItems(AlignItems::CENTER);
    info->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    info->setPadding(0, 56, 0, 56);
    auto* infoText = new Box(Axis::COLUMN);
    infoName = new Label();
    infoName->setFontSize(40);
    infoName->setSingleLine(true);
    infoText->addView(infoName);
    infoMeta = new Label();
    infoMeta->setFontSize(24);
    infoMeta->setMarginTop(6);
    infoMeta->setTextColor(theme["artemis/muted"]);
    infoText->addView(infoMeta);
    info->addView(infoText);

    ctaButton = new Box(Axis::ROW);
    ctaButton->setHeight(68);
    ctaButton->setPadding(0, 32, 0, 28);
    ctaButton->setCornerRadius(16);
    ctaButton->setBackgroundColor(theme["brls/accent"]);
    ctaButton->setAlignItems(AlignItems::CENTER);
    ctaButton->addView(new GlyphView("", 34, theme["artemis/on_accent"]));
    ctaLabel = new Label();
    ctaLabel->setFontSize(28);
    ctaLabel->setMarginLeft(10);
    ctaLabel->setTextColor(theme["artemis/on_accent"]);
    ctaButton->addView(ctaLabel);
    info->addView(ctaButton);
    carouselContainer->addView(info);
    appsContainer->addView(carouselContainer);

    gridContainer = new Box(Axis::COLUMN);
    gridContainer->setAlignItems(AlignItems::STRETCH);
    gridContainer->setPadding(4, 56, 0, 56);
    gridContainer->setGrow(1.0f);

    appsScroll = new ScrollingFrame();
    appsScroll->setGrow(1.0f);
    // Natural scrolling keeps the first row anchored below the header and
    // prevents a focused bottom-row tile from being centered over the footer.
    appsScroll->setScrollingBehavior(ScrollingBehavior::NATURAL);

    appsScrollContent = new Box(Axis::COLUMN);
    appsScrollContent->setAlignItems(AlignItems::STRETCH);
    appsScrollContent->setPaddingBottom(16);

    gridView = new GridView(
        static_cast<int>(artemis::ui::AppLibraryColumns));
    appsScrollContent->addView(gridView);

    loadMoreApps = new DetailCell();
    loadMoreApps->setText("app_list/load_more"_i18n);
    loadMoreApps->title->setSingleLine(true);
    loadMoreApps->detail->setSingleLine(true);
    loadMoreApps->setVisibility(Visibility::GONE);
    loadMoreApps->registerClickAction([this](View*) {
        loadNextAppPage();
        return true;
    });
    appsScrollContent->addView(loadMoreApps);
    appsScroll->setContentView(appsScrollContent);
    gridContainer->addView(appsScroll);
    appsContainer->addView(gridContainer);
    addView(appsContainer);

    // Host settings pane (shown by the button in the header).

    hostContainer = new Box(Axis::COLUMN);
    hostContainer->setAlignItems(AlignItems::STRETCH);
    hostContainer->setPadding(24, 56, 24, 56);
    hostContainer->setHideHighlight(true);
    hostContainer->setGrow(1.0f);
    hostContainer->setVisibility(Visibility::GONE);
    auto* hostTitle = new Label();
    hostTitle->setText("host/tab_host"_i18n);
    hostTitle->setFontSize(40);
    hostTitle->setMarginBottom(16);
    hostContainer->addView(hostTitle);

    hostIntegration = new DetailCell();
    hostIntegration->setText("host/integration"_i18n);
    hostIntegration->setDetailText("host/integration_detecting"_i18n);
    hostIntegration->title->setSingleLine(true);
    hostIntegration->detail->setSingleLine(true);
    hostContainer->addView(hostIntegration);

    webConfig = new DetailCell();
    webConfig->setText("host/web_config"_i18n);
    webConfig->setDetailText("host/web_config_hint"_i18n);
    webConfig->title->setSingleLine(true);
    webConfig->detail->setSingleLine(true);
    webConfig->registerClickAction([this](View*) {
        std::string address = detectedHostAddress;
        if (address.empty())
            address = this->host.preferred_address();
        if (address.empty()) {
            showError("host/web_config_no_address"_i18n);
            return true;
        }
        auto identity = detectedHostIdentity;
        if (identity.kind == artemis::host::HostKind::Unknown)
            identity = artemis::host::HostCapabilityPolicy::identityFor(
                artemis::host::HostKind::Sunshine);
        const std::string url =
            artemis::host::hostConsoleUrl(address, identity);
        brls::sync([url] {
            artemis::ui::showUrlQrDialog("host/web_config"_i18n, url);
        });
        return true;
    });
    hostContainer->addView(webConfig);
    refreshWebConfigVisibility();

    streamProfile = new DetailCell();
    streamProfile->setText("host/stream_profile"_i18n);
    streamProfile->title->setSingleLine(true);
    streamProfile->detail->setSingleLine(true);
    refreshStreamProfileLabel();
    streamProfile->registerClickAction([this](View*) {
        if (hostActionsBlockedByActiveSession())
            return true;
        artemis::streaming::open_host_profile_picker(
            hostProfileKey, [this] { refreshStreamProfileLabel(); });
        return true;
    });
    streamProfile->registerAction(
        "host/edit_profile"_i18n, BUTTON_Y, [this](View*) {
            if (hostActionsBlockedByActiveSession())
                return true;
            const auto selected =
                artemis::streaming::StreamConfigProfileStore::instance()
                    .selectedForHost(hostProfileKey);
            artemis::streaming::openProfileEditor(
                selected, hostProfileKey,
                [this] { refreshStreamProfileLabel(); });
            return true;
        });
    streamProfile->registerAction(
        "host/rename_profile"_i18n, BUTTON_START, [this](View*) {
            auto& store =
                artemis::streaming::StreamConfigProfileStore::instance();
            const auto selected = store.selectedForHost(hostProfileKey);
            if (selected.empty()) {
                showError("host/manage_profile_none"_i18n);
                return true;
            }
            auto profile = store.get(selected);
            if (!profile) {
                showError("host/manage_profile_none"_i18n);
                return true;
            }
            const auto currentName = profile->name;
            Application::getPlatform()->getImeManager()->openForText(
                [this, selected](const std::string& text) {
                    if (text.empty())
                        return;
                    if (artemis::streaming::StreamConfigProfileStore::instance()
                            .rename(selected, text))
                        refreshStreamProfileLabel();
                },
                "host/rename_profile_title"_i18n, "", 40, currentName, 0);
            return true;
        });
    streamProfile->registerAction(
        "host/delete_profile"_i18n, BUTTON_X, [this](View*) {
            const auto selected =
                artemis::streaming::StreamConfigProfileStore::instance()
                    .selectedForHost(hostProfileKey);
            if (selected.empty()) {
                showError("host/manage_profile_none"_i18n);
                return true;
            }
            auto* confirm =
                new brls::Dialog("host/delete_profile_message"_i18n);
            confirm->addButton("common/cancel"_i18n, [] {});
            confirm->addButton("common/remove"_i18n, [this, selected] {
                auto& store =
                    artemis::streaming::StreamConfigProfileStore::instance();
                store.remove(selected);
                store.clearSelectedForHost(hostProfileKey);
                refreshStreamProfileLabel();
            });
            confirm->open();
            return true;
        });
    hostContainer->addView(streamProfile);

    // Client-requested virtual display: fully modelled by ApolloHostOptions /
    // ApolloHostOptionsStore / resolveVirtualDisplay already, just missing a
    // control since 2aa0986/fac2d69 removed the old Host-tab UI. Only shown
    // when the host actually advertises the capability -- streaming_view.cpp
    // reads the same store at launch, so nothing else needs wiring.
    virtualDisplay = new DetailCell();
    virtualDisplay->setText("artemis/overlay/virtual_display"_i18n);
    virtualDisplay->title->setSingleLine(true);
    virtualDisplay->detail->setSingleLine(true);
    virtualDisplay->setVisibility(Visibility::GONE);
    refreshVirtualDisplayLabel();
    virtualDisplay->registerClickAction([this](View*) {
        if (hostActionsBlockedByActiveSession())
            return true;
        auto& store = artemis::apollo::ApolloHostOptionsStore::instance();
        auto options = store.get(hostProfileKey);

        static const std::pair<artemis::apollo::VirtualDisplayTarget,
                               const char*>
            kTargets[] = {
                {artemis::apollo::VirtualDisplayTarget::Off,
                 "artemis/overlay/vd_off"},
                {artemis::apollo::VirtualDisplayTarget::CurrentProfile,
                 "artemis/overlay/vd_current"},
                {artemis::apollo::VirtualDisplayTarget::Handheld,
                 "artemis/overlay/vd_handheld"},
                {artemis::apollo::VirtualDisplayTarget::Docked,
                 "artemis/overlay/vd_docked"},
                {artemis::apollo::VirtualDisplayTarget::PortraitHandheld,
                 "artemis/overlay/vd_portrait_handheld"},
                {artemis::apollo::VirtualDisplayTarget::PortraitDocked,
                 "artemis/overlay/vd_portrait_docked"},
                {artemis::apollo::VirtualDisplayTarget::Custom,
                 "artemis/overlay/vd_custom"},
            };
        std::vector<std::string> labels;
        int selected = 0;
        for (size_t i = 0; i < std::size(kTargets); ++i) {
            labels.push_back(brls::getStr(kTargets[i].second));
            if (kTargets[i].first == options.target)
                selected = static_cast<int>(i);
        }

        auto* dropdown = new brls::Dropdown(
            "artemis/overlay/virtual_display"_i18n, labels,
            [this, options](int index) mutable {
                if (index < 0 ||
                    index >= static_cast<int>(std::size(kTargets)))
                    return;
                options.target = kTargets[static_cast<size_t>(index)].first;
                if (options.target ==
                    artemis::apollo::VirtualDisplayTarget::Custom) {
                    Application::getPlatform()->getImeManager()->openForText(
                        [this, options](const std::string& text) mutable {
                            std::string error;
                            if (!artemis::apollo::parseVirtualDisplaySpec(
                                    text, options, &error)) {
                                showError(error.empty()
                                              ? "artemis/overlay/vd_custom_prompt"_i18n
                                              : error);
                                return;
                            }
                            options = artemis::apollo::validateApolloHostOptions(
                                options);
                            artemis::apollo::ApolloHostOptionsStore::instance()
                                .set(hostProfileKey, options);
                            refreshVirtualDisplayLabel();
                        },
                        "artemis/overlay/vd_custom"_i18n,
                        "artemis/overlay/vd_custom_prompt"_i18n, 24, "", 0);
                    return;
                }
                options = artemis::apollo::validateApolloHostOptions(options);
                artemis::apollo::ApolloHostOptionsStore::instance().set(
                    hostProfileKey, options);
                refreshVirtualDisplayLabel();
            },
            selected);
        Application::pushActivity(new Activity(dropdown));
        return true;
    });
    hostContainer->addView(virtualDisplay);

    addView(hostContainer);

    // Host settings pane: its actions live on the pane, so their hints show
    // only while it is open. B goes back to the games.
    hostContainer->registerAction(
        "hints/back"_i18n, BUTTON_B,
        [this](View*) {
            showPane(Pane::Applications, true);
            return true;
        },
        false, false, SOUND_BACK);
    hostContainer->registerAction("host/edit_profile"_i18n, BUTTON_Y,
                                  [this](View*) {
        const auto selected =
            artemis::streaming::StreamConfigProfileStore::instance()
                .selectedForHost(hostProfileKey);
        brls::sync([this, selected] {
            if (selected.empty()) {
                artemis::streaming::openProfileEditor(
                    {}, hostProfileKey,
                    [this] { refreshStreamProfileLabel(); });
            } else {
                artemis::streaming::openProfileEditor(
                    selected, hostProfileKey,
                    [this] { refreshStreamProfileLabel(); });
            }
        });
        return true;
    });
    hostContainer->registerAction("host/delete_profile"_i18n, BUTTON_X,
                                  [this](View*) {
        const auto selected =
            artemis::streaming::StreamConfigProfileStore::instance()
                .selectedForHost(hostProfileKey);
        if (selected.empty()) {
            showError("host/manage_profile_none"_i18n);
            return true;
        }
        brls::sync([this, selected] {
            auto* confirm = new brls::Dialog("host/delete_profile_message"_i18n);
            confirm->addButton("common/cancel"_i18n, [] {});
            confirm->addButton("common/remove"_i18n, [this, selected] {
                auto& store =
                    artemis::streaming::StreamConfigProfileStore::instance();
                store.remove(selected);
                store.clearSelectedForHost(hostProfileKey);
                refreshStreamProfileLabel();
            });
            confirm->open();
        });
        return true;
    });
    hostContainer->registerAction("host/new_profile"_i18n, BUTTON_RB,
                                  [this](View*) {
        artemis::streaming::open_create_host_profile(
            hostProfileKey, [this] { refreshStreamProfileLabel(); });
        return true;
    });

    // Shortcuts without a footer hint (the header already shows Y; + opens
    // Settings; RB refreshes the games). LB search is added by showPane.
    registerAction(
        "tabs/settings"_i18n, BUTTON_START,
        [](View*) {
            SettingsScreen::open();
            return true;
        },
        true);
    registerAction(
        "", BUTTON_Y,
        [this](View*) {
            if (activePane == Pane::Host)
                return false;
            toggleViewMode();
            return true;
        },
        true);
    appsContainer->registerAction(
        "app_list/reload_app_list"_i18n, BUTTON_RB,
        [this](View*) {
            updateAppList();
            return true;
        },
        true);

    loader = new LoadingOverlay(this);

    // Close the running app from the header too; the footer hint "Close" is
    // on the game tiles while an app runs.
    registerAction(
        "games/close"_i18n, brls::ControllerButton::BUTTON_BACK,
        [this](View*) {
            if (currentApp.has_value())
                this->terninateApp();
            return true;
        },
        true);
    setActionAvailable(brls::ControllerButton::BUTTON_BACK, false);

    refreshAppSearchLabel();
    applyViewMode();
    showPane(Pane::Applications, false);
    blockInput(true);
}

void AppListView::showPane(Pane pane, bool focusContent) {
    activePane = pane;
    const bool apps = pane == Pane::Applications;
    appsContainer->setVisibility(apps ? Visibility::VISIBLE : Visibility::GONE);
    hostContainer->setVisibility(apps ? Visibility::GONE : Visibility::VISIBLE);

    // A hint of an action that is registered always shows (greyed when it is
    // unavailable), so LB exists only while the games pane is open.
    if (apps && !searchAction) {
        searchAction = registerAction(
            "app_list/search"_i18n, BUTTON_LB, [this](View*) {
                promptAppSearch();
                return true;
            });
    } else if (!apps && searchAction) {
        unregisterAction(*searchAction);
        searchAction.reset();
    }
    applyViewMode();

    if (!focusContent)
        return;

    if (apps) {
        if (viewMode == ViewMode::Carousel && carousel->size() > 0)
            Application::giveFocus(carousel);
        else if (viewMode == ViewMode::Grid && !gridView->getChildren().empty())
            Application::giveFocus(gridView);
        else
            Application::giveFocus(backButton);
    } else if (webConfig &&
               webConfig->getVisibility() == Visibility::VISIBLE) {
        Application::giveFocus(webConfig);
    } else if (streamProfile) {
        Application::giveFocus(streamProfile);
    } else {
        Application::giveFocus(backButton);
    }
}

void AppListView::applyViewMode() {
    const bool carouselMode = viewMode == ViewMode::Carousel;
    carouselContainer->setVisibility(carouselMode ? Visibility::VISIBLE
                                                  : Visibility::GONE);
    gridContainer->setVisibility(carouselMode ? Visibility::GONE
                                              : Visibility::VISIBLE);

    // The active side of the switch is filled, as in the prototype.
    Theme theme = Application::getTheme();
    const NVGcolor filled = nvgRGB(238, 241, 244);
    const NVGcolor none = nvgRGBA(0, 0, 0, 0);
    carouselSwitchCell->setBackgroundColor(carouselMode ? filled : none);
    gridSwitchCell->setBackgroundColor(carouselMode ? none : filled);
    carouselSwitchIcon->setColor(carouselMode ? nvgRGB(20, 23, 28)
                                              : theme["artemis/muted"]);
    gridSwitchIcon->setColor(carouselMode ? theme["artemis/muted"]
                                          : nvgRGB(20, 23, 28));
}

void AppListView::toggleViewMode() {
    viewMode = viewMode == ViewMode::Carousel ? ViewMode::Grid
                                              : ViewMode::Carousel;
    lastGridMode = viewMode == ViewMode::Grid;
    applyViewMode();
    resetAppPagination();
    rebuildAppGrid();
    showPane(Pane::Applications, true);
}

void AppListView::clearAppViews() {
    gridView->clearViews();
    carousel->clearTiles();
    shownApps.clear();
}

void AppListView::updateCarouselInfo(size_t index) {
    if (index >= shownApps.size()) {
        infoName->setText("");
        infoMeta->setText("");
        ctaButton->setVisibility(Visibility::INVISIBLE);
        return;
    }

    const AppInfo& app = shownApps[index];
    const bool running = cachedCurrentGame != 0 && app.app_id == cachedCurrentGame;
    const bool blocked = cachedCurrentGame != 0 && !running;
    infoName->setText(app.name);
    infoMeta->setText(running ? "app_list/running"_i18n
                      : Settings::instance().is_favorite(host, app.app_id)
                          ? "games/favorite"_i18n
                          : "");
    // Another app is running, so this one cannot be started.
    ctaButton->setVisibility(blocked ? Visibility::INVISIBLE
                                     : Visibility::VISIBLE);
    ctaLabel->setText(running ? "games/resume"_i18n : "games/start"_i18n);
}

void AppListView::loadPreviewApps() {
    // Made-up games for the desktop UI preview; no host is contacted.
    static const char* const names[] = {
        "Genshin Impact",       "Steam",          "No Man's Sky",
        "Elder Scrolls Online", "Grand Theft Auto V", "Sea of Thieves",
        "Crimson Desert",       "GRID Autosport", "DiRT Rally 2.0",
        "Minecraft",            "War Thunder",    "Mirror's Edge",
        "PUBG: Battlegrounds"};
    cachedApps.clear();
    int id = 1;
    for (const char* name : names) {
        AppInfo app;
        app.name = name;
        app.app_id = id++;
        cachedApps.push_back(app);
    }
    cachedCurrentGame = 1;
    resetAppPagination();
    rebuildAppGrid();
    showPane(Pane::Applications, true);
}

void AppListView::refreshStreamProfileLabel() {
    if (!streamProfile)
        return;
    streamProfile->setDetailText(
        artemis::streaming::profile_detail_label(hostProfileKey));
}

void AppListView::refreshWebConfigVisibility() {
    if (!webConfig)
        return;
    webConfig->setVisibility(Settings::instance().show_host_web_config()
                                 ? Visibility::VISIBLE
                                 : Visibility::GONE);
}

void AppListView::refreshVirtualDisplayLabel() {
    if (!virtualDisplay)
        return;
    const auto options =
        artemis::apollo::ApolloHostOptionsStore::instance().get(hostProfileKey);
    virtualDisplay->setDetailText(
        brls::getStr(virtualDisplayTargetLabelKey(options.target)));
}

void AppListView::refreshHostIntegration(const SERVER_DATA& server) {
    if (!hostIntegration)
        return;

    detectedHostAddress = GameStreamClient::instance().active_address(host);
    detectedHostIdentity = server.hostIdentity;
    const auto capabilities = artemis::host::detectServerCapabilities(server);
    if (detectedHostIdentity.kind == artemis::host::HostKind::Unknown)
        detectedHostIdentity =
            artemis::host::HostCapabilityPolicy::identityFor(capabilities.kind);

    std::string product = detectedHostIdentity.product;
    if (!detectedHostIdentity.version.empty())
        product += " " + detectedHostIdentity.version;

    std::vector<std::string> features;
    features.push_back("host/feature_gamestream"_i18n);
    if (capabilities.preciseRefreshRate)
        features.push_back("host/feature_precise_refresh"_i18n);
    if (capabilities.virtualDisplay)
        features.push_back("host/feature_client_virtual_display"_i18n);
    if (capabilities.hostManagedVirtualDisplay)
        features.push_back("host/feature_host_virtual_display"_i18n);
    if (capabilities.serverCommands && !server.serverCommands.empty()) {
        std::string names;
        for (const auto& name : server.serverCommands) {
            if (!names.empty())
                names += ", ";
            names += name;
        }
        features.push_back("host/feature_server_commands"_i18n + ": " + names);
    }

    std::string detail = product;
    for (const auto& feature : features)
        detail += " · " + feature;
    hostIntegration->setDetailText(detail);

    if (virtualDisplay) {
        // Host-managed virtual display (Vibeshine/Punktfunk) sizes itself from
        // the requested stream resolution; the client-side picker only applies
        // to hosts that accept an explicit target (Apollo family).
        virtualDisplay->setVisibility(
            capabilities.virtualDisplay ? Visibility::VISIBLE : Visibility::GONE);
        refreshVirtualDisplayLabel();
    }

    if (webConfig) {
        switch (detectedHostIdentity.kind) {
            case artemis::host::HostKind::Vibeshine:
                webConfig->setDetailText("host/web_config_vibeshine_hint"_i18n);
                break;
            case artemis::host::HostKind::Punktfunk:
                webConfig->setDetailText("host/web_config_punktfunk_hint"_i18n);
                break;
            default:
                webConfig->setDetailText("host/web_config_hint"_i18n);
                break;
        }
    }
}

void AppListView::blockInput(bool block) {
    if (block && !inputBlocked) {
        inputBlocked = block;
        Application::blockInputs();
    } else if (!block && inputBlocked) {
        inputBlocked = block;
        Application::unblockInputs();
    }
}

void AppListView::terninateApp() {
    if (loading)
        return;

    auto* dialog =
        new Dialog("app_list/terminate_prefix"_i18n + currentApp->name +
                   "app_list/terminate_postfix"_i18n);

    dialog->addButton("common/cancel"_i18n, [] {});

    dialog->addButton("app_list/terminate"_i18n, [this] {
        if (loading)
            return;

        loading = true;
        clearAppViews();
        Application::giveFocus(this);
        loader->setHidden(false);
        blockInput(true);

        ASYNC_RETAIN
        GameStreamClient::instance().quit(
            host, [ASYNC_TOKEN](const GSResult<bool>& result) {
                ASYNC_RELEASE

                loading = false;
                loader->setHidden(true);
                blockInput(false);

                // Host may already have deleted/closed the app; quit then
                // fails but the grid still has to reload and accept input.
                if (artemis::streaming::shouldReloadAppsAfterHostQuit(
                        result.isSuccess()))
                    updateAppList();
            });
    });

    dialog->open();
}

void AppListView::updateAppList() {
    if (loading)
        return;

    if (artemis::ui_preview::enabled()) {
        loadPreviewApps();
        return;
    }

    loading = true;

    clearAppViews();
    resetAppPagination();
    Application::giveFocus(this);
    loader->setHidden(false);
    currentApp = std::nullopt;
    setActionAvailable(brls::ControllerButton::BUTTON_BACK, false);
    blockInput(true);
    refreshWebConfigVisibility();

    getAppletFrameItem()->title = host.hostname;
    updateAppletFrameItem();

    ASYNC_RETAIN
    GameStreamClient::instance().connect(
        host, [ASYNC_TOKEN](const GSResult<SERVER_DATA>& result) {
            ASYNC_RELEASE

            if (result.isSuccess()) {
                refreshHostIntegration(result.value());
                hostProfileKey = artemis::streaming::host_profile_key(
                    this->host, result.value().mac);
                refreshStreamProfileLabel();

                int currentGame = result.value().currentGame;

                ASYNC_RETAIN
                GameStreamClient::instance().applist(
                    host,
                    [ASYNC_TOKEN, currentGame](const GSResult<AppInfoList>& result) {
                        ASYNC_RELEASE

                        loading = false;
                        loader->setHidden(true);
                        blockInput(false);

                        if (result.isSuccess()) {
                            cachedApps = result.value();
                            cachedCurrentGame = currentGame;
                            const auto server =
                                GameStreamClient::instance().server_data(host);
                            if (!server.isApollo()) {
                                std::stable_sort(
                                    cachedApps.begin(), cachedApps.end(),
                                    [this, currentGame](const AppInfo& l,
                                                       const AppInfo& r) {
                                        const int lScore =
                                            (l.app_id == currentGame ? 2 : 0) +
                                            (Settings::instance().is_favorite(
                                                 this->host, l.app_id)
                                                 ? 1
                                                 : 0);
                                        const int rScore =
                                            (r.app_id == currentGame ? 2 : 0) +
                                            (Settings::instance().is_favorite(
                                                 this->host, r.app_id)
                                                 ? 1
                                                 : 0);
                                        return lScore > rScore;
                                    });
                            }
                            resetAppPagination();
                            rebuildAppGrid();
                            showPane(activePane, true);
                        } else {
                            showError(result.error(),
                                      [this] { this->dismiss(); });
                        }
                    });
            } else {
                loading = false;
                loader->setHidden(true);
                blockInput(false);
                showError(result.error(), [this] { this->dismiss(); });
            }
        });
}

void AppListView::refreshAppSearchLabel() {
    if (!searchLabel)
        return;
    searchLabel->setText(appSearchQuery.empty() ? ""
                                                : "\"" + appSearchQuery + "\"");
}

void AppListView::promptAppSearch() {
    Application::getImeManager()->openForText(
        [this](const std::string& text) {
            // Trim edges so accidental spaces do not empty the filtered list.
            std::string trimmed = text;
            while (!trimmed.empty() &&
                   std::isspace(static_cast<unsigned char>(trimmed.front())))
                trimmed.erase(trimmed.begin());
            while (!trimmed.empty() &&
                   std::isspace(static_cast<unsigned char>(trimmed.back())))
                trimmed.pop_back();
            appSearchQuery = std::move(trimmed);
            resetAppPagination();
            refreshAppSearchLabel();
            // Prefer an in-memory filter when apps are already cached so the
            // grid updates immediately after the IME closes.
            if (!cachedApps.empty() && !loading) {
                rebuildAppGrid();
                showPane(Pane::Applications, true);
            } else {
                updateAppList();
            }
        },
        "app_list/search"_i18n, "app_list/search_hint"_i18n, 40,
        appSearchQuery, 0);
}

void AppListView::rebuildAppGrid() {
    if (!gridView)
        return;
    clearAppViews();
    currentApp = std::nullopt;
    setActionAvailable(brls::ControllerButton::BUTTON_BACK, false);

    // Host status is independent of the current search/page. Preserve the
    // running-game indicator even when that app is outside the visible batch.
    if (cachedCurrentGame != 0) {
        const auto running = std::find_if(
            cachedApps.begin(), cachedApps.end(), [this](const AppInfo& app) {
                return app.app_id == cachedCurrentGame;
            });
        if (running != cachedApps.end())
            setCurrentApp(*running);
    }

    const std::string needle = lowercaseCopy(appSearchQuery);
    std::vector<const AppInfo*> uniqueApps;
    std::unordered_map<std::string, std::size_t> appByName;
    uniqueApps.reserve(cachedApps.size());

    for (const AppInfo& app : cachedApps) {
        if (!needle.empty()) {
            const auto haystack = lowercaseCopy(app.name);
            if (haystack.find(needle) == std::string::npos)
                continue;
        }

        std::string nameKey = artemis::ui::appLibraryNameKey(app.name);
        if (nameKey.empty())
            nameKey = "#" + std::to_string(app.app_id);

        const auto [entry, inserted] =
            appByName.emplace(nameKey, uniqueApps.size());
        if (inserted) {
            uniqueApps.push_back(&app);
            continue;
        }

        // If duplicate display names have different IDs, retain the entry that
        // is currently running, then a favorite, otherwise server order wins.
        const AppInfo* existing = uniqueApps[entry->second];
        const int existingPriority =
            (existing->app_id == cachedCurrentGame ? 2 : 0) +
            (Settings::instance().is_favorite(host, existing->app_id) ? 1 : 0);
        const int candidatePriority =
            (app.app_id == cachedCurrentGame ? 2 : 0) +
            (Settings::instance().is_favorite(host, app.app_id) ? 1 : 0);
        if (candidatePriority > existingPriority)
            uniqueApps[entry->second] = &app;
    }

    filteredAppCount = 0;
    displayedAppCount = 0;
    for (const AppInfo* appPtr : uniqueApps) {
        const AppInfo& app = *appPtr;
        ++filteredAppCount;
        if (displayedAppCount >= visibleAppLimit)
            continue;

        const bool inCarousel = viewMode == ViewMode::Carousel;
        auto* tile = inCarousel
                         ? new GameTile(host, app, GameCarousel::kSideWidth,
                                        GameCarousel::kSideHeight,
                                        cachedCurrentGame)
                         : new GameTile(host, app, GameTile::kGridWidth,
                                        GameTile::kGridHeight,
                                        cachedCurrentGame);
        tile->setFavorite(Settings::instance().is_favorite(host, app.app_id));
        if (inCarousel) {
            carousel->addTile(tile);
        } else {
            // The scrolling list traps the focus, so the way up to the header
            // is set explicitly for the first row.
            if (gridView->getChildren().size() <
                artemis::ui::AppLibraryColumns)
                tile->setCustomNavigationRoute(FocusDirection::UP, backButton);
            gridView->addView(tile);
        }
        shownApps.push_back(app);
        if (cachedCurrentGame != 0) {
            // Registered before the favorite action so it is the leftmost hint.
            tile->registerAction("games/close"_i18n, BUTTON_BACK,
                                 [this](View*) {
                                     if (currentApp.has_value())
                                         this->terninateApp();
                                     return true;
                                 });
        }
        updateFavoriteAction(tile, host, app);
        ++displayedAppCount;
    }

    if (loadMoreApps) {
        const auto page = artemis::ui::appLibraryPageWindow(
            filteredAppCount, visibleAppLimit);
        loadMoreApps->setVisibility(
            page.hasMore && viewMode == ViewMode::Grid ? Visibility::VISIBLE
                                                       : Visibility::GONE);
        loadMoreApps->setDetailText(std::to_string(displayedAppCount) +
                                    " / " +
                                    std::to_string(filteredAppCount));
    }
    gridView->invalidate();
    if (appsContainer)
        appsContainer->invalidate();
    updateCarouselInfo(carousel->selected());
}

void AppListView::loadNextAppPage() {
    const size_t firstNewIndex = displayedAppCount;
    visibleAppLimit = artemis::ui::nextAppLibraryLimit(
        filteredAppCount, visibleAppLimit);
    rebuildAppGrid();

    if (viewMode == ViewMode::Carousel) {
        if (firstNewIndex < carousel->size())
            Application::giveFocus(carousel->tileAt(firstNewIndex));
    } else if (firstNewIndex < gridView->getChildren().size()) {
        Application::giveFocus(gridView->getChildren()[firstNewIndex]);
    }
}

void AppListView::resetAppPagination() {
    visibleAppLimit = artemis::ui::AppLibraryPageSize;
    displayedAppCount = 0;
    filteredAppCount = 0;
    if (appsScroll)
        appsScroll->setContentOffsetY(0, false);
}

void AppListView::setCurrentApp(const AppInfo& app) {
    this->currentApp = app;
    setActionAvailable(brls::ControllerButton::BUTTON_BACK, true);
    getAppletFrameItem()->title =
        host.hostname + " - " + "app_list/running"_i18n + " " + app.name;
    updateAppletFrameItem();
}

void AppListView::willAppear(bool resetState) {
    Box::willAppear(resetState);
    if (artemis::streaming::consumeStreamUiClosed()) {
        pendingPostStreamRefresh = false;
        streamWasActive = false;
        updateAppList();
        return;
    }
    // Avoid a full network reload when the IME dismisses after search — that
    // was wiping the filtered grid before the async applist returned.
    if (!cachedApps.empty()) {
        refreshAppSearchLabel();
        rebuildAppGrid();
        if (activePane == Pane::Applications)
            showPane(Pane::Applications, true);
        return;
    }
    updateAppList();
}

void AppListView::draw(NVGcontext* vg, float x, float y, float width,
                       float height, Style style, FrameContext* ctx) {
    const bool sessionActive = MoonlightSession::activeSession() != nullptr;
    if (sessionActive) {
        streamWasActive = true;
    } else if (streamWasActive) {
        streamWasActive = false;
        pendingPostStreamRefresh = true;
    }

    if (pendingPostStreamRefresh && !loading &&
        MoonlightSession::activeSession() == nullptr) {
        pendingPostStreamRefresh = false;
        // Defer one frame so StreamingView/MoonlightSession teardown finishes
        // before Host/Applications UI reconnects and pushes activities.
        ASYNC_RETAIN
        delay(1, [ASYNC_TOKEN] {
            ASYNC_RELEASE
            if (!loading)
                updateAppList();
        });
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

void AppListView::onLayout() {
    Box::onLayout();

    if (loader)
        loader->layout();
}

void AppListView::updateFavoriteAction(GameTile* cell, Host host, const AppInfo& app) {
    bool isFavorite = Settings::instance().is_favorite(host, app.app_id);
    cell->registerAction(
        isFavorite ? "app_list/unstar"_i18n : "app_list/star"_i18n, BUTTON_X,
        [this, cell, host, app](View* view) {
            bool isFavorite =
                Settings::instance().is_favorite(host, app.app_id);
            cell->setFavorite(!isFavorite);
            if (isFavorite) {
                Settings::instance().remove_favorite(host, app.app_id);
            } else {
                App thisApp{app.name, app.app_id};
                Settings::instance().add_favorite(host, thisApp);
            }
            this->updateFavoriteAction(cell, host, app);
            return true;
        });
    Application::getGlobalHintsUpdateEvent()->fire();
}
