#include "IconOverlay.hpp"
#include "IconPassElement.hpp"
#include "globals.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>

#include <cmath>
#include <algorithm>
#include <vector>

static float easeOutCubic(float t) {
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

static float easeInCubic(float t) {
    return t * t * t;
}

CIconOverlay::CIconOverlay(const std::string& appClass, PHLMONITOR monitor) : m_monitor(monitor), m_appClass(appClass) {
    m_startTime = std::chrono::steady_clock::now();
    if (g_pGlobalState) {
        m_iconSize        = g_pGlobalState->iconSize;
        m_fadeInDuration  = g_pGlobalState->fadeInMs;
        m_holdDuration    = g_pGlobalState->holdMs;
        m_fadeOutDuration = g_pGlobalState->fadeOutMs;
    }

    if (g_pGlobalState && g_pGlobalState->iconLookup) {
        auto iconPath = g_pGlobalState->iconLookup->findIconPath(appClass, m_iconSize);
        if (iconPath) {
            loadIcon(*iconPath);
        }
    }
}

CIconOverlay::~CIconOverlay() {
    if (m_hasTexture && m_textureId != 0) {
        glDeleteTextures(1, &m_textureId);
    }
}

bool CIconOverlay::loadIcon(const std::string& iconPath) {
    std::string ext = iconPath.substr(iconPath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "svg") {
        return loadSvgIcon(iconPath);
    } else if (ext == "png") {
        return loadPngIcon(iconPath);
    } else if (ext == "xpm") {
        return false;
    }

    return false;
}

bool CIconOverlay::loadSvgIcon(const std::string& path) {
    GError*    error  = nullptr;
    RsvgHandle* handle = rsvg_handle_new_from_file(path.c_str(), &error);

    if (!handle) {
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    gdouble width, height;
    rsvg_handle_get_intrinsic_size_in_pixels(handle, &width, &height);

    if (width <= 0 || height <= 0) {
        width  = m_iconSize;
        height = m_iconSize;
    }

    double scale = std::min((double)m_iconSize / width, (double)m_iconSize / height);
    int    renderWidth  = (int)(width * scale);
    int    renderHeight = (int)(height * scale);

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, renderWidth, renderHeight);
    cairo_t*         cr      = cairo_create(surface);

    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);

    cairo_scale(cr, scale, scale);

    RsvgRectangle viewport = {0, 0, width, height};
    rsvg_handle_render_document(handle, cr, &viewport, nullptr);

    cairo_destroy(cr);
    g_object_unref(handle);

    bool result = createTextureFromSurface(surface);
    cairo_surface_destroy(surface);

    return result;
}

bool CIconOverlay::loadPngIcon(const std::string& path) {
    cairo_surface_t* surface = cairo_image_surface_create_from_png(path.c_str());

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return false;
    }

    int srcWidth  = cairo_image_surface_get_width(surface);
    int srcHeight = cairo_image_surface_get_height(surface);

    if (srcWidth != m_iconSize || srcHeight != m_iconSize) {
        double scale = std::min((double)m_iconSize / srcWidth, (double)m_iconSize / srcHeight);
        int    newWidth  = (int)(srcWidth * scale);
        int    newHeight = (int)(srcHeight * scale);

        cairo_surface_t* scaledSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, newWidth, newHeight);
        cairo_t*         cr            = cairo_create(scaledSurface);

        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);

        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        surface = scaledSurface;
    }

    bool result = createTextureFromSurface(surface);
    cairo_surface_destroy(surface);

    return result;
}

bool CIconOverlay::createTextureFromSurface(cairo_surface_t* surface) {
    cairo_surface_flush(surface);

    m_textureWidth  = cairo_image_surface_get_width(surface);
    m_textureHeight = cairo_image_surface_get_height(surface);
    unsigned char* data = cairo_image_surface_get_data(surface);

    if (!data || m_textureWidth <= 0 || m_textureHeight <= 0) {
        return false;
    }

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::vector<unsigned char> rgbaData(m_textureWidth * m_textureHeight * 4);
    for (int i = 0; i < m_textureWidth * m_textureHeight; i++) {
        rgbaData[i * 4 + 0] = data[i * 4 + 2];
        rgbaData[i * 4 + 1] = data[i * 4 + 1];
        rgbaData[i * 4 + 2] = data[i * 4 + 0];
        rgbaData[i * 4 + 3] = data[i * 4 + 3];
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_textureWidth, m_textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData.data());

    glBindTexture(GL_TEXTURE_2D, 0);

    m_hasTexture = true;
    return true;
}

bool CIconOverlay::update() {
    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

    switch (m_state) {
        case ANIM_FADE_IN:
            if (elapsed < m_fadeInDuration) {
                float t  = (float)elapsed / (float)m_fadeInDuration;
                m_opacity = easeOutCubic(t);
            } else {
                m_opacity   = 1.0f;
                m_state     = ANIM_HOLD;
                m_startTime = now;
            }
            break;

        case ANIM_HOLD:
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
            if (elapsed >= m_holdDuration) {
                m_state     = ANIM_FADE_OUT;
                m_startTime = now;
            }
            m_opacity = 1.0f;
            break;

        case ANIM_FADE_OUT:
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
            if (elapsed < m_fadeOutDuration) {
                float t  = (float)elapsed / (float)m_fadeOutDuration;
                m_opacity = 1.0f - easeInCubic(t);
            } else {
                m_opacity = 0.0f;
                m_state   = ANIM_DONE;
            }
            break;

        case ANIM_DONE:
            return false;
    }

    return m_state != ANIM_DONE;
}

float CIconOverlay::getOpacity() const {
    return m_opacity;
}

bool CIconOverlay::isDone() const {
    return m_state == ANIM_DONE;
}

void CIconOverlay::draw(PHLMONITOR pMonitor) {
    if (!m_hasTexture || !m_monitor || m_opacity <= 0.0f)
        return;
    if (pMonitor != m_monitor)
        return;
    auto data = CIconPassElement::SIconData{this};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CIconPassElement>(data));
}

void CIconOverlay::renderPass() {
    if (!m_hasTexture || !m_monitor || m_opacity <= 0.0f)
        return;
    const auto& monBox = m_monitor->m_transformedSize;
    double      centerX = (monBox.x - m_textureWidth) / 2.0;
    double      centerY = (monBox.y - m_textureHeight) / 2.0;

    CBox box = {centerX, centerY, (double)m_textureWidth, (double)m_textureHeight};
    const auto TEXTURE = makeShared<CTexture>();
    TEXTURE->m_texID = m_textureId;
    TEXTURE->m_size = {m_textureWidth, m_textureHeight};
    g_pHyprOpenGL->renderTexture(TEXTURE, box, {.a = m_opacity});
}

void CIconOverlayManager::addOverlay(std::shared_ptr<CIconOverlay> overlay) {
    m_overlays.push_back(overlay);
}

void CIconOverlayManager::update() {
    m_overlays.erase(
        std::remove_if(m_overlays.begin(), m_overlays.end(),
                       [](std::shared_ptr<CIconOverlay>& overlay) {
                           if (!overlay)
                               return true;
                           overlay->update();
                           return overlay->isDone();
                       }),
        m_overlays.end());
    if (!m_overlays.empty()) {
        for (auto& m : g_pCompositor->m_monitors) {
            g_pHyprRenderer->damageMonitor(m);
        }
    }
}

void CIconOverlayManager::drawAll() {
    for (auto& overlay : m_overlays) {
        if (overlay && !overlay->isDone()) {
            overlay->draw(overlay->getMonitor());
        }
    }
}
