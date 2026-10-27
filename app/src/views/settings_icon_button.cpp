//
//  settings_icon_button.cpp
//  Artemis
//

#include "views/settings_icon_button.hpp"

SettingsIconButton::SettingsIconButton(std::string glyph)
    : glyph(std::move(glyph)) {
    setWidth(72);
    setHeight(64);
    setMarginBottom(10);
    setFocusable(true);
    setCornerRadius(18);
    setHighlightCornerRadius(18);
    setActive(false);

    registerClickAction([this](brls::View*) {
        if (onSelect)
            onSelect();
        return true;
    });
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

void SettingsIconButton::setActive(bool value) {
    active = value;
    setBackgroundColor(active ? brls::Application::getTheme()["brls/accent"]
                              : nvgRGBA(0, 0, 0, 0));
}

void SettingsIconButton::onFocusGained() {
    Box::onFocusGained();
    // Moving the focus over the sidebar switches the section, like a tab.
    if (onSelect)
        onSelect();
}

void SettingsIconButton::draw(NVGcontext* vg, float x, float y, float width,
                              float height, brls::Style style,
                              brls::FrameContext* ctx) {
    Box::draw(vg, x, y, width, height, style, ctx);

    brls::Theme theme = brls::Application::getTheme();
    nvgFontFaceId(vg, brls::Application::getFont(brls::FONT_MATERIAL_ICONS));
    nvgFontSize(vg, 36.0f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, active ? theme["artemis/on_accent"]
                            : theme["artemis/muted"]);
    nvgText(vg, x + width / 2, y + height / 2, glyph.c_str(), nullptr);
}
