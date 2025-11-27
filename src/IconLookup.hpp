#pragma once

#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <filesystem>

class CIconLookup {
  public:
    CIconLookup();
    ~CIconLookup() = default;

    std::optional<std::string> findIconPath(const std::string& appClass, int size = 128);

    void refreshCache();

  private:
    void parseDesktopFiles();
    std::optional<std::string> searchIconInTheme(const std::string& iconName, int size);
    std::optional<std::string> searchIconInHicolor(const std::string& iconName, int size);
    std::optional<std::string> searchIconInPixmaps(const std::string& iconName);
    std::string getCurrentIconTheme();
    std::vector<std::string> getIconThemePaths();

    std::unordered_map<std::string, std::string> m_appToIcon;
    std::string m_iconTheme;
    std::vector<std::string> m_themePaths;
};
