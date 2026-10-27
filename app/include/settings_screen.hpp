//
//  settings_screen.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

#include <vector>

class SettingsTab;
class SettingsIconButton;

// Settings with an icon-only sidebar. Each icon shows one section; the
// sections are groups of the existing SettingsTab (ids "grp_*"), so every
// option keeps the code that already handles it.
class SettingsScreen : public brls::Box {
  public:
    SettingsScreen();

    void willAppear(bool resetState) override;

    static brls::View* create();
    // Opens Settings as a full screen on top of the current one.
    static void open();

  private:
    void selectSection(size_t index);

    brls::Label* title = nullptr;
    SettingsTab* tab = nullptr;
    std::vector<SettingsIconButton*> buttons;
    size_t current = static_cast<size_t>(-1);
};
