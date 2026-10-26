//
//  glyph_view.cpp
//  Artemis
//

#include "views/glyph_view.hpp"

GlyphView::GlyphView(std::string glyph, float size, NVGcolor color)
    : glyph(std::move(glyph)), size(size), color(color) {
    setWidth(size);
    setHeight(size);
}

void GlyphView::draw(NVGcontext* vg, float x, float y, float width,
                     float height, brls::Style style,
                     brls::FrameContext* ctx) {
    nvgFontFaceId(vg, brls::Application::getFont(brls::FONT_MATERIAL_ICONS));
    nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, color);
    nvgText(vg, x + width / 2, y + height / 2, glyph.c_str(), nullptr);
}
