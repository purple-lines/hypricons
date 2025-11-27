#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/helpers/Color.hpp>

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <optional>

inline HANDLE PHANDLE = nullptr;

class CIconOverlay;
struct SGlobalState;

inline std::unique_ptr<SGlobalState> g_pGlobalState = nullptr;
