#!/usr/bin/env bash
# Run inside the development container. Uses fake audio state, real host KWin.
set -euo pipefail
fixture_dir=$(mktemp -d /tmp/mic-overlay-config.XXXXXX)
log_dir=${MIC_OVERLAY_LOG_DIR:-/build/gui-config-smoke}
binary=${MIC_OVERLAY_BINARY:-/build/mic-overlay}
mkdir -p "$log_dir"
cp /source/tests/fake-wpctl.sh "$fixture_dir/wpctl"
chmod 700 "$fixture_dir/wpctl"

run_case() {
    local name=$1 state=$2 config=$3 scale=${4:-1}
    printf '%s' "$state" > "$fixture_dir/state"
    printf '%s\n' "$config" > "$fixture_dir/config.ini"
    PATH="$fixture_dir:$PATH" QT_FORCE_STDERR_LOGGING=1 WAYLAND_DEBUG=1 \
        QT_SCALE_FACTOR="$scale" "$binary" \
        --config "$fixture_dir/config.ini" --quit-after 1 > "$log_dir/$name.log" 2>&1
    log=$(<"$log_dir/$name.log")
    if [[ $log == *'qrc:/qml/'* || $log == *'Cannot load'* || $log == *'ReferenceError'* ]]; then
        echo "QML error in $name; see $log_dir/$name.log" >&2
        exit 1
    fi
}

require() {
    if [[ $log != *"$1"* ]]; then
        echo "Missing '$1'; see $log_dir" >&2
        exit 1
    fi
}
require_hidden() {
    if [[ $log == *'.get_layer_surface('* ]]; then
        echo "Unexpected visible layer surface; see $log_dir" >&2
        exit 1
    fi
}

for coordinates in "11 44" "317 529" "800 1000"; do
    read -r x y <<< "$coordinates"
    config=$(printf '[overlay]\nx=%s\ny=%s\n' "$x" "$y")
    run_case "position-$x-$y" muted "$config"
    require '.set_anchor(5)'
    require ".set_margin($y, 0, 0, $x)"
    require '.set_keyboard_interactivity(0)'
    require '.set_input_region('
    echo "PASS: free position $x,$y"
done

run_case unmuted-hidden unmuted $'[overlay]\nshowWhenUnmuted=false'
require 'state changed: unmuted'
require_hidden
echo 'PASS: unmuted hidden'

run_case unavailable-hidden error $'[overlay]\nhideWhenUnavailable=true'
require 'wpctl query failed'
require_hidden
echo 'PASS: unavailable hidden'

run_case unavailable-visible error $'[overlay]\nhideWhenUnavailable=false'
require 'wpctl query failed'
require '.get_layer_surface('
echo 'PASS: unavailable visible'

run_case invalid-config muted $'[overlay]\nx=-2\nfontSize=-1\nopacity=nan\nmutedIcon=invalid'
require 'invalid overlay/x'
require '.set_anchor(5)'
# Centering is checked against the actual output geometry by tray_gui_smoke.
echo 'PASS: invalid configuration fallback'

run_case long-text-scaled muted $'[overlay]\nfontSize=22\nbold=false\nopacity=0.4\nmutedText=MIKROFON JE MOMENTALNE STLMENY\nmutedIcon=mic-on' 1.5
require '.get_layer_surface('
require '.attach(wl_buffer'
echo 'PASS: long text, custom style and 150% scale'

# Verify the output passed to layer-shell, not only the controller's chosen QScreen.
mapfile -t screens < <("$binary" --list-screens)
primary_name=
for screen in "${screens[@]}"; do
    index=${screen%%:*}
    name=${screen#*: }
    name=${name%% (*}
    if [[ $screen == *'[primary]'* ]]; then primary_name=$name; fi
    config=$(printf '[overlay]\nscreen=%s\n' "$index")
    run_case "screen-$index" muted "$config"
    require "mic-overlay: screen: $name"
    output_id=
    while IFS= read -r line; do
        if [[ $line == *"wl_output#"*".name(\"$name\")"* ]]; then
            part=${line#*wl_output#}
            output_id=${part%%.*}
        fi
    done < "$log_dir/screen-$index.log"
    [[ -n $output_id ]] || { echo "Missing Wayland output name $name" >&2; exit 1; }
    require "wl_output#$output_id, 3, \"mic-overlay\")"
    echo "PASS: screen=$index ($name)"
done

run_case screen-fallback muted $'[overlay]\nscreen=99999'
require 'selected screen index unavailable; using primary'
require "mic-overlay: screen: $primary_name"
echo 'PASS: missing monitor index fallback'
echo "GUI configuration smoke tests passed; logs: $log_dir"
