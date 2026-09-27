#!/bin/sh
[ "$#" = 2 ] && [ "$1" = --monitor ] && [ "$2" = --no-colors ] || exit 99
fixture_dir=$(dirname "$0")
printf 'start\n' >> "$fixture_dir/monitor-starts"
printf '%s' "$$" > "$fixture_dir/monitor-pid"
case "$(cat "$fixture_dir/monitor-mode")" in
    fail) exit 1 ;;
    invalid) printf 'not JSON\n'; exec sleep 30 ;;
    silent) exec sleep 30 ;;
    valid) exec tail -n +1 -f "$fixture_dir/events" ;;
esac
exit 1
