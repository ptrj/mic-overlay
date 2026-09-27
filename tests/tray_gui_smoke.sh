#!/usr/bin/env bash
# Run in the development container; all configuration writes are private.
set -euo pipefail
fixture=$(mktemp -d /tmp/mic-overlay-tray.XXXXXX)
trap 'rm -rf -- "$fixture"' EXIT
mkdir -p /build/tray-gui
cp /source/tests/fake-wpctl.sh "$fixture/wpctl"
chmod 700 "$fixture/wpctl"
printf 'unmuted' > "$fixture/state"
PATH="$fixture:$PATH" XDG_CONFIG_HOME="$fixture/config" WAYLAND_DEBUG=1 \
    QT_FORCE_STDERR_LOGGING=1 /build/tray-gui-smoke --configure --quit-after 5 \
    > /build/tray-gui/wayland.log 2>&1
grep -E 'PASS:|FAIL:' /build/tray-gui/wayland.log
python - <<'PY'
from pathlib import Path
import re
trace = Path('/build/tray-gui/wayland.log').read_text()
assert 'Binding loop detected' not in trace, 'Unexpected QML geometry binding loop'
assert '.get_xdg_surface(' in trace, 'Configure must use a normal xdg-shell window'
assert re.search(r'get_layer_surface\([^\n]+, 3, "mic-overlay"\)', trace)
regions = re.findall(r'set_input_region\(wl_region#(\d+)\)', trace)
assert regions, 'Missing input region'
# The last region belongs to the relocked overlay and must be empty.
last = trace.rfind(f'set_input_region(wl_region#{regions[-1]})')
creation = trace.rfind(f'create_region(new id wl_region#{regions[-1]})', 0, last)
assert creation >= 0
assert not re.search(r'wl_region#\d+\.add\(', trace[creation:last])
assert '.set_keyboard_interactivity(0)' in trace
print('PASS: ordinary settings window and relocked empty overlay input region')
PY
