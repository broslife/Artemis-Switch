//
//  status_bar.cpp
//  Artemis
//

#include "views/status_bar.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

StatusBar::StatusBar() {
    setWidth(230);
    setHeight(40);
}

void StatusBar::draw(NVGcontext* vg, float x, float y, float width,
                     float height, brls::Style style,
                     brls::FrameContext* ctx) {
    brls::Theme theme = brls::Application::getTheme();
    const NVGcolor muted = theme["artemis/muted"];
    const float cy = y + height / 2;

    const auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::ostringstream clock;
    clock << std::put_time(std::localtime(&now), "%H:%M");

    nvgFontFaceId(vg, brls::Application::getFont(brls::FONT_REGULAR));
    nvgFontSize(vg, 22.0f);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, muted);

    float right = x + width;
    float bounds[4];
    const std::string time = clock.str();
    nvgTextBounds(vg, 0, 0, time.c_str(), nullptr, bounds);
    nvgText(vg, right, cy, time.c_str(), nullptr);
    right -= (bounds[2] - bounds[0]) + 22.0f;

    brls::Platform* platform = brls::Application::getPlatform();
    if (!platform->canShowBatteryLevel())
        return;

    const int level = std::max(0, std::min(100, platform->getBatteryLevel()));
    const std::string percent = std::to_string(level) + "%";
    nvgTextBounds(vg, 0, 0, percent.c_str(), nullptr, bounds);
    nvgText(vg, right, cy, percent.c_str(), nullptr);
    right -= (bounds[2] - bounds[0]) + 10.0f;

    // Battery icon: outline plus a fill proportional to the level.
    const float bw = 34.0f, bh = 17.0f;
    const float bx = right - bw, by = cy - bh / 2;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, bx, by, bw, bh, 4.0f);
    nvgStrokeColor(vg, muted);
    nvgStrokeWidth(vg, 2.0f);
    nvgStroke(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, bx + 3, by + 3, (bw - 6) * level / 100.0f, bh - 6, 1.5f);
    nvgFillColor(vg, level <= 15 ? nvgRGB(229, 57, 53) : theme["brls/accent"]);
    nvgFill(vg);
}
