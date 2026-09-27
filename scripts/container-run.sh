#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
container=${MIC_OVERLAY_CONTAINER:-mic-overlay-dev}
if [[ ! -x "$project_dir/build-container/mic-overlay" ]]; then
    echo "Build first: bash scripts/container-build.sh" >&2
    exit 1
fi
bash "$project_dir/scripts/container-setup.sh"
# Uses the selected Wayland socket and the host PipeWire session.
exec podman exec \
    -e QT_FORCE_STDERR_LOGGING=1 \
    -e WAYLAND_DEBUG="${WAYLAND_DEBUG:-0}" \
    "$container" /build/mic-overlay "$@"
