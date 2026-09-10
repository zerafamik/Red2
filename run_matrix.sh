#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
export LC_ALL=C
cores="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf 1)"
half=$((cores/2)); ((half>=1)) || half=1
read -r -a requested <<< "${RED2_MATRIX_THREADS:-1 $half $cores}"
read -r -a cases <<< "${RED2_MATRIX_CASES:-1000000000:4 10000000000:16 100000000000:64 100000000000:512}"
teams=();declare -A seen=()
for t in "${requested[@]}"; do
    [[ "$t" =~ ^[1-9][0-9]*$ && ${#t} -le 4 ]] && ((t<=1024)) || exit 2
    if [[ ! ${seen[$t]+present} ]]; then teams+=("$t");seen[$t]=1;fi
done
for item in "${cases[@]}"; do
    [[ "$item" =~ ^([1-9][0-9]*):([1-9][0-9]*)$ ]] || exit 2
    n="${BASH_REMATCH[1]}";k="${BASH_REMATCH[2]}"
    case "$n" in 100000000|1000000000|10000000000|100000000000|1000000000000|10000000000000) ;; *) exit 2;; esac
    [[ ${#k} -le 4 ]] && ((k>=4 && k<=8192)) || exit 2
done
suffix=""
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) suffix=".exe";; esac
result_dir="results/$(date -u +%Y%m%dT%H%M%SZ)_calendar_matrix_$$"
mkdir -p -- "$result_dir"
trap 'status=$?; if ((status)); then printf "FAILED exit=%s; retained results: %s\n" "$status" "$result_dir" >&2; fi' EXIT
date -u +%FT%TZ > "$result_dir/started_utc.txt"
printf 'Results: %s\nCases N:KiB: %s\nThreads: %s\n' "$result_dir" "${cases[*]}" "${teams[*]}"
sha256sum -c SHA256SUMS.txt > "$result_dir/package_check.txt"
bash build_calendar.sh > "$result_dir/build.txt" 2>&1
export OMP_DYNAMIC=false OMP_PROC_BIND=false
unset OMP_PLACES
{
    uname -a
    printf 'cases=%s threads=%s pairs=2 excluded_warmups_per_engine=1 schedule_factor=2 build_mode=%s\n' "${cases[*]}" "${teams[*]}" "${RED2_BUILD_MODE:-native}"
    printf 'OMP_DYNAMIC=false OMP_PROC_BIND=false OMP_PLACES=unset OMP_THREAD_LIMIT=%s\n' "${OMP_THREAD_LIMIT:-unset}"
    if [[ -r /proc/cpuinfo ]]; then awk -F: '/model name/{print;exit}' /proc/cpuinfo; fi
} > "$result_dir/environment.txt"
sha256sum red2_range.c red2_range_cli.c red2_v33.h red2_v34.h red2_calendar.c red2_calendar_cli.c \
    validate_calendar_cases.c validate_v33.c validate_10t.c build_calendar.sh run_matrix.sh summarize_pairs.awk \
    "red2_original${suffix}" "red2_calendar${suffix}" "validate_original${suffix}" "validate_calendar${suffix}" > "$result_dir/run_sha256.txt"
for engine in original calendar; do
    "./validate_${engine}${suffix}" > "$result_dir/validation_${engine}.txt" 2>&1
    cat "$result_dir/validation_${engine}.txt"
done
for item in "${cases[@]}"; do
    n="${item%:*}";segment="${item#*:}"
    case "$n" in
        100000000) expected=5761455;; 1000000000) expected=50847534;;
        10000000000) expected=455052511;; 100000000000) expected=4118054813;;
        1000000000000) expected=37607912018;; 10000000000000) expected=346065536839;;
    esac
    for threads in "${teams[@]}"; do
        folder="$result_dir/N_${n}_K_${segment}_T_${threads}"
        mkdir -p -- "$folder"
        printf 'phase,pair,position,engine,seconds,count,segment_kib\n' > "$folder/raw.csv"
        for ((pair=-1;pair<2;pair++)); do
            phase=measured
            if ((pair<0)); then phase=warmup; order="calendar original"
            elif ((pair%2==0)); then order="calendar original"
            else order="original calendar"; fi
            read -r -a sequence <<< "$order"
            for ((position=1;position<=2;position++)); do
                engine="${sequence[$((position-1))]}";raw="$folder/${phase}_${pair}_${position}_${engine}.json"
                printf 'N=%s KiB=%s threads=%s %s pair=%s %s...\n' "$n" "$segment" "$threads" "$phase" "$pair" "$engine"
                "./red2_${engine}${suffix}" --limit "$n" --threads "$threads" --segment-kib "$segment" --frontier-multiplier 1 > "$raw" 2> "${raw%.json}_stderr.txt"
                seconds="$(sed -n 's/.*"total_seconds":\([0-9.]*\).*/\1/p' "$raw")"
                count="$(sed -n 's/.*"prime_count":\([0-9]*\).*/\1/p' "$raw")"
                [[ "$count" == "$expected" && "$seconds" =~ ^[0-9]+([.][0-9]+)?$ ]] || exit 1
                awk -v t="$seconds" 'BEGIN{exit !(t>0)}'
                printf '%s,%s,%s,%s,%s,%s,%s\n' "$phase" "$pair" "$position" "$engine" "$seconds" "$count" "$segment" >> "$folder/raw.csv"
            done
        done
        printf 'N=%s KiB=%s threads=%s\n' "$n" "$segment" "$threads" | tee -a "$result_dir/summary.txt"
        awk -f summarize_pairs.awk "$folder/raw.csv" | tee "$folder/summary.txt" | tee -a "$result_dir/summary.txt"
    done
done
sha256sum -c "$result_dir/run_sha256.txt" > "$result_dir/postrun_check.txt"
date -u +%FT%TZ > "$result_dir/finished_utc.txt"
printf 'COMPLETE: all counts matched. Results: %s\n' "$result_dir"
if command -v zip >/dev/null 2>&1; then
    zip -qr "$result_dir.zip" "$result_dir";printf 'Upload: %s.zip\n' "$result_dir"
elif command -v powershell.exe >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
    report_win="$(cygpath -aw "$result_dir")";zip_win="$(cygpath -aw "$result_dir.zip")"
    if RED2_REPORT_DIR="$report_win" RED2_REPORT_ZIP="$zip_win" powershell.exe -NoProfile -NonInteractive -Command 'Compress-Archive -LiteralPath $env:RED2_REPORT_DIR -DestinationPath $env:RED2_REPORT_ZIP -Force';then
        printf 'Upload: %s.zip\n' "$result_dir"
    else printf 'Automatic ZIP creation failed; zip the results folder above manually.\n';fi
else printf 'Zip the results folder above and upload it.\n';fi
