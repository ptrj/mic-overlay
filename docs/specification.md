# Technical Specification: KDE/Qt Wayland Microphone Mute Overlay


> Version 0.2 is a native tray application. Configure enables temporary overlay
> dragging, icon-only display, live opacity preview and desktop autostart.
> Closing Configure restores locked click-through behavior. No background service
> is required. Documentation is in English. License: GPL-3.0-only.
> See [installation](installation.md), [configuration](configuration.md),
> [development](development.md), [packaging](packaging.md),
> [verification](verification.md), [acceptance](acceptance.md) and [TODO](../TODO.md).

Author: **ptrj**. Maintainer contact: **aur@dbs.sk**.

## 1. Project goal

Implement a small standalone application named:

`mic-overlay`

for:

- Arch Linux / CachyOS
- KDE Plasma 6
- KWin Wayland session
- PipeWire + WirePlumber
- Qt 6
- LayerShellQt

The application displays microphone mute state as a click-through overlay and provides a system tray menu with Configure, About and Exit. The overlay accepts mouse dragging only while Configure is open.

The overlay MUST remain visible above normal windows and above fullscreen applications/games running under KDE Plasma Wayland.

This is not a Plasma widget. It is a standalone native Wayland layer-shell client.

Primary use case:

```text
physical Caps Lock
    ↓
udev HWDB remap
    ↓
F13
    ↓
KDE global shortcut
    ↓
Toggle Microphone Mute
    ↓
PipeWire/WirePlumber microphone state
    ↓
mic-overlay updates automatically
```

The overlay must observe the actual microphone state. It must NOT assume that F13 caused the state change.

---

# 2. Required behaviour

Monitor:

```bash
wpctl get-volume @DEFAULT_AUDIO_SOURCE@
```

Typical unmuted output:

```text
Volume: 0.41
```

Typical muted output:

```text
Volume: 0.41 [MUTED]
```

The application must interpret presence of:

```text
[MUTED]
```

as:

```text
muted = true
```

and its absence as:

```text
muted = false
```

`@DEFAULT_AUDIO_SOURCE@` must be resolved on every query instead of caching a PipeWire numeric node ID, so changing the default microphone while the application is running continues to work. WirePlumber documents `@DEFAULT_AUDIO_SOURCE@` as a runtime-resolved identifier for the current default capture source.

---

# 3. UI behaviour

Default display mode:

### Microphone muted

Show overlay:

```text
┌──────────────┐
│  MIC MUTED   │
└──────────────┘
```

Preferably with a microphone-muted SVG icon.

Example:

```text
🎙̸  MIC MUTED
```

Do not depend on emoji fonts for the final implementation. Include SVG icons in the application resources.

### Microphone unmuted

Show:

```text
┌──────────┐
│  MIC ON  │
└──────────┘
```

with microphone SVG icon.

The application must also support configuration:

```ini
showWhenUnmuted=true
```

If:

```ini
showWhenUnmuted=false
```

then the overlay completely disappears when the microphone is unmuted and appears when muted.

Default:

```ini
showWhenUnmuted=true
```

The intention is that the user always has positive confirmation of the microphone state while gaming.

---

# 4. Visual design

Default appearance should be minimal and suitable for gaming.

Default position: centered on the selected monitor. While Configure is open,
drag the overlay freely anywhere on that monitor. Reset position centers it.

Recommended default overlay size:

```text
approximately 150 × 42 px
```

but size should adapt to content.

Muted state:

```text
MIC MUTED
```

should be visually much more prominent than the unmuted state.

Use:

- rounded background
- semi-transparent background
- SVG icon
- text
- approximately 14–16 px text
- bold label
- configurable opacity

Do not use decorations, title bars or normal desktop window chrome.

Do not show the application in the taskbar.

Do not show it in Alt+Tab.

Do not steal focus.

Do not accept keyboard input.

Do not accept mouse input while Configure is closed.

With Configure closed, mouse clicks must pass through the overlay to the fullscreen game underneath.

---

# 5. Wayland / LayerShell requirements

Use KDE LayerShellQt.

Qt >= 6.5 supports selecting shell integration per window. Use LayerShellQt
only on the overlay. Do not globally force layer-shell: Configure and About
must remain ordinary interactive xdg-shell windows.

Obtain the LayerShell window object using:

```cpp
auto *layerWindow = LayerShellQt::Window::get(window);
```

LayerShellQt officially exposes this model and CMake target `LayerShellQt::Interface`.

Configure:

```cpp
layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
```

This is important.

Do NOT use:

```cpp
LayerTop
```

unless `LayerOverlay` is proven incompatible.

The purpose is to make the indicator visible above fullscreen applications.

LayerShellQt defines:

