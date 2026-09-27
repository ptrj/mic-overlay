# Configuration

Open **Configure** from the system tray menu. The overlay enters editing mode
while this window is open: drag it with the left mouse button. The indicator
follows the cursor continuously, without multiplying the movement. Its position is
saved when the mouse is released. Closing, cancelling or accepting Configure
locks the overlay and restores click-through input behavior.

The Overlay tab controls the monitor, Reset position, visibility, icon-only
mode and desktop autostart. The Appearance tab controls text, icons, font size,
bold text, background colors and an opacity slider. Opacity and icon-only mode
have live previews. Apply/OK saves settings; Cancel restores the last saved
appearance. Dragged positions are saved independently and remain after Cancel.
The opacity slider changes the background from 0% to 100%; text and icons remain
opaque. In icon-only mode the overlay shrinks to fit the icon.

Settings are stored in `~/.config/mic-overlay/config.ini` (`XDG_CONFIG_HOME` is
respected). An explicit `--config /path/config.ini` selects another profile.
Each profile can run one instance. Missing default files use built-in defaults;
a missing explicitly specified file is an error. The tray remains available even
when the overlay is hidden or the microphone is unavailable.

All settings belong to `[overlay]`:

| Key | Default | Supported values |
|---|---|---|
| x, y | -1 | -1 centers the axis; otherwise nonnegative logical pixels from the monitor’s top-left |
| screen | primary | primary or a current Qt monitor index |
| showWhenUnmuted | true | true/false or 1/0 |
| hideWhenUnavailable | true | true/false or 1/0 |
| iconOnly | false | true/false or 1/0 |
| opacity | 0.85 | background opacity, 0.0–1.0 |
| fontSize | 15 | integer 8–72, logical pixels |
| bold | true | true/false or 1/0 |
| mutedText | MIC MUTED | nonempty single-line text |
| unmutedText | MIC ON | nonempty single-line text |
| mutedColor | #7b202b | #RRGGBB |
| unmutedColor | #166534 | #RRGGBB |
| mutedIcon | mic-muted | mic-muted or mic-on |
| unmutedIcon | mic-on | mic-muted or mic-on |

Drag freely anywhere on the selected monitor. **Reset position** centers the
overlay and saves that position immediately. Placement is bounded to keep the
overlay visible; there is no grid or corner snapping. Coordinates above 4096 are
supported on large monitors. Older corner/margin settings are replaced with
centered placement; appearance settings are preserved. Dragging stays on the
selected monitor; choose another monitor in Configure to move there. Numeric
monitor indices refer to Qt's current ordering and can change between sessions
or after hotplug. An unavailable index falls back to the primary monitor.

The unavailable state has a neutral background and shows MIC N/A when enabled.
Editing mode shows the overlay regardless of normal visibility settings so it
can always be positioned. It accepts mouse input only during editing and never
accepts keyboard input.

The full sample is `config/config.ini.example`. External INI changes take effect
on the next application launch; GUI Apply changes the running overlay directly.
Invalid values produce a warning and fall back to defaults. An invalid or
unreadable file uses defaults. Quote text containing commas for QSettings, e.g.
`mutedText="MIC, MUTED"`.

The project-local `config.local.ini` is used only when passed with `--config`.
Autostart is a desktop entry, not an INI key; see [installation](installation.md).
