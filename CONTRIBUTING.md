# Contributing

Report bugs with your distribution, KDE/KWin, Qt and LayerShellQt versions,
monitor layout/scaling and steps to reproduce. Describe whether the overlay was
locked or Configure was open. For gaming issues include the game, fullscreen
mode and native/Proton runtime.

Use C++20 and Qt6. Keep the overlay observer-only: it must not change microphone
mute state or audio routing. Preserve locked click-through, keyboard focus
behavior and event-driven idle monitoring. Project documentation is in English.

## Development

```bash
bash scripts/container-build.sh
```

This builds the application and runs CTest in the persistent Podman container.
GUI tests require a KDE Wayland session:

```bash
podman exec mic-overlay-dev bash /source/tests/tray_gui_smoke.sh
podman exec mic-overlay-dev /build/overlay-controller-test
```

See [development](docs/development.md) for isolated audio tests and other checks.
Run checks appropriate to the change. Never record manual acceptance as passed
without running it or an explicit user report.

Keep generated binaries, packages, diagnostic logs and personal configuration
out of source commits. Edit files in `src/` and `tests/`, not extracted package
sources under `build-container/packages/arch/src/`.

For releases, keep CMake and the Arch PKGBUILD versions aligned and follow
[packaging](docs/packaging.md). Release artifacts are uploaded separately from
Git source history. The upstream repository is https://github.com/ptrj/mic-overlay. Upload the
versioned source release asset and verify its checksum before AUR publication.