```text
LayerBackground
LayerBottom
LayerTop
LayerOverlay
```

with `LayerOverlay` as the highest layer.

Use top-left layer-shell anchors internally and update margins from the freely
chosen monitor-relative coordinates. These anchors do not restrict user placement.

```cpp
layerWindow->setAnchors(
    LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) |
    LayerShellQt::Window::AnchorLeft
);
layerWindow->setMargins(QMargins(x, y, 0, 0));
```

Set:

```cpp
layerWindow->setKeyboardInteractivity(
    LayerShellQt::Window::KeyboardInteractivityNone
);
```

The overlay MUST NOT reserve usable desktop space.

Use a non-positive/no-reservation exclusive-zone configuration suitable for an overlay rather than a desktop panel.

Verify behaviour experimentally under KWin.

Set a meaningful scope:

```cpp
layerWindow->setScope(QStringLiteral("mic-overlay"));
```

KDE itself uses the same LayerShellQt concepts for Plasma surfaces including anchors, keyboard interactivity and layer assignment.

---

# 6. Click-through requirements

Qt window flags must prevent input interception.

Use the appropriate Qt flags including equivalents of:

```cpp
Qt::FramelessWindowHint
Qt::WindowDoesNotAcceptFocus
Qt::WindowTransparentForInput
```

When Configure is closed, the overlay must never interfere with:

- mouse movement
- mouse clicks
- keyboard input
- game controllers
- window focus

A fullscreen game underneath must behave exactly as if the overlay did not exist.

This is a hard requirement.

---

# 7. Qt architecture

Preferred architecture:

```text
Qt 6
├── C++ bootstrap
│   ├── application startup
│   ├── LayerShellQt configuration
│   ├── microphone state backend
│   └── configuration handling
│
└── QML
    └── visual overlay
```

Suggested classes:

```text
main.cpp
MicStateMonitor
OverlayController
```

### MicStateMonitor

Responsibilities:

- determine current default microphone mute state
- emit signal when mute state changes
- detect PipeWire/WirePlumber errors
- never modify microphone state

Suggested interface:

```cpp
class MicStateMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    bool muted() const;
    bool available() const;

signals:
    void mutedChanged();
    void availableChanged();
};
```

The overlay is strictly a state observer.

It must NOT perform:

```bash
wpctl set-mute
```

The existing KDE shortcut remains responsible for toggling microphone mute.

---

# 8. Microphone state monitoring

## MVP implementation

Initial implementation may use:

```bash
wpctl get-volume @DEFAULT_AUDIO_SOURCE@
```

via `QProcess`.

On application startup, immediately query the microphone.

Do not wait for the first timer/event.

Parsing logic:

```text
stdout contains "[MUTED]"
    → muted = true

stdout does not contain "[MUTED]"
    → muted = false
```

Do not parse the volume value.

Examples:

```text
Volume: 0.41
```

means unmuted.

```text
Volume: 0.41 [MUTED]
```

means muted.

---

# 9. Event driven monitoring

Avoid permanently executing `wpctl` many times per second if possible.

Preferred implementation:

1. Initial state obtained through `wpctl`.
2. Start a persistent audio-event monitor.
3. When microphone/source state changes, execute one `wpctl get-volume` refresh.
4. Update UI only if state actually changed.

Possible implementation options, in priority order:

### Option A — PipeWire native monitoring

Use PipeWire APIs or a persistent PipeWire event stream.

This is the preferred long-term architecture if reasonably simple.

### Option B — `pw-dump --monitor`

`pw-dump --monitor` can remain running and outputs JSON describing PipeWire state changes. PipeWire officially supports the `--monitor` mode.

The application may run:

```bash
pw-dump --monitor
```

as a persistent `QProcess`.

When a relevant PipeWire state change is detected, refresh the state using:

```bash
wpctl get-volume @DEFAULT_AUDIO_SOURCE@
```

Do not attempt to infer mute purely from fragile human-readable `pw-dump` output unless the implementation is robust.

### Option C — fallback polling

If event monitoring cannot be initialized, use a timer fallback.

Recommended interval:

```text
500 ms
```

This is acceptable as fallback behaviour.

Do NOT use busy loops.

Do NOT poll faster than 200 ms.

The event-driven implementation should be preferred because the overlay is expected to run for entire gaming sessions.

---

# 10. Error states

If:

```bash
wpctl get-volume @DEFAULT_AUDIO_SOURCE@
```

fails because there is no microphone/default source, the app must not crash.

Set:

```text
available = false
```

Possible UI:

```text
MIC N/A
```

or hide the overlay.

Make this configurable.

Default:

```ini
hideWhenUnavailable=true
```

If PipeWire temporarily restarts, the application should recover automatically when the service becomes available again.

