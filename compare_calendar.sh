#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
export LC_ALL=C
limit="${RED2_LIMIT:-10000000000000}"
threads="${RED2_THREADS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf 1)}"
pairs="${RED2_PAIRS:-2}"
segment="${RED2_SEGMENT_KIB:-512}"
for v in "$limit" "$threads" "$pairs" "$segment"; do
    [[ "$v" =~ ^[1-9][0-9]*$ ]] || { printf 'Use positive decimal integers.\n' >&2; exit 2; }
done
[[ ${#threads} -le 4 && ${#pairs} -le 2 && ${#segment} -le 4 ]] || exit 2
((threads<=1024 && pairs>=2 && pairs<=20 && pairs%2==0 && segment>=4 && segment<=8192)) || exit 2
case "$limit" in
    100000000) expected=5761455;; 1000000000) expected=50847534;;
    10000000000) expected=455052511;; 100000000000) expected=4118054813;;
    1000000000000) expected=37607912018;; 10000000000000) expected=346065536839;;
    *) printf 'Use powers of ten from 10^8 through 10^13.\n' >&2; exit 2;;
esac
reference=original
if [[ $# -gt 0 ]]; then
    reference=primesieve; ps_exe="$1"
    if command -v cygpath >/dev/null 2>&1 && [[ "$ps_exe" == *:* ]]; then ps_exe="$(cygpath -u "$ps_exe")"; fi
    command -v "$ps_exe" >/dev/null 2>&1 || { printf 'Primesieve path not found.\n' >&2; exit 2; }
    ps_exe="$(command -v "$ps_exe")"
fi
suffix=""
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) suffix=".exe";; esac
result_dir="results/$(date -u +%Y%m%dT%H%M%SZ)_calendar_$$"
mkdir -p -- "$result_dir"
trap 'status=$?; if ((status)); then printf "FAILED exit=%s; retained results: %s\n" "$status" "$result_dir" >&2; fi' EXIT
date -u +%FT%TZ > "$result_dir/started_utc.txt"
printf 'Results: %s\nN=%s threads=%s segment_kib=%s pairs=%s reference=%s\n' "$result_dir" "$limit" "$threads" "$segment" "$pairs" "$reference"
sha256sum -c SHA256SUMS.txt > "$result_dir/package_check.txt"
bash build_calendar.sh > "$result_dir/build.txt" 2>&1
export OMP_DYNAMIC=false OMP_PROC_BIND=false
unset OMP_PLACES
{
    date -u +%FT%TZ; uname -a
    printf 'limit=%s threads=%s segment_kib=%s pairs=%s reference=%s schedule_factor=2 frontier_multiplier=1\n' "$limit" "$threads" "$segment" "$pairs" "$reference"
    printf 'OMP_DYNAMIC=false OMP_PROC_BIND=false OMP_PLACES=unset OMP_THREAD_LIMIT=%s RED2_BUILD_MODE=%s\n' "${OMP_THREAD_LIMIT:-unset}" "${RED2_BUILD_MODE:-native}"
    if [[ -r /proc/cpuinfo ]]; then awk -F: '/model name/{print;exit}' /proc/cpuinfo; fi
    if [[ -r /proc/meminfo ]]; then awk '/^MemTotal:|^MemAvailable:/{print}' /proc/meminfo; fi
} > "$result_dir/environment.txt"
sha256sum red2_range.c red2_range_cli.c red2_v33.h red2_v34.h red2_calendar.c red2_calendar_cli.c \
    validate_calendar_cases.c validate_v33.c validate_10t.c build_calendar.sh compare_calendar.sh summarize_pairs.awk \
    "red2_original${suffix}" "red2_calendar${suffix}" "validate_original${suffix}" "validate_calendar${suffix}" "validate_range_calendar${suffix}" > "$result_dir/run_sha256.txt"
if [[ "$reference" == primesieve ]]; then
    "$ps_exe" --version > "$result_dir/primesieve_version.txt" 2>&1
    "$ps_exe" --cpu-info > "$result_dir/primesieve_cpu_info.txt" 2>&1 || true
    sha256sum "$ps_exe" >> "$result_dir/run_sha256.txt"
fi
for engine in original calendar; do
    "./validate_${engine}${suffix}" > "$result_dir/validation_${engine}.txt" 2>&1
    cat "$result_dir/validation_${engine}.txt"
done
printf 'Checking scheduled rows against an independent bitmap oracle at one billion...\n'
"./validate_range_calendar${suffix}" 1000000000 "$threads" 4 1 > "$result_dir/oracle_1b.txt" 2> "$result_dir/oracle_1b_stderr.txt"
tail -n 1 "$result_dir/oracle_1b.txt"
printf 'phase,pair,position,engine,seconds,count,segment_kib\n' > "$result_dir/raw.csv"
for ((pair=-1;pair<pairs;pair++)); do
    phase=measured
    if ((pair<0)); then phase=warmup; order="calendar $reference"
    elif ((pair%2==0)); then order="calendar $reference"
    else order="$reference calendar"; fi
    read -r -a sequence <<< "$order"
    for ((position=1;position<=2;position++)); do
        engine="${sequence[$((position-1))]}"
        raw="$result_dir/${phase}_${pair}_${position}_${engine}.txt"
        printf 'Running N=%s %s pair=%s position=%s %s...\n' "$limit" "$phase" "$pair" "$position" "$engine"
        if [[ "$engine" == primesieve ]]; then
            size=128
            "$ps_exe" "$limit" --count --threads="$threads" --size=128 --time > "$raw" 2> "${raw%.txt}_stderr.txt"
            seconds="$(tr -d '\r' < "$raw" | awk '/^Seconds:/ {print $2}')"
            count="$(tr -d '\r' < "$raw" | awk '/^Primes:/ {print $2}')"
        else
            size="$segment"
            "./red2_${engine}${suffix}" --limit "$limit" --threads "$threads" --segment-kib "$segment" --frontier-multiplier 1 > "$raw" 2> "${raw%.txt}_stderr.txt"
            seconds="$(sed -n 's/.*"total_seconds":\([0-9.]*\).*/\1/p' "$raw")"
            count="$(sed -n 's/.*"prime_count":\([0-9]*\).*/\1/p' "$raw")"
        fi
        [[ "$count" == "$expected" && "$seconds" =~ ^[0-9]+([.][0-9]+)?$ ]] || { printf 'FAIL: inspect %s\n' "$raw" >&2; exit 1; }
        awk -v t="$seconds" 'BEGIN{exit !(t>0)}'
        printf '%s,%s,%s,%s,%s,%s,%s\n' "$phase" "$pair" "$position" "$engine" "$seconds" "$count" "$size" | tee -a "$result_dir/raw.csv"
    done
done
awk -f summarize_pairs.awk "$result_dir/raw.csv" | tee "$result_dir/summary.txt"
sha256sum -c "$result_dir/run_sha256.txt" > "$result_dir/postrun_check.txt"
date -u +%FT%TZ > "$result_dir/finished_utc.txt"
printf 'COMPLETE: every count matched %s.\n' "$expected"
if command -v zip >/dev/null 2>&1; then
    zip -qr "$result_dir.zip" "$result_dir"
    printf 'Upload: %s.zip\n' "$result_dir"
elif command -v powershell.exe >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
    report_win="$(cygpath -aw "$result_dir")"; zip_win="$(cygpath -aw "$result_dir.zip")"
    if RED2_REPORT_DIR="$report_win" RED2_REPORT_ZIP="$zip_win" powershell.exe -NoProfile -NonInteractive -Command 'Compress-Archive -LiteralPath $env:RED2_REPORT_DIR -DestinationPath $env:RED2_REPORT_ZIP -Force'; then
        printf 'Upload: %s.zip\n' "$result_dir"
    else printf 'Automatic ZIP creation failed; zip the results folder above manually.\n'; fi
else printf 'Zip the results folder above and upload it.\n'; fi
