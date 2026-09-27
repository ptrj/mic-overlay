#!/usr/bin/env bash
# Optional real-service test inside the development container, entirely isolated
# from /host-runtime and the user's audio/session configuration.
set -euo pipefail
if [[ ${MIC_OVERLAY_TEST_DBUS:-0} != 1 ]]; then
    exec dbus-run-session -- env MIC_OVERLAY_TEST_DBUS=1 bash "$0"
fi
test_runtime=$(mktemp -d /tmp/mic-overlay-pipewire.XXXXXX)
export XDG_RUNTIME_DIR="$test_runtime"
export PIPEWIRE_RUNTIME_DIR="$test_runtime"
export XDG_CONFIG_HOME="$test_runtime/config"
export XDG_STATE_HOME="$test_runtime/state"
export XDG_DATA_HOME="$test_runtime/data"
export QT_FORCE_STDERR_LOGGING=1
mkdir -p "$XDG_CONFIG_HOME" "$XDG_STATE_HOME" "$XDG_DATA_HOME" /build/pipewire-integration
log_dir=/build/pipewire-integration
probe_log="$log_dir/probe.log"
query_log="$test_runtime/queries"
touch "$query_log" "$probe_log"
: > "$probe_log"
pipewire_pid=
wireplumber_pid=
probe_pid=

cleanup() {
    for pid in "${probe_pid:-}" "${wireplumber_pid:-}" "${pipewire_pid:-}"; do
        if [[ -n $pid ]]; then kill "$pid" 2>/dev/null || true; fi
    done
    for pid in "${probe_pid:-}" "${wireplumber_pid:-}" "${pipewire_pid:-}"; do
        if [[ -n $pid ]]; then wait "$pid" 2>/dev/null || true; fi
    done
}
trap cleanup EXIT

start_services() {
    pipewire >> "$log_dir/pipewire.log" 2>&1 & pipewire_pid=$!
    for ((i=0; i<50; ++i)); do
        [[ -S "$test_runtime/pipewire-0" ]] && break
        sleep 0.1
    done
    [[ -S "$test_runtime/pipewire-0" ]]
    wireplumber >> "$log_dir/wireplumber.log" 2>&1 & wireplumber_pid=$!
}
stop_wireplumber() {
    kill "$wireplumber_pid"
    wait "$wireplumber_pid" || true
    wireplumber_pid=
}
stop_services() {
    stop_wireplumber
    kill "$pipewire_pid"
    wait "$pipewire_pid" || true
    pipewire_pid=
}
create_source() {
    pw-cli create-node adapter "{ factory.name = support.null-audio-sink node.name = $1 media.class = Audio/Source/Virtual audio.position = [ MONO ] object.linger = true }"
}
node_id() {
    pw-dump --no-colors | python -c 'import json,sys; print(next(o["id"] for o in json.load(sys.stdin) if o.get("info",{}).get("props",{}).get("node.name")==sys.argv[1]))' "$1"
}
wait_default() {
    for ((i=0; i<100; ++i)); do
        if /usr/bin/wpctl get-volume @DEFAULT_AUDIO_SOURCE@ > /dev/null 2>&1; then return; fi
        sleep 0.1
    done
    echo 'Default source did not become available' >&2
    exit 1
}
mark() { wc -c < "$probe_log"; }
wait_after() {
    local pattern=$1 offset=${2:-0}
    for ((i=0; i<100; ++i)); do
        local suffix
        suffix=$(tail -c "+$((offset+1))" "$probe_log")
        if [[ $suffix == *"$pattern"* ]]; then return; fi
        sleep 0.1
    done
    echo "Missing '$pattern' after byte $offset; see $probe_log" >&2
    cat "$probe_log" >&2
    exit 1
}

# Count observer queries, while control commands keep using the real wpctl.
mkdir "$test_runtime/bin"
cat > "$test_runtime/bin/wpctl" <<'SCRIPT'
#!/bin/sh
printf 'query\n' >> "$MIC_OVERLAY_QUERY_LOG"
exec /usr/bin/wpctl "$@"
SCRIPT
chmod 700 "$test_runtime/bin/wpctl"
export MIC_OVERLAY_QUERY_LOG="$query_log"
start_services
create_source test-mic-a
wait_default
source_a=$(node_id test-mic-a)
/usr/bin/wpctl set-default "$source_a"
initial_muted=${MIC_OVERLAY_INITIAL_MUTED:-0}
[[ "$initial_muted" == 0 || "$initial_muted" == 1 ]]
/usr/bin/wpctl set-mute "$source_a" "$initial_muted"
PATH="$test_runtime/bin:$PATH" /build/mic-state-probe > "$probe_log" 2>&1 & probe_pid=$!
wait_after 'PipeWire event monitoring active'
if [[ "$initial_muted" == 1 ]]; then
    wait_after 'state changed: muted'
else
    wait_after 'state changed: unmuted'
fi
sleep 0.8
idle_queries=$(wc -l < "$query_log")
sleep 2
[[ $(wc -l < "$query_log") == "$idle_queries" ]]
echo "PASS: idle has no repeated wpctl queries ($idle_queries startup queries)"

if [[ "$initial_muted" == 1 ]]; then
    offset=$(mark)
    /usr/bin/wpctl set-mute "$source_a" 0
    wait_after 'state changed: unmuted' "$offset"
    echo 'PASS: muted startup follows the first unmute'
fi

offset=$(mark)
/usr/bin/wpctl set-mute "$source_a" 1
wait_after 'state changed: muted' "$offset"
offset=$(mark)
/usr/bin/wpctl set-mute "$source_a" 0
wait_after 'state changed: unmuted' "$offset"
echo 'PASS: real mute events'

create_source test-mic-b
source_b=$(node_id test-mic-b)
/usr/bin/wpctl set-mute "$source_b" 1
offset=$(mark)
/usr/bin/wpctl set-default "$source_b"
wait_after 'state changed: muted' "$offset"
offset=$(mark)
pw-cli destroy "$source_b"
wait_after 'state changed: unmuted' "$offset"
echo 'PASS: default microphone switch and source removal'

offset=$(mark)
stop_wireplumber
wait_after 'default microphone unavailable' "$offset"
offset=$(mark)
wireplumber >> "$log_dir/wireplumber.log" 2>&1 & wireplumber_pid=$!
wait_after 'default microphone available' "$offset"
echo 'PASS: WirePlumber restart with PipeWire still running'

offset=$(mark)
stop_services
wait_after 'default microphone unavailable' "$offset"
offset=$(mark)
start_services
create_source test-mic-a
wait_default
wait_after 'PipeWire event monitoring active' "$offset"
wait_after 'default microphone available' "$offset"
echo 'PASS: PipeWire/WirePlumber restart and monitor reconnect'
echo "Isolated PipeWire integration passed; logs: $log_dir"
