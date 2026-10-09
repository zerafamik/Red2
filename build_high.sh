#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
cc="${CC:-gcc}"
mode="${RED2_BUILD_MODE:-native}"
case "$mode" in native) arch="-march=native";; portable) arch="";; *) printf 'RED2_BUILD_MODE must be native or portable.\n' >&2; exit 2;; esac
read -r -a flags <<< "${CFLAGS:--O3 $arch -std=c11 -DNDEBUG -Wall -Wextra -Wpedantic -fopenmp}"
suffix=""
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) suffix=".exe";; esac
printf 'Compiler: %s\nFlags: %s\n' "$cc" "${flags[*]}"
"$cc" --version
"$cc" "${flags[@]}" red2_calendar_cli.c red2_calendar.c -lm -o "red2_v34${suffix}"
"$cc" "${flags[@]}" red2_block_cli.c red2_block.c -lm -o "red2_v35${suffix}"
"$cc" "${flags[@]}" -DRED2_CALENDAR_CHECKS=1 validate_calendar_cases.c red2_calendar.c -lm -o "validate_v34${suffix}"
"$cc" "${flags[@]}" -DRED2_CALENDAR_CHECKS=1 validate_block_cases.c red2_block.c -lm -o "validate_v35${suffix}"
"$cc" "${flags[@]}" -DRED2_CALENDAR_CHECKS=1 validate_10t.c red2_block.c -lm -o "validate_range_v35${suffix}"
"$cc" "${flags[@]}" -DRED2_WINDOW_CONTROL screen_window.c -lm -o "window_v34${suffix}"
"$cc" "${flags[@]}" screen_window.c -lm -o "window_v35${suffix}"
