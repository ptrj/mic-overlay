# Installation and startup

Microphone Overlay 0.2 is a native KDE Plasma Wayland application with a system
tray menu. It does not require a background service or a process supervisor.

## Local installation from Podman

```bash
bash scripts/prepare-install.sh
bash scripts/install-local.sh
```

This installs the executable into `~/.local/bin`, an application launcher into
`~/.local/share/applications`, the icon, English documentation and GPL-3.0-only
license into `~/.local/share`. The installer creates
`~/.config/mic-overlay/config.ini` only if it does not already exist.
It respects `XDG_CONFIG_HOME`. Existing configuration is preserved.

Launch **Microphone Overlay** from the KDE application menu, or run:

```bash
~/.local/bin/mic-overlay
~/.local/bin/mic-overlay --configure
```

The tray menu provides **Configure**, **About** and **Exit**. Closing Configure
keeps the application running and locks the overlay. Exit stops the application
and its own PipeWire monitor. A second launch opens the existing instance's
settings for the same configuration file.

## Autostart

In Configure, enable **Start automatically after login**, then Apply or OK.
The application writes `~/.config/autostart/mic-overlay.desktop` pointing to the
current executable and configuration. Disabling the option removes that entry.
Autostart takes effect at the next KDE login. It does not restart the application
after Exit. `XDG_CONFIG_HOME` is respected.

Command-line equivalents:

```bash
~/.local/bin/mic-overlay --enable-autostart
~/.local/bin/mic-overlay --disable-autostart
```

Keep the executable at its installed location after enabling autostart. Using a
binary from the build directory makes autostart depend on that build directory.
The normal application launcher is installed separately and does not enable
autostart for users automatically.

## Migrating from the old service version

Disable the previous service once before starting the tray version:

```bash
systemctl --user disable --now mic-overlay.service
```

Remove the old `mic-overlay.service` file from the location where it was installed
and reload the user manager. The new application does not install or use that
service. Do not run both startup mechanisms. Existing INI settings remain usable.

## Build without Podman

Arch/CachyOS build dependencies:

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative \
    qt6-wayland qt6-svg layer-shell-qt pipewire wireplumber
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

For a native local installation, configure with `-DCMAKE_INSTALL_PREFIX="$HOME/.local"`
and install without sudo. Configure the prefix before building: the generated
desktop launcher embeds the executable path. Changing only the install-time
prefix does not update that launcher.

Runtime requirements are Qt6 Core/Gui/Widgets/Quick/QML/SVG, Qt Wayland,
LayerShellQt >= 6.6, PipeWire (`pw-dump`), WirePlumber (`wpctl`) and a font.
The application uses system libraries; an Arch-built executable requires a
compatible host. After installation, Podman is not needed to run it.

To uninstall a local version, first disable autostart in Configure and select
Exit. Remove the installed executable, application launcher, icon, documentation
and license. User configuration can be kept for a later installation.

See [configuration](configuration.md), [packaging](packaging.md) and
[acceptance checks](acceptance.md). License: GPL-3.0-only, see `LICENSE`.
