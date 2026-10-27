//
//  settings_icon_button.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

#include <functional>
#include <string>

// Icon-only entry of the Settings sidebar. The glyph is a Material Icons
// code point drawn with the font Borealis already loads.
class SettingsIconButton : public brls::Box {
  public:
    explicit SettingsIconButton(std::string glyph);

    void setActive(bool active);
    void setOnSelect(std::function<void()> callback) {
        onSelect = std::move(callback);
    }

    void onFocusGained() override;
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    std::string glyph;
    bool active = false;
    std::function<void()> onSelect;
};
