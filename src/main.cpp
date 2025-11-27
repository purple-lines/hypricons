#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <any>

#include "globals.hpp"
#include "IconOverlay.hpp"
#include "IconLookup.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

static int onTick(void* data) {
    if (!g_pGlobalState || !g_pGlobalState->overlayManager)
        return 0;

    g_pGlobalState->overlayManager->update();

    if (g_pGlobalState->overlayManager->hasActiveOverlays()) {
        const int TIMEOUT = g_pHyprRenderer->m_mostHzMonitor ? 1000.0 / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate : 16;
        wl_event_source_timer_update(g_pGlobalState->tickSource, TIMEOUT);
    }

    return 0;
}

static void onOpenWindow(void* self, std::any data) {
    if (!g_pGlobalState || !g_pGlobalState->enabled)
        return;

    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
    if (!PWINDOW)
        return;

    std::string appClass = PWINDOW->m_class;

    if (appClass.empty()) {
        appClass = PWINDOW->m_initialClass;
    }

    if (appClass.empty()) {
        appClass = PWINDOW->m_title;
    }

    if (appClass.empty())
        return;

    PHLMONITOR monitor = PWINDOW->m_monitor.lock();
    if (!monitor) {
        monitor = g_pCompositor->m_lastMonitor.lock();
    }

    if (!monitor)
        return;

    auto overlay = std::make_shared<CIconOverlay>(appClass, monitor);

    if (overlay->getTextureId() != 0) {
        g_pGlobalState->overlayManager->addOverlay(overlay);
        g_pHyprRenderer->damageMonitor(monitor);
        if (g_pGlobalState->tickSource) {
            wl_event_source_timer_update(g_pGlobalState->tickSource, 1);
        }
    }
}

static void refreshConfig() {
    if (!g_pGlobalState)
        return;

    static auto* const PENABLED    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hypricons:enabled")->getDataStaticPtr();
    static auto* const PICONSIZE   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hypricons:icon_size")->getDataStaticPtr();
    static auto* const PFADEIN     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hypricons:fade_in_ms")->getDataStaticPtr();
    static auto* const PHOLD       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hypricons:hold_ms")->getDataStaticPtr();
    static auto* const PFADEOUT    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hypricons:fade_out_ms")->getDataStaticPtr();

    g_pGlobalState->enabled    = **PENABLED;
    g_pGlobalState->iconSize   = **PICONSIZE;
    g_pGlobalState->fadeInMs   = **PFADEIN;
    g_pGlobalState->holdMs     = **PHOLD;
    g_pGlobalState->fadeOutMs  = **PFADEOUT;
}

static void onConfigReloaded(void* self, std::any data) {
    refreshConfig();
    if (g_pGlobalState && g_pGlobalState->iconLookup) {
        g_pGlobalState->iconLookup->refreshCache();
    }
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hypricons] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hypricons] Version mismatch");
    }

    g_pGlobalState = std::make_unique<SGlobalState>();
    g_pGlobalState->iconLookup = std::make_unique<CIconLookup>();
    g_pGlobalState->overlayManager = std::make_unique<CIconOverlayManager>();

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hypricons:enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hypricons:icon_size", Hyprlang::INT{128});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hypricons:fade_in_ms", Hyprlang::INT{150});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hypricons:hold_ms", Hyprlang::INT{300});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hypricons:fade_out_ms", Hyprlang::INT{400});

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow",
        [&](void* self, SCallbackInfo& info, std::any data) { onOpenWindow(self, data); });

    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded",
        [&](void* self, SCallbackInfo& info, std::any data) { onConfigReloaded(self, data); });

    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "render",
        [&](void* self, SCallbackInfo& info, std::any data) {
            if (g_pGlobalState && g_pGlobalState->overlayManager) {
                g_pGlobalState->overlayManager->drawAll();
            }
        });

    g_pGlobalState->tickSource = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, &onTick, nullptr);
    HyprlandAPI::reloadConfig();
    refreshConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hypricons] Initialized successfully! App icons will appear when launching apps.",
                                 CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hypricons", "Shows app icon briefly when launching applications", "PurpleLines", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_pGlobalState && g_pGlobalState->tickSource) {
        wl_event_source_remove(g_pGlobalState->tickSource);
    }
    g_pHyprRenderer->m_renderPass.removeAllOfType("CIconPassElement");
    g_pGlobalState.reset();
}
