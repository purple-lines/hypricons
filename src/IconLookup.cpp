#include "IconLookup.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <gio/gio.h>

namespace fs = std::filesystem;

CIconLookup::CIconLookup() {
    m_themePaths = getIconThemePaths();
    m_iconTheme  = getCurrentIconTheme();
    parseDesktopFiles();
}

std::string CIconLookup::getCurrentIconTheme() {
    GSettings* settings = g_settings_new("org.gnome.desktop.interface");
    if (settings) {
        gchar* theme = g_settings_get_string(settings, "icon-theme");
        if (theme) {
            std::string result(theme);
            g_free(theme);
            g_object_unref(settings);
            return result;
        }
        g_object_unref(settings);
    }

    const char* gtkTheme = std::getenv("GTK_ICON_THEME");
    if (gtkTheme)
        return std::string(gtkTheme);

    std::vector<std::string> configPaths = {
        std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.config/gtk-3.0/settings.ini",
        std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.config/gtk-4.0/settings.ini",
        std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.gtkrc-2.0",
    };

    for (const auto& path : configPaths) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("gtk-icon-theme-name") != std::string::npos) {
                    auto pos = line.find('=');
                    if (pos != std::string::npos) {
                        std::string theme = line.substr(pos + 1);
                        theme.erase(std::remove(theme.begin(), theme.end(), '"'), theme.end());
                        theme.erase(std::remove(theme.begin(), theme.end(), '\''), theme.end());
                        theme.erase(0, theme.find_first_not_of(" \t"));
                        theme.erase(theme.find_last_not_of(" \t\n\r") + 1);
                        if (!theme.empty())
                            return theme;
                    }
                }
            }
        }
    }

    return "hicolor";
}

std::vector<std::string> CIconLookup::getIconThemePaths() {
    std::vector<std::string> paths;

    const char* home = std::getenv("HOME");
    if (home) {
        paths.push_back(std::string(home) + "/.local/share/icons");
        paths.push_back(std::string(home) + "/.icons");
    }

    const char* xdgDataDirs = std::getenv("XDG_DATA_DIRS");
    if (xdgDataDirs) {
        std::stringstream ss(xdgDataDirs);
        std::string       path;
        while (std::getline(ss, path, ':')) {
            paths.push_back(path + "/icons");
        }
    } else {
        paths.push_back("/usr/local/share/icons");
        paths.push_back("/usr/share/icons");
    }

    paths.push_back("/usr/share/pixmaps");

    return paths;
}

void CIconLookup::parseDesktopFiles() {
    std::vector<std::string> desktopDirs;

    const char* home = std::getenv("HOME");
    if (home) {
        desktopDirs.push_back(std::string(home) + "/.local/share/applications");
    }

    const char* xdgDataDirs = std::getenv("XDG_DATA_DIRS");
    if (xdgDataDirs) {
        std::stringstream ss(xdgDataDirs);
        std::string       path;
        while (std::getline(ss, path, ':')) {
            desktopDirs.push_back(path + "/applications");
        }
    } else {
        desktopDirs.push_back("/usr/local/share/applications");
        desktopDirs.push_back("/usr/share/applications");
    }

    desktopDirs.push_back("/var/lib/flatpak/exports/share/applications");
    if (home) {
        desktopDirs.push_back(std::string(home) + "/.local/share/flatpak/exports/share/applications");
    }

    for (const auto& dir : desktopDirs) {
        if (!fs::exists(dir))
            continue;

        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.path().extension() != ".desktop")
                    continue;

                std::ifstream file(entry.path());
                if (!file.is_open())
                    continue;

                std::string line;
                std::string iconName;
                std::string startupWmClass;
                std::string appName;
                bool        inDesktopEntry = false;

                while (std::getline(file, line)) {
                    line.erase(0, line.find_first_not_of(" \t"));
                    line.erase(line.find_last_not_of(" \t\n\r") + 1);

                    if (line == "[Desktop Entry]") {
                        inDesktopEntry = true;
                        continue;
                    }
                    if (line.starts_with("[") && line != "[Desktop Entry]") {
                        inDesktopEntry = false;
                        continue;
                    }

                    if (!inDesktopEntry)
                        continue;

                    if (line.starts_with("Icon=")) {
                        iconName = line.substr(5);
                    } else if (line.starts_with("StartupWMClass=")) {
                        startupWmClass = line.substr(15);
                    } else if (line.starts_with("Name=") && appName.empty()) {
                        appName = line.substr(5);
                    }
                }

                if (iconName.empty())
                    continue;

                std::string desktopBasename = entry.path().stem().string();

                auto toLower = [](std::string s) {
                    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                    return s;
                };

                if (!startupWmClass.empty()) {
                    m_appToIcon[toLower(startupWmClass)] = iconName;
                }
                if (!appName.empty()) {
                    m_appToIcon[toLower(appName)] = iconName;
                }
                m_appToIcon[toLower(desktopBasename)] = iconName;

                m_appToIcon[toLower(iconName)] = iconName;
            }
        } catch (const std::exception& e) {
        }
    }
}

