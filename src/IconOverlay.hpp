#pragma once

#include "globals.hpp"
#include "IconLookup.hpp"

#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

#include <cairo/cairo.h>
#include <librsvg/rsvg.h>
#include <GLES3/gl32.h>

#include <chrono>
#include <string>
#include <memory>

enum eAnimationState {
    ANIM_FADE_IN,
    ANIM_HOLD,
    ANIM_FADE_OUT,
    ANIM_DONE
};

class CIconOverlay {
  public:
    CIconOverlay(const std::string& appClass, PHLMONITOR monitor);
    ~CIconOverlay();

    bool update();
    float getOpacity() const;
    bool isDone() const;
    PHLMONITOR getMonitor() const { return m_monitor; }
    void draw(PHLMONITOR pMonitor);
    void renderPass();
    GLuint getTextureId() const { return m_textureId; }
    int getIconSize() const { return m_iconSize; }

  private:
    bool loadIcon(const std::string& iconPath);
    bool loadSvgIcon(const std::string& path);
    bool loadPngIcon(const std::string& path);
    bool createTextureFromSurface(cairo_surface_t* surface);

    PHLMONITOR m_monitor;
    std::string m_appClass;
    std::chrono::steady_clock::time_point m_startTime;
    eAnimationState m_state = ANIM_FADE_IN;
    int m_fadeInDuration  = 150;
    int m_holdDuration    = 300;
    int m_fadeOutDuration = 400;
    float m_opacity = 0.0f;
    GLuint m_textureId = 0;
    bool m_hasTexture = false;
    int m_iconSize = 128;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
};

class CIconOverlayManager {
  public:
    CIconOverlayManager() = default;
    ~CIconOverlayManager() = default;

    void addOverlay(std::shared_ptr<CIconOverlay> overlay);
    void update();
    void drawAll();
    bool hasActiveOverlays() const { return !m_overlays.empty(); }

  private:
    std::vector<std::shared_ptr<CIconOverlay>> m_overlays;
};

struct SGlobalState {
    std::unique_ptr<CIconLookup>        iconLookup;
    std::unique_ptr<CIconOverlayManager> overlayManager;
    wl_event_source*                    tickSource = nullptr;
    int   iconSize       = 128;
    int   fadeInMs       = 150;
    int   holdMs         = 300;
    int   fadeOutMs      = 400;
    bool  enabled        = true;
};
