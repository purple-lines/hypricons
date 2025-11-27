# hypricons

A Hyprland plugin that displays the application icon briefly in the center of the screen when you launch an app. The icon fades in quickly and fades out smoothly, providing visual feedback for app launches.

![Hyprland](https://img.shields.io/badge/Hyprland-Plugin-blue)
![License](https://img.shields.io/badge/License-MIT-green)

## Features

- 🎨 **SVG & PNG Support** - Renders high-quality icons from your icon theme
- ⚡ **Smooth Animations** - Fast fade-in, brief hold, smooth fade-out
- 🔧 **Configurable** - Customize icon size and animation timings
- 🎯 **Smart Icon Lookup** - Finds icons from desktop files, icon themes, and pixmaps
- 🖥️ **Multi-monitor** - Shows icon on the monitor where the app opens

## Installation

### Using hyprpm (Recommended)

```bash
hyprpm add https://github.com/purple-lines/hypricons
hyprpm enable hypricons
```

### Manual Build

**Dependencies:**
- Hyprland (with headers)
- meson & ninja
- librsvg
- cairo
- pango
- glib/gio

```bash
# Clone the repository
git clone https://github.com/purple-lines/hypricons.git
cd hypricons

# Build
meson setup build
ninja -C build

# The plugin will be at build/libhypricons.so
```

**Load the plugin:**

Add to your `hyprland.conf`:
```ini
exec-once = hyprctl plugin load /path/to/libhypricons.so
```

Or use hyprpm:
```bash
hyprpm enable hypricons
```

## Configuration

Add these to your `hyprland.conf`:

```ini
plugin {
    hypricons {
        # Enable/disable the plugin
        enabled = true

        # Icon size in pixels
        icon_size = 128

        # Fade-in duration (milliseconds) - fast appearance
        fade_in_ms = 150

        # Hold duration (milliseconds) - how long icon stays fully visible
        hold_ms = 300

        # Fade-out duration (milliseconds) - smooth disappearance
        fade_out_ms = 400
    }
}
```

### Default Values

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | `true` | Enable/disable the plugin |
| `icon_size` | `128` | Size of the displayed icon in pixels |
| `fade_in_ms` | `150` | Duration of fade-in animation |
| `hold_ms` | `300` | Duration icon stays at full opacity |
| `fade_out_ms` | `400` | Duration of fade-out animation |

## How It Works

1. **Window Detection** - Listens for the `openWindow` event from Hyprland
2. **Icon Lookup** - Searches for the app icon using:
   - Desktop file entries (`.desktop` files)
   - Current icon theme (from GTK settings)
   - Hicolor fallback theme
   - `/usr/share/pixmaps`
3. **Rendering** - Renders the icon as an overlay in the center of the monitor
4. **Animation** - Applies smooth easing functions for natural-feeling animations

## Icon Theme Support

The plugin automatically detects your icon theme from:
- GSettings (`org.gnome.desktop.interface icon-theme`)
- GTK config files (`~/.config/gtk-3.0/settings.ini`, etc.)
- Environment variable `GTK_ICON_THEME`

It searches for icons in standard locations:
- `~/.local/share/icons`
- `~/.icons`
- `/usr/share/icons`
- `/usr/local/share/icons`
- `/usr/share/pixmaps`

## Troubleshooting

### No icon appears
- Check if the app has a proper `.desktop` file with an `Icon=` entry
- Verify your icon theme has the required icons
- Try setting `icon_size` to a common size like 48, 64, or 128

### Icon looks wrong
- The plugin prefers SVG icons for best quality
- Ensure your icon theme has scalable icons

### Performance issues
- Reduce `icon_size` for faster rendering
- Shorten animation durations

## Building from Source

```bash
# Install dependencies (Arch Linux)
sudo pacman -S hyprland meson ninja librsvg cairo pango glib2

# Build
meson setup build --buildtype=release
ninja -C build

# Install system-wide (optional)
sudo ninja -C build install
```

## License

MIT License - see LICENSE file for details.

## Credits

- Inspired by macOS app launch animations
- Built for [Hyprland](https://hyprland.org/)
- Uses [librsvg](https://wiki.gnome.org/Projects/LibRsvg) for SVG rendering