std::optional<std::string> CIconLookup::findIconPath(const std::string& appClass, int size) {
    std::string lowerClass = appClass;
    std::transform(lowerClass.begin(), lowerClass.end(), lowerClass.begin(), ::tolower);

    std::string iconName = lowerClass;
    auto        it       = m_appToIcon.find(lowerClass);
    if (it != m_appToIcon.end()) {
        iconName = it->second;
    }

    if (iconName.starts_with("/") && fs::exists(iconName)) {
        return iconName;
    }

    auto result = searchIconInTheme(iconName, size);
    if (result)
        return result;

    result = searchIconInHicolor(iconName, size);
    if (result)
        return result;

    result = searchIconInPixmaps(iconName);
    if (result)
        return result;

    if (iconName != lowerClass) {
        result = searchIconInTheme(lowerClass, size);
        if (result)
            return result;
        result = searchIconInHicolor(lowerClass, size);
        if (result)
            return result;
        result = searchIconInPixmaps(lowerClass);
        if (result)
            return result;
    }

    return std::nullopt;
}

std::optional<std::string> CIconLookup::searchIconInTheme(const std::string& iconName, int size) {
    std::vector<int> sizes = {size, 256, 128, 96, 72, 64, 48, 32, 24, 22, 16};
    std::vector<std::string> extensions = {".svg", ".png", ".xpm"};

    for (const auto& basePath : m_themePaths) {
        std::string themePath = basePath + "/" + m_iconTheme;
        if (!fs::exists(themePath))
            continue;

        for (const auto& subdir : {"scalable/apps", "scalable/applications", "scalable"}) {
            std::string path = themePath + "/" + subdir + "/" + iconName + ".svg";
            if (fs::exists(path))
                return path;
        }

        for (int s : sizes) {
            for (const auto& subdir : {"apps", "applications", ""}) {
                for (const auto& ext : extensions) {
                    std::string sizeStr = std::to_string(s) + "x" + std::to_string(s);
                    std::string path    = themePath + "/" + sizeStr + "/" + subdir + "/" + iconName + ext;
                    if (fs::exists(path))
                        return path;
                    path = themePath + "/" + sizeStr + "/" + iconName + ext;
                    if (fs::exists(path))
                        return path;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> CIconLookup::searchIconInHicolor(const std::string& iconName, int size) {
    std::vector<int>         sizes      = {size, 256, 128, 96, 72, 64, 48, 32, 24, 22, 16};
    std::vector<std::string> extensions = {".svg", ".png", ".xpm"};

    for (const auto& basePath : m_themePaths) {
        std::string themePath = basePath + "/hicolor";
        if (!fs::exists(themePath))
            continue;

        for (const auto& subdir : {"scalable/apps", "scalable/applications", "scalable"}) {
            std::string path = themePath + "/" + subdir + "/" + iconName + ".svg";
            if (fs::exists(path))
                return path;
        }

        for (int s : sizes) {
            for (const auto& subdir : {"apps", "applications", ""}) {
                for (const auto& ext : extensions) {
                    std::string sizeStr = std::to_string(s) + "x" + std::to_string(s);
                    std::string path    = themePath + "/" + sizeStr + "/" + subdir + "/" + iconName + ext;
                    if (fs::exists(path))
                        return path;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> CIconLookup::searchIconInPixmaps(const std::string& iconName) {
    std::vector<std::string> extensions = {".svg", ".png", ".xpm", ""};

    for (const auto& ext : extensions) {
        std::string path = "/usr/share/pixmaps/" + iconName + ext;
        if (fs::exists(path))
            return path;
    }

    return std::nullopt;
}

void CIconLookup::refreshCache() {
    m_appToIcon.clear();
    m_iconTheme = getCurrentIconTheme();
    parseDesktopFiles();
}
