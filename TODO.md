# Implementation plan

Priority: P0 feasibility, P1 application features, P2 packaging/acceptance,
P3 optional extensions. Historical automated results are in docs/verification.md.
Manual tests must be marked complete only after actual execution.

## 1. P0 — Build and Wayland overlay

- [x] C++20/CMake, Qt6/QML, LayerShellQt, embedded SVG assets.
- [x] LayerOverlay, no workspace reservation, no keyboard input.
- [x] Locked overlay uses an empty mouse input region and no focus activation.
- [x] Persistent Podman development container and native host launch.
- [x] Physical fullscreen/game input checks and absence from Alt+Tab/taskbar (user confirmed 2026-09-27).

## 2. P0 — Actual microphone state

- [x] Async wpctl query, immediate startup, mute/availability state.
- [x] Timeout, invalid-output handling, no overlapping queries.
- [x] Unavailable-state recovery and observer-only behavior.
- [x] Physical default-microphone change, disable/enable and disconnect/reconnect
      (confirmed by the user on 2026-09-27).
- [ ] Manual KDE Caps/F13 shortcut acceptance.

## 3. P1 — Appearance and configuration

- [x] INI settings, free placement, font, text, icons and visibility.
- [x] Validated green unmuted/red muted background colors.
- [x] Monitor selection, invalid-index fallback and topology recovery.
- [ ] Physical hotplug and mixed-DPI acceptance checks.

## 4. P1 — Event monitoring

- [x] Incremental pw-dump JSON stream and relevant-source filtering.
- [x] Event batching, no self-triggered query loops, idle polling disabled.
- [x] 500 ms fallback, monitor timeout/reconnect and WirePlumber recovery.
- [x] Isolated real-audio integration tests and graceful process cleanup.

## 5. P1 — Tray application (requested 0.2 change)

- [x] Configure, About and Exit tray actions; no background service requirement.
- [x] Ordinary settings window alongside the layer-shell overlay.
- [x] Single instance per configuration profile.
- [x] Autostart checkbox creates/removes the per-user desktop entry.
- [x] Configure enables dragging; closing it relocks click-through input.
- [x] Dragged position is saved on mouse release.
- [x] Free placement on the selected monitor and Reset position to center.
- [x] Scalable microphone application icon for the desktop launcher.
- [x] Icon-only mode and opacity slider with live previews.
- [x] Validate tray registration and automated editing/relocking on real KWin.
- [x] Confirm physical dragging manually (confirmed by the user).
- [x] Confirm locked game input manually (multiple games, user confirmed 2026-09-27).
- [x] Confirm next-login autostart (user confirmed 2026-09-27).

## 6. P2 — Installation and packages

- [x] Local/native CMake installation, launcher, icon and configuration example.
- [x] GPL-3.0-only license.
- [x] English project and installed documentation.
- [x] DEB/RPM CPack recipes and target distribution Podman images.
- [x] Arch PKGBUILD, source archive, checksum and .SRCINFO preparation.
- [x] Revalidate 0.2 builds and DEB/RPM installation; Arch build/check passed.
- [x] Build current 0.2.1 DEB/RPM/Arch packages, verify installation and Wayland
      startup; collect release artifacts and SHA-256 checksums.
- [x] Author/maintainer ptrj and contact aur@dbs.sk.
- [x] Public upstream URL: https://github.com/ptrj/mic-overlay.
- [ ] Before publishing: upload and verify the versioned release archive and
      complete clean Arch chroot validation; aarch64 needs separate verification.

## 7. P2 — Acceptance

- [x] Historical 0.1 clean build, GUI/protocol checks and idle measurements.
- [x] Repeat backend/UI, real KWin editing, single-instance and idle checks.
- [ ] Complete docs/acceptance.md: fullscreen browser, native app and Steam/Proton game.
- [x] Verify game input and focus in locked mode (multiple games, user confirmed).
- [ ] Verify tray configuration, dragging, persistence, icon-only and opacity.
- [x] Verify new-login autostart (user confirmed).
- [ ] Record gaming session duration (at least 30 minutes).
- [ ] Record actual games/results and complete Definition of Done.

## 8. P3 — Optional extensions

- [ ] Native PipeWire API, all-monitor mode, external config live reload.
- [ ] Optional transitions, theme integration, D-Bus status or explicit device selection.

The overlay is moved while Configure is open and remains locked during normal
use. V1 is not complete until manual fullscreen/input and startup acceptance
checks have passed.
