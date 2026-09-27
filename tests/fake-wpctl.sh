#!/bin/sh
[ "$#" = 2 ] && [ "$1" = get-volume ] && [ "$2" = '@DEFAULT_AUDIO_SOURCE@' ] || exit 99
printf 'query\n' >> "$(dirname "$0")/queries"
case "$(cat "$(dirname "$0")/state")" in
    muted) printf 'Volume: 0.27 [MUTED]\n' ;;
    unmuted) printf 'Volume: 0.27\n' ;;
    error) printf 'Volume: 0.27\n'; exit 1 ;;
    empty) exit 0 ;;
    malformed) printf 'diagnostic [MUTED]\n' ;;
    slow) touch "$(dirname "$0")/slow-started"; exec sleep 5 ;;
    delayed) touch "$(dirname "$0")/delayed-started"; sleep 1.5; printf 'Volume: 0.27\n' ;;
esac
