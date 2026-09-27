# Verification record

## 2026-09-26: observer and overlay foundation (0.1)

Target: CachyOS/Arch, KDE KWin Wayland 6.7.5, Qt 6.11.2, LayerShellQt 6.7.5.
Outputs: primary DP-1 (2560 × 1440), HDMI-A-1 (1200 × 1920), both scale 1.
Native demo was confirmed working by the user. That confirmation did not include
separate game input, fullscreen or hotplug acceptance checks.

Validated behavior:

- LayerOverlay=3, exclusive zone -1, keyboard interactivity 0 and empty input
  regions in Wayland traces; no ordinary xdg application surface for the overlay.
- SVG resources, INI validation, four corners, hidden/unavailable states, long
  text, 150% scaling, two explicit outputs and invalid-index fallback.
- Controller tests: primary changes, simulated removal/return/no outputs and
  preservation of visibility bindings. Physical display settings were unchanged.
- Asynchronous wpctl queries: both startup states, errors, malformed/empty output,
  timeout, recovery, no overlapping processes and signals only on state changes.
- Incremental JSON framing, source/default metadata tracking, unrelated-client
  filtering, batching, monitor timeout/failure/reconnect and polling fallback.
- Isolated real PipeWire/WirePlumber/D-Bus tests: virtual mic mute/unmute,
  default-source changes, removal and recovery after individual/full-stack restarts.
- SIGTERM/SIGINT exit cleanly and reap owned query/monitor processes.

The 0.1 backend/config/parser CTest suites passed (34 QtTest results including
initialization/cleanup). The clean Release build used GCC 16.2.1. Final controller
tests passed 6/6; all 12 GUI configuration scenarios passed. Logs are in
`build-container/acceptance-clean.log` and `build-container/acceptance-config/`.

DEB (Ubuntu 26.04), RPM (Fedora 44) and Arch x86_64 packages were built and
installed in temporary containers. Real KWin startup, PNG capture, license,
configuration, platform rejection and SVG decoding checks passed. Tests exposed
Ubuntu's separate layer-shell/SVG plugin packages; explicit dependencies were added.
A source archive, SHA-256-checked PKGBUILD and .SRCINFO were generated. No release
was published. Public upstream hosting, contact and clean-chroot verification
remain outstanding. These package results are for 0.1 and need repeating for 0.2.

## 2026-09-26: historical idle measurements (0.1)

Stable GUI test, real KWin, software rendering, fake audio: after initialization,
15 seconds produced zero queries, zero buffer attachments and zero surface
commits. CPU delta was zero clock ticks; RSS was 55,484 KiB. Two startup queries
were expected. Mute/unmute and unavailable/restore transitions preserved locked
input behavior.

The installed observer with real audio was also sampled for 15 seconds: overlay
RSS 95,504 KiB/PSS 22,931 KiB, pw-dump RSS 10,868 KiB/PSS 2,552 KiB. Combined PSS
was about 24.9 MiB. Both had zero measured CPU ticks at SC_CLK_TCK=100 and no mic
state changes. RSS includes shared libraries and should not be treated as private
memory. Zero measured ticks means below short-sample resolution, not absolute
zero CPU consumption. These measurements predate the tray version.

## 0.2: tray application and configuration editing

Requested changes: native tray menu, Configure/About/Exit, per-user desktop
autostart, draggable overlay while Configure is open, automatic relocking,
icon-only mode, live opacity slider and English documentation. The old service
implementation and its tests were removed; autostart uses a desktop entry.

Validation results for this change are recorded below as they are completed.
Manual game/fullscreen input, physical hotplug, mixed DPI, prolonged gaming and
new-login autostart remain pending. V1 acceptance is not complete.

Completed 0.2 checks:

- Arch, Ubuntu and Fedora builds passed all four CTest suites, including the
  desktop UI/autostart tests. Arch makepkg check() and package creation passed.
- Real KWin GUI driver passed: ordinary xdg-shell Configure window, temporary
  mouse input, synthetic drag/persist, live opacity, compact icon-only mode,
  Apply, relocking and the final empty overlay input region.
- Second launch successfully contacted the existing instance and returned.
- Native KDE StatusNotifier registration was confirmed for mic-overlay; its
  exported menu contains Configure, About and Exit.
- The old user unit was disabled and removed. A validated desktop autostart entry
  points to the installed executable and existing configuration. Next-login
  startup still requires a real relogin check.
- DEB/RPM 0.2 installed and launched successfully in temporary containers; their
  desktop launchers, SVG rendering and captures passed. Tray registration is
  tested natively because these package test containers have no session D-Bus.
- Updated locked-overlay idle test (--no-tray): 15 s, zero queries, attachments
  and commits, zero measured CPU ticks, RSS 58,628 KiB. This is not a complete
  process-memory measurement of the tray application.
- Project and installed documentation were rewritten in English. No unit file,
  runtime supervisor dependency or unit-install rule remains. The installation
  guide retains a one-time migration note for older versions.

Physical dragging and gaming input are not proven by synthetic Qt mouse events.
Manual checks in acceptance.md remain open; no complete v1 acceptance is claimed.

## 2026-09-26: free placement and application icon

Configure now offers free dragging and Reset position instead of a corner selector
and four margin fields. Coordinates are relative to the selected monitor, bounded
so the overlay stays visible. Reset saves centered placement; centering follows
changes to overlay size. Existing appearance settings survive the change.
The desktop launcher uses a new scalable microphone icon with a green indicator.

