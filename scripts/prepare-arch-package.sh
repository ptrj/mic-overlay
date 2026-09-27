#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
container=${MIC_OVERLAY_CONTAINER:-mic-overlay-dev}
bash "$project_dir/scripts/container-setup.sh"
podman exec "$container" bash -euc '
    cmake -S /source -B /build/arch-release -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    mkdir -p /build/packages/arch
    cpack --config /build/arch-release/CPackSourceConfig.cmake -G TGZ -B /build/packages/arch
    cp /source/packaging/arch/PKGBUILD /build/packages/arch/PKGBUILD
    cd /build/packages/arch
    version=$(sed -n "s/^pkgver=//p" PKGBUILD)
    archive="mic-overlay-$version.tar.gz"
    test -f "$archive"
    checksum=$(sha256sum "$archive")
    checksum=${checksum%% *}
    sed -i "s/sha256sums=(.*)/sha256sums=(\x27$checksum\x27)/" PKGBUILD
    makepkg --printsrcinfo > .SRCINFO
    makepkg --verifysource --force
'
echo "Arch release prepared: $project_dir/build-container/packages/arch"
