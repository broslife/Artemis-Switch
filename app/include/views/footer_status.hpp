//
//  footer_status.hpp
//  Artemis
//

#pragma once

#include <borealis.hpp>

namespace artemis::ui {

// Battery and clock live in the screen header, so the footer keeps only the
// button hints. Looks the footer up from the root of the view's activity.
inline void hideFooterStatus(brls::View* from) {
    brls::View* root = from;
    while (root->hasParent())
        root = root->getParent();
    for (const char* id : {"brls/battery", "brls/wireless", "brls/hints/time"}) {
        if (brls::View* view = root->getView(id))
            view->setVisibility(brls::Visibility::GONE);
    }
}

} // namespace artemis::ui