- Release build and all four CTest suites passed in the persistent Podman container.
- Controller tests passed 7/7, including free coordinates, boundary reversal,
  centering after resize and simulated monitor topology changes.
- Real KWin GUI driver verified reset against output geometry, dragging and
  persistence, removed corner/margin widgets, appearance previews, single-instance
  behavior and relocking with an empty input region.
- Wayland configuration smoke tests passed arbitrary positions, visibility,
  invalid-value fallback, 150% scaling, both monitors and unavailable-index fallback.
- SVG rendering was inspected at 256 px. Physical dragging remains a manual check.

### Drag motion correction

The initial drag implementation repeatedly added the full displacement from the
press point to the previous position. Layer-shell margin changes also waited for
a surface commit, so users could see exaggerated movement only after release.

During Configure, the layer surface now stays fixed at the selected monitor's
size. The indicator moves inside it, using incremental pointer coordinates in
that fixed surface. Its input mask follows the indicator, leaving the rest of
the transparent surface click-through. Closing Configure restores the compact,
locked layer surface.

The real KWin GUI driver passed at 100% and 150% scaling. It verifies five
successive cursor movements, exact final displacement of 10 × 15 logical pixels,
no movement from repeated coordinates, frame rendering before release, the
indicator-only input mask, exact saved coordinates and restored compact
click-through behavior. Controller tests passed 7/7; the four CTest suites passed.
These synthetic events do not replace physical mouse acceptance testing.

### Code cleanup after drag acceptance

The user confirmed that native dragging now behaves correctly. Window state,
configuration previews, editing transitions and drag/reset handling are grouped
in OverlayController. main.cpp connects these operations directly; DesktopUi no
longer relays pointer movement. Indicator sizing and screen filtering have shared
helpers. Obsolete corner validation and unused includes were removed, and checkbox
construction was simplified. Legacy INI keys are still removed when saving.

The Release build and four CTest suites passed. After the final UI cleanup,
desktop-ui was rerun successfully; controller tests passed 7/7 and the real KWin
GUI regression passed at 100% and 150% scaling. The regression also rejects QML
geometry binding loops. Physical locked-input acceptance remains pending.

## 2026-09-27: user acceptance and 0.2.1 release preparation

The user reported success in multiple games, working login autostart, and correct
behavior while changing, disabling/enabling and disconnecting/reconnecting
microphones. Titles, native/Proton runtime and gaming duration were not supplied.
This does not establish physical monitor hotplug or mixed-DPI acceptance.

Author: ptrj. Maintainer contact: aur@dbs.sk. Package metadata, the Arch recipe
and About dialog include these details. Version 0.2.1 packages contain the latest
free placement, drag correction, cleanup and comma-text configuration handling.
Application version is derived from CMake to keep binary/package versions aligned.
Repository/release hosting and clean Arch chroot validation remain outstanding.

Completed 0.2.1 packaging validation:

- DEB (Ubuntu 26.04), RPM (Fedora 44) and Arch builds passed all four CTest suites.
- Temporary-container installation and real KWin startup/capture passed for all
  three formats. Containers have no session D-Bus; tray registration remains a
  native desktop check, previously validated. Host packages were not changed.
- DEB Maintainer and RPM Packager both contain ptrj <aur@dbs.sk>; RPM Vendor is ptrj.
  The Arch PKGBUILD includes the maintainer comment and .SRCINFO has a real checksum.
- Release files and SHA256SUMS are collected in build-container/release-0.2.1/.
  No upload or AUR publication was performed.

## 2026-09-27: muted-startup investigation (0.2.2)

The user reported that a microphone already muted at login could leave the
indicator stuck, while launching with an unmuted microphone worked normally.
A reproducible gap was found in event filtering: only capture Node/default
metadata changes caused a wpctl query. Changes to a capture source's backing
Device (including hardware Route parameters) were ignored.

A deterministic monitor regression starts muted and sends only backing-device
Route updates. It failed before the fix and passes with device association
tracking. Parser tests cover numeric/string device IDs, unrelated devices,
source removal, device changes and cache reset. The observer still obtains the
actual state exclusively from wpctl; event payloads only trigger a query.

All four CTest suites passed. Isolated real PipeWire/WirePlumber integration
passed with MIC_OVERLAY_INITIAL_MUTED=1, including the first unmute, subsequent
mute/unmute, default changes, removals and service restarts. A real-KWin test with
fake hardware events passed mute/unmute/availability transitions and locked input.
During 15 seconds of stable idle it recorded zero queries, buffer attachments
and surface commits (zero measured CPU ticks, RSS 53,744 KiB).

The original login sequence on physical hardware still needs user confirmation;
virtual-source startup alone also worked before this fix. This release addresses
the reproduced missed-device-event case without claiming the reported login
failure has been reproduced on the host. Existing 0.2.1 packages predate this fix.

### Physical fullscreen/game input and window-list acceptance (2026-09-27)

The user confirmed that the overlay stays visible over fullscreen games, locked
input reaches the game without focus stealing, and the overlay does not appear
in Alt+Tab or the taskbar. The P0 acceptance item is complete based on this
manual confirmation. Game titles, runtime and session duration remain unspecified.
