# Microphone Overlay

<img src="assets/mic-overlay.svg" alt="Microphone Overlay icon" width="96" height="96">

A small microphone status overlay for **KDE Plasma 6 on Wayland**. It observes
the actual mute state of the default PipeWire microphone and stays visible over
applications and fullscreen games.

Project: [ptrj/mic-overlay](https://github.com/ptrj/mic-overlay)

Author: **ptrj** · Contact: **aur@dbs.sk** · License: **GPL-3.0-only**

## Features

- Red indicator when muted and green when the microphone is on.
- System tray menu with Configure, About and Exit.
- Free dragging while Configure is open; closing it restores locked click-through.
- Reset position to the selected monitor's center.
- Icon-only or icon-and-text display, colors, fonts and background opacity.
- Optional desktop autostart after login.
- Default microphone tracking and recovery after device disconnects.

## Requirements

KDE Plasma Wayland, PipeWire, WirePlumber, Qt 6.5 or newer and LayerShellQt 6.6
or newer. The project has been tested on CachyOS/Arch with KWin Wayland.
DEB and RPM builds are tested in Ubuntu 26.04 and Fedora 44 containers.

## Build and install

On Arch/CachyOS:

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative \
    qt6-wayland qt6-svg layer-shell-qt pipewire wireplumber
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Launch **Microphone Overlay** from the application menu or run:

```bash
~/.local/bin/mic-overlay
```

Open **Configure** from the tray to move the overlay and adjust its appearance.
Settings are stored in `~/.config/mic-overlay/config.ini`. Enable
**Start automatically after login** in Configure for desktop autostart.

For a persistent Podman development environment:

```bash
bash scripts/container-build.sh
bash scripts/native-run.sh
```

## Documentation

- [Installation and startup](docs/installation.md)
- [Configuration](docs/configuration.md)
- [Podman development and tests](docs/development.md)
- [DEB, RPM and Arch/AUR packaging](docs/packaging.md)
- [Acceptance checks](docs/acceptance.md) and [verification record](docs/verification.md)
- [Technical specification](docs/specification.md)
- [Contributing](CONTRIBUTING.md) and [implementation plan](TODO.md)

The user has confirmed working behavior in multiple games, login autostart,
physical microphone changes and dragging. Monitor hotplug/mixed DPI and clean
Arch chroot validation remain open. See the acceptance record for scope.

Generated packages belong in GitHub Releases. Build directories, private local
configuration and diagnostics are excluded from Git. See
[repository preparation](docs/development.md#preparing-a-github-repository).

## License

[GNU General Public License version 3 only](LICENSE).
