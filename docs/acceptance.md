# Acceptance checks

Do not mark manual checks complete before running them on the target PC.
[Verification results](verification.md) distinguish historical results from
current tray/editing-mode validation.

## Automated checks

```bash
bash scripts/container-build.sh
podman exec mic-overlay-dev bash /source/tests/gui_config_smoke.sh
podman exec mic-overlay-dev /build/overlay-controller-test
podman exec -e MIC_OVERLAY_BINARY=/build/mic-overlay mic-overlay-dev python /source/tests/idle_smoke.py
```

For a clean build, configure a new empty build directory. The idle test uses
stable fake audio on real KWin and samples 15 seconds after startup. It requires
zero additional queries, buffer attachments and commits, checks state changes,
and verifies locked overlay input settings. CPU resolution is limited by clock
ticks. Results are in `build-container/acceptance-idle/`. This test uses
`--no-tray`; it does not validate desktop integration.

## Manual checks

| Check | Expected result | Status |
|---|---|---|
| Tray menu | Configure, About and Exit work | pending |
| Configure | Normal interactive window; overlay becomes draggable | pending |
| Close Configure | Overlay is locked and clicks pass through | pending |
| Drag/relaunch | Saved position survives restart | pending |
| Icon-only mode | Text disappears and overlay shrinks | pending |
| Opacity slider | Background preview changes smoothly; Apply persists it | pending |
| Autostart checkbox | Desktop entry is created/removed | pending |
| New KDE login | Enabled autostart launches one tray instance | passed — user, 2026-09-27 |
| Normal/maximized windows | Overlay remains visible, without decorations | pending |
| Alt+Tab/taskbar | Only the settings window appears, not the overlay | passed — user, 2026-09-27 |
| Fullscreen browser/native application | Overlay remains above fullscreen content | pending |
| Fullscreen games | Overlay remains visible and locked input reaches the game | passed — user, multiple games, 2026-09-27 |
| Steam/Proton identification | Record whether tested games used Proton | not specified |
| Game input under overlay | No input interception or focus stealing in locked mode | passed — user, multiple games, 2026-09-27 |
| Caps/F13 KDE mute shortcut | Text/color follow the actual microphone state | pending |
| Default microphone change | New source is followed without restart | passed — user, 2026-09-27 |
| Microphone disable/enable and disconnect/reconnect | Status recovers and follows the active source | passed — user, 2026-09-27 |
| Physical monitor hotplug/mixed DPI | One overlay remains on the correct output | pending |
| At least 30 minutes of gaming | No focus stealing, leaks or excessive idle work | pending |

The user confirmed correct dragging on 2026-09-26 and game, login/autostart and
physical microphone tests on 2026-09-27. Game titles, native/Proton runtime and
session duration were not specified. The user also confirmed fullscreen/game
input and the overlay's absence from Alt+Tab/taskbar on 2026-09-27. Monitor
hotplug and mixed DPI remain open.

Record date, application/game, fullscreen mode, monitors and observations.
Isolated audio tests already cover virtual mute/default changes and restarts
without interrupting host audio. An input protocol trace does not replace
physical click-through testing in a game. Configuration mode intentionally
accepts mouse input; close it before gaming acceptance tests.
