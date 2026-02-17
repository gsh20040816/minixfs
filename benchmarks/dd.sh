#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 5 ]]; then
    echo "Usage: $0 <mount_dir> <mode> <bs> <count> <sample_id> [csv_path]" >&2
    echo "mode: sync | fdatasync | buffered" >&2
    exit 1
fi

MNT_DIR="$1"
MODE="$2"
BS="$3"
COUNT="$4"
SAMPLE_ID="$5"
CSV_PATH="${6:-}"

if ! mountpoint -q "${MNT_DIR}"; then
    echo "Mountpoint is not active: ${MNT_DIR}" >&2
    exit 1
fi

OUTPUT_FILE="${MNT_DIR}/dd_${MODE}_${BS}_${SAMPLE_ID}.bin"

cleanup() {
    rm -f "${OUTPUT_FILE}"
}
trap cleanup EXIT INT TERM

declare -a DD_ARGS
DD_ARGS=("if=/dev/zero" "of=${OUTPUT_FILE}" "bs=${BS}" "count=${COUNT}" "status=none")

case "${MODE}" in
    sync)
        DD_ARGS+=("oflag=sync")
        ;;
    fdatasync)
        DD_ARGS+=("conv=fdatasync")
        ;;
    buffered)
        ;;
    *)
        echo "Unknown mode: ${MODE}" >&2
        exit 1
        ;;
esac

start_ns=$(date +%s%N)
dd "${DD_ARGS[@]}"
end_ns=$(date +%s%N)

bytes=$(stat -c '%s' "${OUTPUT_FILE}")
seconds=$(awk -v start="${start_ns}" -v end="${end_ns}" 'BEGIN { printf "%.6f", (end - start) / 1000000000 }')
mib_per_s=$(awk -v b="${bytes}" -v s="${seconds}" 'BEGIN { if (s <= 0) { printf "0.00" } else { printf "%.2f", b / 1024 / 1024 / s } }')

printf "sample=%-18s mode=%-9s bs=%-5s speed=%8s MiB/s time=%ss\n" "${SAMPLE_ID}" "${MODE}" "${BS}" "${mib_per_s}" "${seconds}"

if [[ -n "${CSV_PATH}" ]]; then
    mkdir -p "$(dirname "${CSV_PATH}")"
    if [[ ! -f "${CSV_PATH}" ]]; then
        printf "timestamp,mode,bs,count,sample_id,seconds,bytes,mib_per_s\n" > "${CSV_PATH}"
    fi
    printf "%s,%s,%s,%s,%s,%s,%s,%s\n" "$(date -Iseconds)" "${MODE}" "${BS}" "${COUNT}" "${SAMPLE_ID}" "${seconds}" "${bytes}" "${mib_per_s}" >> "${CSV_PATH}"
fi
