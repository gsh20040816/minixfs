#!/usr/bin/env bash

set -euo pipefail

MNT_DIR="${1:-run/mnt}"
OUT_DIR="${2:-run/benchmarks}"
REPEAT="${REPEAT:-3}"

RESULT_CSV="${OUT_DIR}/dd_results.csv"
SUMMARY_TXT="${OUT_DIR}/dd_summary.txt"

CASES=(
    "64K:16384"
    "256K:4096"
    "1M:1024"
    "4M:256"
    "16M:64"
    "64M:16"
    "256M:4"
    "1G:1"
)

MODES=("sync" "fdatasync" "buffered")

mkdir -p "${OUT_DIR}"

if ! mountpoint -q "${MNT_DIR}"; then
    echo "Mountpoint is not active: ${MNT_DIR}" >&2
    exit 1
fi

printf "mode,bs,count,run,seconds,bytes,mib_per_s\n" > "${RESULT_CSV}"

run_one_case() {
    local mode="$1"
    local bs="$2"
    local count="$3"
    local run_id="$4"
    local output_file="${MNT_DIR}/dd_${mode}_${bs}_r${run_id}.bin"
    local start_ns end_ns seconds bytes mib_per_s
    local -a dd_args

    dd_args=("if=/dev/zero" "of=${output_file}" "bs=${bs}" "count=${count}" "status=none")
    case "${mode}" in
        sync)
            dd_args+=("oflag=sync")
            ;;
        fdatasync)
            dd_args+=("conv=fdatasync")
            ;;
        buffered)
            ;;
        *)
            echo "Unknown mode: ${mode}" >&2
            exit 1
            ;;
    esac

    start_ns=$(date +%s%N)
    dd "${dd_args[@]}"
    end_ns=$(date +%s%N)

    bytes=$(stat -c '%s' "${output_file}")
    seconds=$(awk -v start="${start_ns}" -v end="${end_ns}" 'BEGIN { printf "%.6f", (end - start) / 1000000000 }')
    mib_per_s=$(awk -v b="${bytes}" -v s="${seconds}" 'BEGIN { if (s <= 0) { printf "0.00" } else { printf "%.2f", b / 1024 / 1024 / s } }')

    printf "%s,%s,%s,%s,%s,%s,%s\n" "${mode}" "${bs}" "${count}" "${run_id}" "${seconds}" "${bytes}" "${mib_per_s}" >> "${RESULT_CSV}"
    printf "mode=%-9s bs=%-5s run=%s speed=%8s MiB/s time=%ss\n" "${mode}" "${bs}" "${run_id}" "${mib_per_s}" "${seconds}"

    rm -f "${output_file}"
}

echo "Running dd benchmark on ${MNT_DIR} (repeat=${REPEAT})..."
for mode in "${MODES[@]}"; do
    for entry in "${CASES[@]}"; do
        bs="${entry%%:*}"
        count="${entry##*:}"
        for ((run_id = 1; run_id <= REPEAT; run_id++)); do
            run_one_case "${mode}" "${bs}" "${count}" "${run_id}"
        done
    done
done

{
    echo "mode,bs,avg_mib_per_s,min_mib_per_s,max_mib_per_s,samples"
    awk -F',' '
        NR > 1 {
            key = $1 "," $2
            speed = $7 + 0
            sum[key] += speed
            cnt[key] += 1
            if (!(key in min) || speed < min[key]) min[key] = speed
            if (!(key in max) || speed > max[key]) max[key] = speed
        }
        END {
            for (k in cnt) {
                printf "%s,%.2f,%.2f,%.2f,%d\n", k, sum[k] / cnt[k], min[k], max[k], cnt[k]
            }
        }
    ' "${RESULT_CSV}" | sort -t',' -k1,1 -k2,2
} > "${SUMMARY_TXT}"

echo "CSV: ${RESULT_CSV}"
echo "Summary: ${SUMMARY_TXT}"
