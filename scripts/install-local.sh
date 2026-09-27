#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
prefix="$HOME/.local"
staged="$project_dir/build-container/install-stage$prefix"
for file in bin/mic-overlay share/applications/mic-overlay.desktop \
    share/icons/hicolor/scalable/apps/mic-overlay.svg \
    share/mic-overlay/config.ini.example share/doc/mic-overlay/installation.md \
    share/doc/mic-overlay/configuration.md share/doc/mic-overlay/packaging.md share/doc/mic-overlay/acceptance.md \
    share/licenses/mic-overlay/LICENSE; do
    if [[ ! -f "$staged/$file" ]]; then
        echo "Prepare first: bash scripts/prepare-install.sh" >&2
        exit 1
    fi
done
install -CDm755 "$staged/bin/mic-overlay" "$prefix/bin/mic-overlay"
for file in share/applications/mic-overlay.desktop share/icons/hicolor/scalable/apps/mic-overlay.svg share/mic-overlay/config.ini.example \
    share/doc/mic-overlay/installation.md share/doc/mic-overlay/configuration.md \
    share/doc/mic-overlay/packaging.md share/doc/mic-overlay/acceptance.md share/licenses/mic-overlay/LICENSE; do
    install -Dm644 "$staged/$file" "$prefix/$file"
done
config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/mic-overlay"
mkdir -p -- "$config_dir"
# Never replace an existing user configuration.
if [[ ! -e "$config_dir/config.ini" && ! -L "$config_dir/config.ini" ]]; then
    install -m644 "$staged/share/mic-overlay/config.ini.example" "$config_dir/config.ini"
fi
echo "Installed: $prefix/bin/mic-overlay"
echo "Configuration: $config_dir/config.ini"
echo "Launch Microphone Overlay from the application menu."
echo "Use Configure in the tray menu to enable desktop autostart."
