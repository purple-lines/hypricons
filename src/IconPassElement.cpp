#include "IconPassElement.hpp"
#include "IconOverlay.hpp"
#include "globals.hpp"

#include <hyprland/src/render/OpenGL.hpp>

CIconPassElement::CIconPassElement(const SIconData& data) : m_data(data) {}

void CIconPassElement::draw(const CRegion& damage) {
    if (!m_data.overlay)
        return;

    m_data.overlay->renderPass();
}

std::optional<CBox> CIconPassElement::boundingBox() {
    return std::nullopt;
}
