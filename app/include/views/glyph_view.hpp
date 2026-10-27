//
//  glyph_view.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

#include <string>

// A single Material Icons glyph, drawn with the font Borealis already loads.
class GlyphView : public brls::View {
  public:
    GlyphView(std::string glyph, float size, NVGcolor color);

    void setColor(NVGcolor value) { color = value; }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    std::string glyph;
    float size;
    NVGcolor color;
};
