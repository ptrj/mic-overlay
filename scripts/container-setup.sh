#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
image=${MIC_OVERLAY_IMAGE:-localhost/mic-overlay-dev:latest}
container=${MIC_OVERLAY_CONTAINER:-mic-overlay-dev}

if ! podman container exists "$container"; then
    if ! podman image exists "$image"; then
        podman build -t "$image" -f "$project_dir/containers/Containerfile" "$project_dir/containers"
    fi
    : "${XDG_RUNTIME_DIR:?Run initial setup from your KDE Wayland session}"
    : "${WAYLAND_DISPLAY:?Run initial setup from your KDE Wayland session}"
    case "$WAYLAND_DISPLAY" in
        /*) wayland_socket=$WAYLAND_DISPLAY ;;
        *) wayland_socket="$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ;;
    esac
    if [[ ! -S "$wayland_socket" ]]; then
        echo "Wayland socket does not exist: $wayland_socket" >&2
        exit 1
    fi
    mkdir -p "$project_dir/build-container"
    podman create --name "$container" --init --userns=keep-id \
        --label mic-overlay.development=true \
        -v "$project_dir:/source:ro" \
        -v "$project_dir/build-container:/build:rw" \
        -v "$wayland_socket:/tmp/mic-overlay-wayland:ro" \
        -v "$XDG_RUNTIME_DIR:/host-runtime:ro" \
        -e XDG_RUNTIME_DIR=/tmp/mic-overlay-runtime \
        -e WAYLAND_DISPLAY=wayland-0 \
        -e QT_QPA_PLATFORM=wayland \
        -e QT_QUICK_BACKEND=software \
        -e PIPEWIRE_RUNTIME_DIR=/host-runtime \
        "$image" bash -euc '
            mkdir -p -m 700 "$XDG_RUNTIME_DIR"
            ln -sf /tmp/mic-overlay-wayland "$XDG_RUNTIME_DIR/wayland-0"
            exec sleep infinity
        '
fi

if [[ $(podman inspect --format '{{.State.Running}}' "$container") != true ]]; then
    podman start "$container"
fi
echo "Development container ready: $container"
