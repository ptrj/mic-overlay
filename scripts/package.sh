#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
case "${1:-}" in
    deb) format=deb; generator=DEB ;;
    rpm) format=rpm; generator=RPM ;;
    *) echo "Usage: bash scripts/package.sh deb|rpm" >&2; exit 1 ;;
esac
image="localhost/mic-overlay-package-$format:latest"
mkdir -p "$project_dir/build-container/package-$format"
podman build -t "$image" -f "$project_dir/containers/Containerfile.$format" "$project_dir/containers"
podman run --rm --network=none \
    -v "$project_dir:/source:ro" \
    -v "$project_dir/build-container/package-$format:/build:rw" \
    "$image" bash -euc '
        cmake -S /source -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/usr
        cmake --build /build --parallel 2
        ctest --test-dir /build --output-on-failure
        cpack --config /build/CPackConfig.cmake -G "$1" -B /build/packages
    ' bash "$generator"
echo "Packages: $project_dir/build-container/package-$format/packages"
