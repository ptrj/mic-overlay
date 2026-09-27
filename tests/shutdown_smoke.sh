#!/usr/bin/env bash
set -euo pipefail
probe_pid=
monitor_pid=
cleanup() {
    if [[ -n ${probe_pid:-} ]]; then kill "$probe_pid" 2>/dev/null || true; wait "$probe_pid" 2>/dev/null || true; fi
    if [[ -n ${monitor_pid:-} ]]; then kill "$monitor_pid" 2>/dev/null || true; fi
}
trap cleanup EXIT
for signal in TERM INT; do
    QT_FORCE_STDERR_LOGGING=1 /build/mic-state-probe > "/build/shutdown-$signal.log" 2>&1 & probe_pid=$!
    for ((i=0; i<30; ++i)); do
        monitor_pid=$(pgrep -P "$probe_pid" -x pw-dump || true)
        [[ -n $monitor_pid ]] && break
        sleep 0.1
    done
    [[ -n $monitor_pid ]]
    kill "-$signal" "$probe_pid"
    wait "$probe_pid"
    probe_pid=
    if kill -0 "$monitor_pid" 2>/dev/null; then
        echo "Orphaned pw-dump after SIG$signal" >&2
        exit 1
    fi
    monitor_pid=
    echo "PASS: SIG$signal exits cleanly and reaps pw-dump"
done
