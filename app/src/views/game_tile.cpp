//
//  game_tile.cpp
//  Artemis
//

#include "views/game_tile.hpp"
#include "BoxArtManager.hpp"
#include "UiPreview.hpp"
#include "streaming_view.hpp"

#include <functional>

using namespace brls::literals;

GameTile::GameTile(const Host& host, const AppInfo& app, float width,
                   float height, int currentApp)
    : brls::Box(brls::Axis::COLUMN) {
    brls::Theme theme = brls::Application::getTheme();

    setFocusable(true);
    setCornerRadius(18);
    setHighlightCornerRadius(18);

    // Shown until the box art arrives; a hue taken from the name keeps tiles apart.
    const float hue = (std::hash<std::string>{}(app.name) % 360) / 360.0f;
    setBackgroundColor(nvgHSL(hue, 0.35f, 0.28f));

    image = new brls::Image();
    image->setGrow(1.0f);
    image->setScalingType(brls::ImageScalingType::FILL);
    image->setCornerRadius(18);
    addView(image);

    // Dims the tiles that cannot be started while another app is running.
    const bool inactive = currentApp != 0 && currentApp != app.app_id;
    dimLayer = new brls::Box(brls::Axis::COLUMN);
    dimLayer->setPositionType(brls::PositionType::ABSOLUTE);
    dimLayer->setPositionLeft(0);
    dimLayer->setPositionTop(0);
    dimLayer->setCornerRadius(18);
    dimLayer->setBackgroundColor(nvgRGBA(74, 74, 74, 160));
    dimLayer->setVisibility(inactive ? brls::Visibility::VISIBLE
                                     : brls::Visibility::GONE);
    addView(dimLayer);

    labelBox = new brls::Box(brls::Axis::COLUMN);
    labelBox->setPositionType(brls::PositionType::ABSOLUTE);
    labelBox->setPositionLeft(0);
    labelBox->setPositionBottom(0);
    labelBox->setPadding(14);
    labelBox->setJustifyContent(brls::JustifyContent::FLEX_END);
    labelBox->setBackgroundColor(theme["brls/backdrop"]);
    auto* label = new brls::Label();
    label->setText(app.name);
    label->setFontSize(20);
    label->setTextColor(nvgRGB(238, 241, 244));
    labelBox->addView(label);
    addView(labelBox);

    badge = new brls::Box(brls::Axis::ROW);
    badge->setPositionType(brls::PositionType::ABSOLUTE);
    badge->setPositionLeft(12);
    badge->setPositionTop(12);
    badge->setPadding(4, 12, 4, 12);
    badge->setCornerRadius(16);
    badge->setBackgroundColor(theme["brls/accent"]);
    auto* badgeLabel = new brls::Label();
    badgeLabel->setText("app_list/running"_i18n);
    badgeLabel->setFontSize(16);
    badgeLabel->setTextColor(theme["artemis/on_accent"]);
    badge->addView(badgeLabel);
    badge->setVisibility(currentApp == app.app_id ? brls::Visibility::VISIBLE
                                                  : brls::Visibility::GONE);
    addView(badge);

    favoriteIcon = new GlyphView("", 28, theme["brls/accent"]);
    favoriteIcon->setPositionType(brls::PositionType::ABSOLUTE);
    favoriteIcon->setPositionRight(10);
    favoriteIcon->setPositionTop(10);
    favoriteIcon->setVisibility(brls::Visibility::GONE);
    addView(favoriteIcon);

    setSize(width, height);

    if (!artemis::ui_preview::enabled()) {
        if (BoxArtManager::instance().has_boxart(app.app_id)) {
            image->setImageFromFile(BoxArtManager::get_texture_path(app.app_id));
        } else {
            ASYNC_RETAIN
            GameStreamClient::instance().app_boxart(
                host, app.app_id, [ASYNC_TOKEN, app](auto result) {
                    ASYNC_RELEASE

                    if (result.isSuccess()) {
                        BoxArtManager::instance().set_data(result.value(),
                                                           app.app_id);
                        image->setImageFromFile(
                            BoxArtManager::get_texture_path(app.app_id));
                    }
                });
        }
    }

    addGestureRecognizer(new brls::TapGestureRecognizer(this));
    registerClickAction([host, app](brls::View*) {
        if (artemis::ui_preview::enabled()) {
            brls::Application::notify("Preview mode: no stream is started");
            return true;
        }
        auto* frame = new brls::AppletFrame(new StreamingView(host, app));
        frame->setBackground(brls::ViewBackground::NONE);
        frame->setHeaderVisibility(brls::Visibility::GONE);
        frame->setFooterVisibility(brls::Visibility::GONE);
        brls::Application::pushActivity(new brls::Activity(frame));
        return true;
    });
    setActionAvailable(brls::ControllerButton::BUTTON_A, !inactive);
}

void GameTile::setSize(float width, float height) {
    setWidth(width);
    setHeight(height);
    dimLayer->setWidth(width);
    dimLayer->setHeight(height);
    labelBox->setWidth(width);
}

void GameTile::setFavorite(bool favorite) {
    favoriteIcon->setVisibility(favorite ? brls::Visibility::VISIBLE
                                         : brls::Visibility::GONE);
}

void GameTile::setLabelVisible(bool visible) {
    labelBox->setVisibility(visible ? brls::Visibility::VISIBLE
                                    : brls::Visibility::GONE);
}

void GameTile::onFocusGained() {
    Box::onFocusGained();
    if (onFocus)
        onFocus();
}
