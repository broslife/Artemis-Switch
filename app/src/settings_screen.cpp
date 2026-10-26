//
//  settings_screen.cpp
//  Artemis
//

#include "settings_screen.hpp"
#include "settings_tab.hpp"
#include "views/settings_icon_button.hpp"

using namespace brls::literals;

namespace {
struct Section {
    const char* glyph; // Material Icons code point (UTF-8)
    const char* titleKey;
    std::vector<const char*> groups; // ids of the groups shown in this section
};

const std::vector<Section>& sections() {
    static const std::vector<Section> list = {
        {"", "settings/sections/video",
         {"grp_quality", "grp_bitrate", "grp_image", "grp_custom_res",
          "grp_frame", "grp_presentation"}},
        {"", "settings/sections/audio", {"grp_audio"}},
        {"", "settings/sections/host", {"grp_host"}},
        {"", "settings/sections/controls",
         {"grp_keys", "grp_deadzone", "grp_rumble", "grp_joycon", "grp_guide",
          "grp_motion"}},
        {"", "settings/sections/overlay", {"grp_overlay"}},
        {"", "settings/sections/input",
         {"grp_mouse_input", "grp_keyboard", "grp_mouse", "grp_mouse_speed"}},
        {"", "settings/sections/advanced",
         {"grp_language", "grp_debug", "grp_remote_access", "grp_notes"}},
    };
    return list;
}
} // namespace

SettingsScreen::SettingsScreen() {
    setAxis(brls::Axis::ROW);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);

    auto* sidebar = new brls::Box(brls::Axis::COLUMN);
    sidebar->setWidth(72);
    sidebar->setMarginRight(24);
    sidebar->setMarginTop(24);
    addView(sidebar);

    auto* panel = new brls::Box(brls::Axis::COLUMN);
    panel->setGrow(1.0f);
    addView(panel);

    title = new brls::Label();
    title->setFontSize(40);
    title->setMarginTop(16);
    title->setMarginLeft(8);
    panel->addView(title);

    tab = new SettingsTab();
    tab->setGrow(1.0f);
    panel->addView(tab);

    const auto& list = sections();
    for (size_t i = 0; i < list.size(); i++) {
        auto* button = new SettingsIconButton(list[i].glyph);
        button->setOnSelect([this, i] { selectSection(i); });
        sidebar->addView(button);
        buttons.push_back(button);
    }

    selectSection(0);
}

void SettingsScreen::selectSection(size_t index) {
    if (index == current || index >= sections().size())
        return;
    current = index;

    const auto& selected = sections()[index];
    for (const auto& section : sections()) {
        for (const char* id : section.groups) {
            if (brls::View* group = tab->getView(id)) {
                const bool show = &section == &selected;
                group->setVisibility(show ? brls::Visibility::VISIBLE
                                          : brls::Visibility::GONE);
            }
        }
    }

    for (size_t i = 0; i < buttons.size(); i++)
        buttons[i]->setActive(i == index);

    title->setText(brls::getStr(selected.titleKey));

    if (auto* scroll = dynamic_cast<brls::ScrollingFrame*>(
            tab->getView("settings_scroll")))
        scroll->setContentOffsetY(0, false);
}

brls::View* SettingsScreen::create() { return new SettingsScreen(); }
