//
//  status_bar.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

// Battery level and clock for screen headers.
class StatusBar : public brls::View {
  public:
    StatusBar();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
};
