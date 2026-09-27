#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
container=${MIC_OVERLAY_CONTAINER:-mic-overlay-dev}
bash "$project_dir/scripts/container-setup.sh"
podman exec "$container" bash -euc '
        cmake -S /source -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release
        cmake --build /build
        ctest --test-dir /build --output-on-failure
    '
