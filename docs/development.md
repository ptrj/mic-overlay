# Development with Podman

The persistent Arch container is named `mic-overlay-dev`. Sources are mounted
read-only at `/source`; build outputs are writable at `/build` and appear in
`build-container/` on the host.

```bash
bash scripts/container-build.sh
podman exec mic-overlay-dev ctest --test-dir /build --output-on-failure
bash scripts/container-run.sh --quit-after 30
```

The setup script creates or starts the container as needed. It installs build
dependencies inside the container, not on the host. Backend and desktop UI tests
use private fixtures. Optional real audio integration tests start isolated
PipeWire/WirePlumber/D-Bus instances and do not change host microphone settings.

```bash
podman exec mic-overlay-dev bash /source/tests/pipewire_integration.sh
podman exec -e MIC_OVERLAY_INITIAL_MUTED=1 mic-overlay-dev bash /source/tests/pipewire_integration.sh
podman exec mic-overlay-dev bash /source/tests/shutdown_smoke.sh
podman exec mic-overlay-dev bash /source/tests/gui_config_smoke.sh
podman exec mic-overlay-dev /build/overlay-controller-test
```

GUI tests briefly display indicators on the real KWin desktop. They do not
reconfigure host monitors. The controller tests simulate topology changes using
existing screens. GUI protocol checks do not replace manual game testing.

## Native host launch

The container-built executable can run directly on a compatible Arch/CachyOS
host with matching system libraries:

```bash
bash scripts/native-run.sh
bash scripts/native-run.sh --config config.local.ini
```

Use the tray menu to configure or exit. Ctrl+C also terminates a foreground
launch. `--quit-after 180` is useful for bounded demos; `--no-tray` is available
for overlay-only automated tests. `--capture /path/image.png` captures the client
window, not the composited desktop. `--list-screens` prints monitor indices.

For a larger demo use `config/demo-primary.ini` or `config/demo-secondary.ini`.
For installation independent of the build directory, see
[installation](installation.md). Distribution builds are covered in
[packaging](packaging.md).

## Runtime behavior

The first state query is immediate. A persistent `pw-dump --monitor --no-colors`
process triggers `wpctl get-volume @DEFAULT_AUDIO_SOURCE@` for source changes,
backing capture-device route/parameter changes and default capture metadata changes. Events are batched for 200 ms. Unrelated
client/playback changes are ignored. Healthy idle monitoring does not poll.

A missing/unhealthy monitor or unavailable source enables 500 ms fallback
polling. Reconnect attempts are spaced 3 s apart; initialization has a 2 s
timeout. SIGINT/SIGTERM exit the Qt loop and reap both owned processes.
The application never toggles mute or changes audio routing.

## Container management

```bash
podman exec -it mic-overlay-dev bash
podman stop mic-overlay-dev
podman start mic-overlay-dev
```

`MIC_OVERLAY_IMAGE` and `MIC_OVERLAY_CONTAINER` override the default image/name.
The Wayland socket and host session runtime are mounted read-only for GUI and
audio observation. GPU devices are not shared; tests use software rendering.
The container does not share the host session D-Bus, so tray registration is best
tested by native launch. It can still run overlay-only GUI tests.

After logout or a KWin restart, recreate a container with a stale Wayland socket:

```bash
podman stop mic-overlay-dev
podman rm mic-overlay-dev
bash scripts/container-setup.sh
```

Build outputs remain in the project. Arch package versions depend on the image
build date. See [acceptance](acceptance.md) for final checks and measurement.

## Preparing a GitHub repository

The source repository includes code, tests, container recipes, package recipes,
assets and documentation. `.gitignore` excludes build outputs, generated packages,
private `config.local.ini`, diagnostics and local agent files. Upload DEB/RPM/Arch
packages as GitHub Release assets, rather than committing them to Git history.

To create a separate clean source tree without deleting your local builds:

```bash
bash scripts/prepare-github.sh
```

The default output is `build-container/github/mic-overlay/`. An optional argument
selects another new directory. The script refuses to overwrite an existing
folder. Exported sources include their own `.gitignore` and `.gitattributes`.
Local build outputs, container configuration and personal settings stay intact
in the original project. Make future code changes in the original `src/` and
`tests/`, then export to a new directory when needed.

After creating a GitHub repository, initialize Git in the exported directory
and follow GitHub's instructions to add the remote and push. The preparation
script does not contact GitHub or publish anything. The upstream remote is `https://github.com/ptrj/mic-overlay.git`. Package metadata
already uses the public repository URL. Upload the versioned source release
asset before publishing to AUR.