Do not require restarting `mic-overlay`.

---

# 11. Multi-monitor support

Support at least:

```ini
screen=primary
```

and numeric screen selection:

```ini
screen=0
screen=1
```

Default:

```ini
screen=primary
```

Use Qt `QScreen` / `QGuiApplication::screens()`.

Set the correct `QWindow` screen before or while configuring LayerShellQt.

The overlay should only exist on one selected monitor.

Future support for:

```ini
screen=all
```

is optional and not part of MVP.

---

# 12. Config file

Use:

```text
~/.config/mic-overlay/config.ini
```

Example:

```ini
[overlay]
; -1 centers each axis; otherwise logical pixels from monitor top-left.
x=-1
y=-1

showWhenUnmuted=true
hideWhenUnavailable=true

screen=primary

opacity=0.85

fontSize=15
bold=true

mutedText=MIC MUTED
unmutedText=MIC ON

mutedIcon=mic-muted
unmutedIcon=mic-on
```

Placement is free within the selected monitor, without a grid or corner snapping.
Reset position saves centered placement immediately.

Configure changes apply to the running overlay. Dragging saves position on mouse release. Closing Configure locks it. External INI edits are loaded on the next launch. `iconOnly=false` displays the icon and text; `iconOnly=true` displays only the icon.

---

# 13. Assets

Include the following project-owned SVG assets:

```text
assets/mic-on.svg
assets/mic-muted.svg
```

Do not rely on a specific KDE icon theme being installed.

SVGs should use a neutral foreground that can be colored/tinted by QML if practical.

Avoid emoji as the production icon.

---

# 14. Suggested project layout

```text
mic-overlay/
├── CMakeLists.txt
├── README.md
├── LICENSE
│
├── src/
│   ├── main.cpp
│   ├── micstatemonitor.h
│   ├── micstatemonitor.cpp
│   ├── overlaycontroller.h
│   └── overlaycontroller.cpp
│
├── qml/
│   └── Main.qml
│
├── assets/
│   ├── mic-on.svg
│   └── mic-muted.svg
│
├── packaging/
│   └── mic-overlay.desktop.in
│
└── config/
    └── config.ini.example
```

Keep the codebase intentionally small.

Do not build a generic overlay framework.

---

# 15. CMake

Use CMake.

Expected concepts:

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    Qml
)

find_package(LayerShellQt REQUIRED)
```

Link against:

```cmake
Qt6::Core
Qt6::Gui
Qt6::Quick
Qt6::Qml
LayerShellQt::Interface
```

LayerShellQt officially exports `LayerShellQt::Interface`.

Prefer C++20.

---

# 16. Arch Linux dependencies

Target Arch Linux first.

Build dependencies should be documented.

Expected packages:

```bash
sudo pacman -S \
    base-devel \
    cmake \
    qt6-base \
    qt6-declarative \
    qt6-wayland \
    qt6-svg \
    layer-shell-qt \
    pipewire \
    wireplumber
