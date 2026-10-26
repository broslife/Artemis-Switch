//
//  host_card.cpp
//  Artemis
//

#include "host_card.hpp"
#include "GameStreamClient.hpp"
#include "app_list_view.hpp"
#include "helper.hpp"
#include "home_screen.hpp"
#include "streaming/HostProfileKey.hpp"
#include "streaming/HostStreamProfileUi.hpp"
#include "streaming/ProfileEditorDialog.hpp"
#include "streaming/StreamConfigProfileStore.hpp"

using namespace brls::literals;

namespace {
std::string host_subtitle(const Host& host) {
    const auto addresses = host.connection_addresses();
    if (addresses.empty()) {
        return "";
    }
    if (addresses.size() == 1) {
        return addresses.front();
    }
    return addresses.front() + " | +" +
           std::to_string(addresses.size() - 1) + " more";
}
} // namespace

HostCard::HostCard(const Host& host) : brls::Box(brls::Axis::COLUMN), host(host) {
    buildLayout();
    reloadHost();

    setFocusable(true);
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
    registerClickAction([this](brls::View*) {
        if (state == AVAILABLE)
            present(new AppListView(this->host));
        else if (state == UNAVAILABLE)
            connectOrWake();
        return true;
    });

    registerAction("host/rename"_i18n, brls::ControllerButton::BUTTON_START,
                   [this](brls::View*) {
                       std::string title = this->host.hostname;
                       brls::Application::getPlatform()->getImeManager()->openForText(
                           [this](const std::string& text) {
                               this->host.hostname = text;
                               Settings::instance().add_host(this->host);
                               HomeScreen::instance()->refreshHosts();
                           },
                           "host/rename_title"_i18n, "", 60, title, 0);
                       return true;
                   });

    registerAction("host/new_profile"_i18n, brls::ControllerButton::BUTTON_RB,
                   [this](brls::View*) {
                       if (state == AVAILABLE)
                           createProfileForHost();
                       return true;
                   });

    registerAction("host/options"_i18n, brls::ControllerButton::BUTTON_Y,
                   [this](brls::View*) {
                       showOptions();
                       return true;
                   });
}

void HostCard::buildLayout() {
    brls::Theme theme = brls::Application::getTheme();

    setWidth(368);
    setHeight(300);
    setPadding(28);
    setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    setAlignItems(brls::AlignItems::STRETCH);
    setCornerRadius(24);
    setHighlightCornerRadius(24);
    setBorderThickness(2);

    auto* top = new brls::Box(brls::Axis::ROW);
    top->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    top->setAlignItems(brls::AlignItems::CENTER);

    icon = new GlyphView("", 52, theme["brls/accent"]);
    top->addView(icon);

    badge = new brls::Box(brls::Axis::ROW);
    badge->setPadding(6, 14, 6, 14);
    badge->setCornerRadius(20);
    badgeLabel = new brls::Label();
    badgeLabel->setFontSize(18);
    badge->addView(badgeLabel);
    top->addView(badge);
    addView(top);

    auto* middle = new brls::Box(brls::Axis::COLUMN);
    nameLabel = new brls::Label();
    nameLabel->setFontSize(30);
    middle->addView(nameLabel);
    subLabel = new brls::Label();
    subLabel->setFontSize(20);
    subLabel->setMarginTop(8);
    subLabel->setTextColor(theme["artemis/muted"]);
    middle->addView(subLabel);
    addView(middle);

    pill = new brls::Box(brls::Axis::ROW);
    pill->setHeight(60);
    pill->setCornerRadius(14);
    pill->setJustifyContent(brls::JustifyContent::CENTER);
    pill->setAlignItems(brls::AlignItems::CENTER);
    pillLabel = new brls::Label();
    pillLabel->setFontSize(24);
    pill->addView(pillLabel);
    addView(pill);

    nameLabel->setText(host.hostname);
    applyStyle();
}

void HostCard::applyStyle() {
    brls::Theme theme = brls::Application::getTheme();
    const NVGcolor accent = theme["brls/accent"];
    const NVGcolor muted = theme["artemis/muted"];
    const NVGcolor onAccent = theme["artemis/on_accent"];
    const NVGcolor line = theme["artemis/card_line"];

    setBackgroundColor(focused ? nvgRGB(38, 45, 56) : theme["artemis/card"]);
    setBorderColor(line);

    const bool online = state == AVAILABLE;
    icon->setColor(online ? accent : nvgRGB(125, 135, 148));

    badge->setBackgroundColor(online ? nvgRGBA(46, 230, 193, 36)
                                     : nvgRGBA(0, 0, 0, 0));
    badge->setBorderColor(line);
    badge->setBorderThickness(online ? 0 : 2);
    badgeLabel->setTextColor(online ? accent : muted);

    const bool showPill = state != FETCHING;
    pill->setVisibility(showPill ? brls::Visibility::VISIBLE
                                 : brls::Visibility::INVISIBLE);
    if (focused && online) {
        pill->setBackgroundColor(accent);
        pill->setBorderThickness(0);
        pillLabel->setTextColor(onAccent);
    } else {
        pill->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
        pill->setBorderColor(focused ? accent : line);
        pill->setBorderThickness(2);
        pillLabel->setTextColor(focused ? accent : muted);
    }
}

