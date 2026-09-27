#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
binary="$project_dir/build-container/mic-overlay"
if [[ ! -x "$binary" ]]; then
    echo "Build first: bash scripts/container-build.sh" >&2
    exit 1
fi
# The executable was built in Podman, but runs here as a native host process.
export QT_QPA_PLATFORM=wayland
export QT_QUICK_BACKEND=${QT_QUICK_BACKEND:-software}
export QT_FORCE_STDERR_LOGGING=1
exec "$binary" "$@"
