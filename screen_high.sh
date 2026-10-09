#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
export LC_ALL=C OMP_DYNAMIC=false OMP_PROC_BIND=false
unset OMP_PLACES
threads="${RED2_THREADS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf 1)}"
[[ "$threads" =~ ^[1-9][0-9]*$ && ${#threads} -le 4 ]] || exit 2
((threads<=1024)) || exit 2
suffix=""; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) suffix=".exe";; esac
result_dir="results/$(date -u +%Y%m%dT%H%M%SZ)_screen_$$"
mkdir -p -- "$result_dir"
trap 'status=$?; if ((status)); then printf "FAILED exit=%s; retained results: %s\n" "$status" "$result_dir" >&2; fi' EXIT
printf 'Results: %s\nShort windows at global N=1T, 3T and 10T; these timings are NOT complete sieves to N.\n' "$result_dir"
date -u +%FT%TZ > "$result_dir/started_utc.txt"
sha256sum -c SHA256SUMS.txt > "$result_dir/package_check.txt"
bash build_high.sh > "$result_dir/build.txt" 2>&1
sha256sum *.c *.h *.sh *.awk "window_v34${suffix}" "window_v35${suffix}" > "$result_dir/run_sha256.txt"
{ uname -a; printf 'threads=%s segment_kib=512 OMP_DYNAMIC=false OMP_PROC_BIND=false OMP_PLACES=unset\n' "$threads"; } > "$result_dir/environment.txt"
"./validate_v35${suffix}" | tee "$result_dir/validation.txt"
case_id=0
while read -r limit first count; do
    printf 'Case %s: N=%s first_segment=%s segments=%s threads=%s\n' "$case_id" "$limit" "$first" "$count" "$threads"
    "./window_v35${suffix}" "$limit" "$threads" 512 "$first" 4 0 0 1 1 > "$result_dir/check_${case_id}.json"
    for tile in 0 64; do
        out="$result_dir/case_${case_id}_tile_${tile}.csv"
        printf 'phase,pair,position,engine,seconds,count,segment_kib\n' > "$out"
        # One warmup per engine, then alternating measured AB/BA pairs.
        for pair in -1 0 1; do
            phase=measured; order="v35 v34"
            if ((pair<0)); then phase=warmup; elif ((pair%2)); then order="v34 v35"; fi
            position=0
            for engine in $order; do
                position=$((position+1))
                raw="$result_dir/window_${case_id}_${tile}_${pair}_${position}_${engine}.json"
                "./window_${engine}${suffix}" "$limit" "$threads" 512 "$first" "$count" 0 "$tile" 1 0 > "$raw"
                seconds="$(sed -n 's/.*"work_seconds":\([0-9.]*\).*/\1/p' "$raw")"
                survivors="$(sed -n 's/.*"bitmap_survivors":\([0-9]*\).*/\1/p' "$raw")"
                [[ "$seconds" =~ ^[0-9]+([.][0-9]+)?$ && "$survivors" =~ ^[0-9]+$ ]] || exit 1
                if ((position==1)); then expected="$survivors"; else [[ "$survivors" == "$expected" ]] || exit 1; fi
                printf '%s,%s,%s,%s,%s,%s,512\n' "$phase" "$pair" "$position" "$engine" "$seconds" "$survivors" | tee -a "$out"
            done
        done
        printf 'WINDOW WORK TIME: case=%s dense_tile_kib=%s (0=auto)\n' "$case_id" "$tile"
        awk -f summarize_pairs.awk "$out" | tee "$result_dir/summary_${case_id}_${tile}.txt"
    done
    case_id=$((case_id+1))
done <<'CASES'
1000000000000 60000 2048
3000000000000 180000 2048
10000000000000 300000 2048
10000000000000 630000 2048
CASES
sha256sum -c "$result_dir/run_sha256.txt" > "$result_dir/postrun_check.txt"
date -u +%FT%TZ > "$result_dir/finished_utc.txt"
printf 'Complete. All comparisons used window work time; initialization is preserved separately in each JSON.\n'
if command -v zip >/dev/null 2>&1; then
    zip -qr "$result_dir.zip" "$result_dir"
    printf 'Upload: %s.zip\n' "$result_dir"
elif command -v powershell.exe >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
    report_win="$(cygpath -aw "$result_dir")"; zip_win="$(cygpath -aw "$result_dir.zip")"
    if RED2_REPORT_DIR="$report_win" RED2_REPORT_ZIP="$zip_win" powershell.exe -NoProfile -NonInteractive -Command 'Compress-Archive -LiteralPath $env:RED2_REPORT_DIR -DestinationPath $env:RED2_REPORT_ZIP -Force'; then
        printf 'Upload: %s.zip\n' "$result_dir"
    else printf 'Automatic ZIP creation failed; zip the results folder above manually.\n'; fi
else printf 'Zip the results folder above and upload it.\n'; fi