void HostCard::onFocusGained() {
    Box::onFocusGained();
    focused = true;
    applyStyle();
}

void HostCard::onFocusLost() {
    Box::onFocusLost();
    focused = false;
    applyStyle();
}

void HostCard::showOptions() {
    auto* dialog = new brls::Dialog(host.hostname);
    if (state == AVAILABLE) {
        dialog->addButton("host/edit_profile"_i18n,
                          [this] { quickEditProfile(); });
        dialog->addButton("host/delete_profile"_i18n,
                          [this] { quickDeleteProfile(); });
    }
    const Host removed = host;
    dialog->addButton("common/remove"_i18n, [removed] {
        auto* confirm = new brls::Dialog("host/remove_message"_i18n);
        confirm->addButton("common/cancel"_i18n, [] {});
        confirm->addButton("common/remove"_i18n, [removed] {
            Settings::instance().remove_host(removed);
            HomeScreen::instance()->refreshHosts();
        });
        confirm->open();
    });
    dialog->open();
}

void HostCard::connectOrWake() {
    if (!GameStreamClient::can_wake_up_host(this->host))
        return;

    const auto wakeRequestId = ++this->wakeRequestGeneration;
    this->canceledWakeRequestGeneration = 0;

    brls::Dialog* loader = createLoadingDialog(
        "host/wake_up_message"_i18n, [this, wakeRequestId] {
            if (this->wakeRequestGeneration == wakeRequestId) {
                this->canceledWakeRequestGeneration = wakeRequestId;
            }
        });
    loader->open();

    ASYNC_RETAIN
    GameStreamClient::wake_up_host(
        this->host,
        [ASYNC_TOKEN, loader, wakeRequestId](const GSResult<bool>& result) {
            ASYNC_RELEASE

            if (wakeRequestId != this->wakeRequestGeneration) {
                return;
            }

            if (this->canceledWakeRequestGeneration == wakeRequestId) {
                return;
            }

            loader->close([this, result, wakeRequestId] {
                if (wakeRequestId != this->wakeRequestGeneration) {
                    return;
                }

                if (result.isSuccess()) {
                    reloadHost();
                } else {
                    showError("host/wake_up_error"_i18n);
                }
            });
        });
}

void HostCard::createProfileForHost() {
    const auto key = artemis::streaming::host_profile_key(host);
    brls::sync([key] {
        artemis::streaming::openProfileEditor({}, key, [] {});
    });
}

void HostCard::quickEditProfile() {
    const auto key = artemis::streaming::host_profile_key(host);
    const auto selected =
        artemis::streaming::StreamConfigProfileStore::instance().selectedForHost(
            key);
    brls::sync([key, selected] {
        if (selected.empty()) {
            artemis::streaming::openProfileEditor({}, key, [] {});
        } else {
            artemis::streaming::openProfileEditor(selected, key, [] {});
        }
    });
}

void HostCard::quickDeleteProfile() {
    const auto key = artemis::streaming::host_profile_key(host);
    const auto selected =
        artemis::streaming::StreamConfigProfileStore::instance().selectedForHost(
            key);
    if (selected.empty()) {
        showError("host/manage_profile_none"_i18n);
        return;
    }
    brls::sync([key, selected] {
        auto* confirm = new brls::Dialog("host/delete_profile_message"_i18n);
        confirm->addButton("common/cancel"_i18n, [] {});
        confirm->addButton("common/remove"_i18n, [key, selected] {
            auto& store = artemis::streaming::StreamConfigProfileStore::instance();
            store.remove(selected);
            store.clearSelectedForHost(key);
        });
        confirm->open();
    });
}

void HostCard::reloadHost() {
    state = FETCHING;
    badgeLabel->setText("host/fetching"_i18n);
    subLabel->setText(host_subtitle(host));
    pillLabel->setText("host/wait"_i18n);
    applyStyle();

    ASYNC_RETAIN
    GameStreamClient::instance().connect(
        host, [ASYNC_TOKEN](const GSResult<SERVER_DATA>& result) {
            ASYNC_RELEASE

            if (result.isSuccess()) {
                const auto connectedAddress =
                    GameStreamClient::instance().active_address(this->host);
                badgeLabel->setText("host/ready"_i18n);
                subLabel->setText(connectedAddress.empty()
                                      ? host_subtitle(this->host)
                                      : connectedAddress);
                pillLabel->setText("host/connect"_i18n);
                state = AVAILABLE;
            } else {
                badgeLabel->setText("host/unable"_i18n);
                pillLabel->setText("host/wake_up"_i18n);
                state = UNAVAILABLE;
            }
            applyStyle();
        });
}
