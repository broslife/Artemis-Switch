//
//  favorite_tile.cpp
//  Artemis
//

#include "views/favorite_tile.hpp"
#include "BoxArtManager.hpp"
#include "UiPreview.hpp"
#include "streaming_view.hpp"

#include <functional>

FavoriteTile::FavoriteTile(const Host& host, const AppInfo& app)
    : brls::Box(brls::Axis::COLUMN) {
    setWidth(kWidth);
    setHeight(kHeight);
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

    auto* labelBox = new brls::Box(brls::Axis::COLUMN);
    labelBox->setPositionType(brls::PositionType::ABSOLUTE);
    labelBox->setPositionLeft(0);
    labelBox->setPositionBottom(0);
    labelBox->setWidth(kWidth);
    labelBox->setPadding(14);
    labelBox->setJustifyContent(brls::JustifyContent::FLEX_END);
    labelBox->setBackgroundColor(brls::Application::getTheme()["brls/backdrop"]);
    auto* label = new brls::Label();
    label->setText(app.name);
    label->setFontSize(20);
    label->setTextColor(nvgRGB(238, 241, 244));
    labelBox->addView(label);
    addView(labelBox);

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
}