```

Do not bundle Qt or LayerShellQt.

Use system libraries.

Arch currently packages `layer-shell-qt` in `extra`; it depends on Qt 6 and provides both its interface library and QML module.

---

# 17. Runtime environment

The application is Wayland-only for v1.

On startup check:

```text
XDG_SESSION_TYPE
```

and/or Qt platform.

If not running under Wayland, print a clear error and exit.

Do not implement:

- X11 always-on-top hacks
- XShape
- XInput
- xdotool
- wmctrl

Do not implement an X11 fallback in v1.

Target:

```text
KDE Plasma
KWin
Wayland
```

---

# 18. Desktop integration and autostart

Use Qt Widgets `QSystemTrayIcon` with Configure, About and Exit actions.
Configure is a normal window with appearance, visibility, monitor and startup
settings. It enables mouse dragging on the overlay while open; closing it
restores click-through behavior. Opacity and icon-only mode have live previews.
Apply/OK persists appearance settings; dragged position is saved separately.

The autostart checkbox manages `~/.config/autostart/mic-overlay.desktop`,
respecting XDG_CONFIG_HOME. The entry uses the current executable and config path.
Disabling it removes the entry. Install a separate application launcher and icon.
A second launch opens the existing instance's settings for the same config file.
Do not require or install a background service. Exit ends the process and its
owned monitor normally; it does not restart until launched again or the next login.

---

# 19. Logging

Use Qt logging.

Normal operation should be quiet.

Useful messages:

```text
mic-overlay: started
mic-overlay: default microphone available
mic-overlay: state changed: muted
mic-overlay: state changed: unmuted
```

Warnings:

```text
mic-overlay: wpctl query failed
mic-overlay: no default audio source
mic-overlay: PipeWire event monitor stopped
```

Do not log every timer iteration.

---

# 20. Performance requirements

The application is intended to run while gaming.

Requirements:

- negligible idle CPU usage
- negligible GPU usage
- low memory consumption
- no continuous rendering if nothing changes
- no animations by default
- no busy polling
- no spawning processes continuously if event-driven monitoring works

QML animations should be disabled by default.

Optional short state-transition fade:

```text
100–150 ms
```

is acceptable but not required.

---

# 21. Fullscreen requirements

This requirement is critical.

Test with:

- normal KDE windows
- maximized window
- fullscreen browser
- native Wayland fullscreen application
- Steam/Proton fullscreen game under KWin

Overlay must remain visible above the fullscreen surface.

The implementation must use:

```cpp
LayerShellQt::Window::LayerOverlay
```

rather than relying on:

```text
Qt::WindowStaysOnTopHint
```

alone.

A Qt "always on top" window is not considered sufficient.

A comparable KDE Wayland overlay project, KOverlay, uses Qt6/QML + LayerShellQt specifically to provide click-through, always-on-top behaviour over applications under Plasma/KWin.

---

# 22. Input/focus acceptance tests

While fullscreen game is active:

1. Move mouse through overlay area.
2. Click where overlay is drawn.
3. Press keyboard keys.
4. Move game camera.
5. Interact with UI beneath overlay.

Expected result:

```text
all input reaches the game
```

Overlay must never:

```text
gain focus
activate
consume click
consume keyboard input
appear in Alt+Tab
```

---

# 23. Functional acceptance tests

## Test 1 — startup muted

Before starting application:

```bash
wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1
```

Start overlay.

Expected:

```text
MIC MUTED
```

immediately.

---

## Test 2 — startup unmuted

```bash
wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 0
```

Start overlay.

Expected:

```text
MIC ON
```

immediately.

---

## Test 3 — external mute change

While overlay runs:

```bash
wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle
```

Expected:

overlay updates without restart.

---

## Test 4 — KDE shortcut

Use existing:

```text
Caps → F13 → KDE Toggle Microphone Mute
```

Expected:

overlay reflects actual resulting microphone state.

The overlay itself must not listen for F13.

---

## Test 5 — default microphone changes

Connect/select another microphone as the default source.

Toggle mute.

Expected:

overlay follows the new:

```text
@DEFAULT_AUDIO_SOURCE@
```

without application restart.

---

## Test 6 — PipeWire restart

Restart user PipeWire/WirePlumber services.

Expected:

- overlay does not crash
- unavailable state is handled
- monitoring reconnects
- state resumes when PipeWire returns

---

## Test 7 — fullscreen game

Launch a fullscreen Steam/Proton game.

Expected:

```text
overlay remains visible
```

and:

```text
overlay is completely click-through
```

---

# 24. Definition of Done

Version 1 is complete when all of the following are true:

- builds cleanly on current Arch Linux
- runs natively under Wayland
- uses Qt6
- uses LayerShellQt
- uses `LayerOverlay`
- displays microphone mute state
- detects changes without restarting
- tracks `@DEFAULT_AUDIO_SOURCE@`
- remains visible above fullscreen KWin applications
- accepts no keyboard input
- accepts no mouse input when Configure is closed
- does not steal focus
- supports free placement and resetting to the monitor center
- supports selecting monitor
- provides a system tray menu and optional desktop autostart
- handles missing/restarting PipeWire gracefully
- has no dependency on X11
- codebase remains minimal

---

# 25. Non-goals for version 1

Do not implement:

- microphone volume controls
- microphone selection GUI
- KDE System Settings module
- Plasma widget
- hotkey registration
- Push-to-Talk
- mute toggling
- audio level visualization
- voice activity detection
- recording
- X11 support
- Windows/macOS support

The application has exactly one responsibility:

> Display the actual mute state of the current default PipeWire microphone as a fullscreen-safe KDE Wayland overlay.

---

# 26. Optional version 2 features

Only after v1 works reliably:

```text
fade transitions
custom colors
multiple monitors
Plasma theme integration
KConfig integration
D-Bus status interface
native libpipewire monitoring
per-device microphone selection
audio input activity indicator
```

Do not implement these until the MVP and fullscreen behaviour have been verified.

---

# 27. Important implementation principle

Treat PipeWire/WirePlumber as the source of truth.

Do NOT derive microphone state from:

- whether F13 was pressed
- current icon state
- internal toggle boolean
- KDE shortcut state

Correct flow:

```text
KDE changes microphone
        ↓
PipeWire/WirePlumber contains actual state
        ↓
mic-overlay reads actual state
        ↓
UI renders that state
```

This prevents desynchronization if mute is changed from KDE, another application, `wpctl`, the physical microphone, or another audio control.
