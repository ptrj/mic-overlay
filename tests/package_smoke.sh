#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=$(awk '/^project\(mic-overlay VERSION / {print $3}' "$project_dir/CMakeLists.txt")
case "${1:-}" in
    deb|rpm)
        format=$1
        image="localhost/mic-overlay-package-$format:latest"
        package_dir="$project_dir/build-container/package-$format/packages"
        ;;
    arch)
        format=arch
        image=${MIC_OVERLAY_IMAGE:-localhost/mic-overlay-dev:latest}
        package_dir="$project_dir/build-container/packages/arch"
        ;;
    *) echo "Usage: bash tests/package_smoke.sh deb|rpm|arch" >&2; exit 1 ;;
esac
: "${XDG_RUNTIME_DIR:?Run from a KDE Wayland session}"
: "${WAYLAND_DISPLAY:?Run from a KDE Wayland session}"
podman run --rm --network=none --user 0 \
    -v "$package_dir:/packages:ro" \
    -v "$XDG_RUNTIME_DIR:/runtime:ro" \
    -e XDG_RUNTIME_DIR=/runtime -e WAYLAND_DISPLAY \
    -e QT_QPA_PLATFORM=wayland -e QT_QUICK_BACKEND=software \
    -e QT_FORCE_STDERR_LOGGING=1 \
    -e LANG=C.UTF-8 \
    "$image" bash -euc '
        set -o pipefail
        export PIPEWIRE_RUNTIME_DIR=/runtime
        export XDG_RUNTIME_DIR=$(mktemp -d /tmp/mic-overlay-runtime.XXXXXX)
        export WAYLAND_DISPLAY="/runtime/${WAYLAND_DISPLAY##*/}"
        if [[ "$1" == deb ]]; then
            packages=(/packages/mic-overlay_"$2"-*.deb)
            dpkg -i "${packages[@]}"
            dpkg-deb -f "${packages[0]}" Package Version Architecture Depends
        elif [[ "$1" == rpm ]]; then
            packages=(/packages/mic-overlay-"$2"-*.rpm)
            rpm -Uvh "${packages[@]}"
            rpm -q mic-overlay --qf "%{NAME} %{VERSION} %{ARCH} %{LICENSE}\n"
        else
            packages=(/packages/mic-overlay-"$2"-*-"$(uname -m)".pkg.tar.zst)
            pacman -U --noconfirm "${packages[@]}"
            pacman -Q mic-overlay
        fi
        test -f /usr/share/licenses/mic-overlay/LICENSE
        test -f /usr/share/mic-overlay/config.ini.example
        test -f /usr/share/applications/mic-overlay.desktop
        grep -Fx "Exec=\"/usr/bin/mic-overlay\"" /usr/share/applications/mic-overlay.desktop
        set +e
        QT_QPA_PLATFORM=offscreen /usr/bin/mic-overlay > /tmp/platform.log 2>&1
        status=$?
        set -e
        test "$status" = 1
        grep -F "requires the Qt Wayland platform plugin" /tmp/platform.log
        sed "s/hideWhenUnavailable=true/hideWhenUnavailable=false/" \
            /usr/share/mic-overlay/config.ini.example > /tmp/config.ini
        /usr/bin/mic-overlay --config /tmp/config.ini --quit-after 2 \
            --capture /tmp/overlay.png 2>&1 | tee /tmp/gui.log
        if grep -E "QML Image: Error|could not save overlay capture" /tmp/gui.log; then
            exit 1
        fi
        test -s /tmp/overlay.png
        echo "PASS: installed package, license, desktop launcher, platform rejection and real Wayland startup"
    ' bash "$format" "$version"
