#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
prefix=${1:-"$HOME/.local"}
if [[ "$prefix" != /* || "$prefix" == *$'\n'* ]]; then
    echo "Installation prefix must be an absolute path without newlines." >&2
    exit 1
fi
container=${MIC_OVERLAY_CONTAINER:-mic-overlay-dev}
bash "$project_dir/scripts/container-setup.sh"
podman exec "$container" bash -euc '
    cmake -S /source -B /build/install-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$1"
    cmake --build /build/install-build
    DESTDIR=/build/install-stage cmake --install /build/install-build
' bash "$prefix"
echo "Prepared installation: $project_dir/build-container/install-stage$prefix"
