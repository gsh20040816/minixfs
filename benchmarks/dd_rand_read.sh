#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 6 ]]; then
    echo "Usage: $0 <mount_dir> <backend> <mode> <bs> <count> <sample_id> [csv_path]" >&2
    echo "backend: fuse | kernel" >&2
    echo "mode: buffered (sync/fdatasync accepted and treated as buffered)" >&2
    exit 1
fi

parse_size_to_bytes() {
    local token="$1"
    if command -v numfmt >/dev/null 2>&1; then
        numfmt --from=iec "${token}"
        return
    fi

    if [[ "${token}" =~ ^([0-9]+)([kKmMgGtTpP]?)$ ]]; then
        local value="${BASH_REMATCH[1]}"
        local unit="${BASH_REMATCH[2]}"
        local factor=1
        case "${unit}" in
            k | K)
                factor=1024
                ;;
            m | M)
                factor=$((1024 * 1024))
                ;;
            g | G)
                factor=$((1024 * 1024 * 1024))
                ;;
            t | T)
                factor=$((1024 * 1024 * 1024 * 1024))
                ;;
            p | P)
                factor=$((1024 * 1024 * 1024 * 1024 * 1024))
                ;;
        esac
        echo $((value * factor))
        return
    fi

    echo "${token}"
}

MNT_DIR="$1"
BACKEND="$2"
MODE="$3"
BS="$4"
COUNT="$5"
SAMPLE_ID="$6"
CSV_PATH="${7:-}"
WORKLOAD="rand_read"

if ! mountpoint -q "${MNT_DIR}"; then
    echo "Mountpoint is not active: ${MNT_DIR}" >&2
    exit 1
fi

case "${MODE}" in
    sync | fdatasync | buffered)
        ;;
    *)
        echo "Unknown mode: ${MODE}" >&2
        exit 1
        ;;
esac

if [[ "${COUNT}" -le 0 ]]; then
    echo "count must be positive, got: ${COUNT}" >&2
    exit 1
fi

INPUT_FILE="${MNT_DIR}/dd_${WORKLOAD}_${BS}_${SAMPLE_ID}.bin"

cleanup() {
    rm -f "${INPUT_FILE}"
}
trap cleanup EXIT INT TERM

bs_bytes="$(parse_size_to_bytes "${BS}")"

# Pre-create source file outside timed section.
dd if=/dev/zero of="${INPUT_FILE}" bs="${BS}" count="${COUNT}" conv=fdatasync status=none

start_ns=$(date +%s%N)
for ((i = 0; i < COUNT; i++)); do
    block_index=$((RANDOM % COUNT))
    dd if="${INPUT_FILE}" of=/dev/null bs="${BS}" count=1 skip="${block_index}" status=none
done
end_ns=$(date +%s%N)

bytes=$((bs_bytes * COUNT))
seconds=$(awk -v start="${start_ns}" -v end="${end_ns}" 'BEGIN { printf "%.6f", (end - start) / 1000000000 }')
mib_per_s=$(awk -v b="${bytes}" -v s="${seconds}" 'BEGIN { if (s <= 0) { printf "0.00" } else { printf "%.2f", b / 1024 / 1024 / s } }')
ops_per_s=$(awk -v o="${COUNT}" -v s="${seconds}" 'BEGIN { if (s <= 0) { printf "0.00" } else { printf "%.2f", o / s } }')

printf "sample=%-18s workload=%-16s backend=%-6s mode=%-9s bs=%-5s speed=%8s MiB/s ops=%8s ops/s time=%ss\n" "${SAMPLE_ID}" "${WORKLOAD}" "${BACKEND}" "${MODE}" "${BS}" "${mib_per_s}" "${ops_per_s}" "${seconds}"

if [[ -n "${CSV_PATH}" ]]; then
    mkdir -p "$(dirname "${CSV_PATH}")"
    if [[ ! -f "${CSV_PATH}" ]]; then
        printf "timestamp,workload,backend,mode,bs,count,sample_id,seconds,bytes,operations,mib_per_s,ops_per_s\n" > "${CSV_PATH}"
    fi
    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" "$(date -Iseconds)" "${WORKLOAD}" "${BACKEND}" "${MODE}" "${BS}" "${COUNT}" "${SAMPLE_ID}" "${seconds}" "${bytes}" "${COUNT}" "${mib_per_s}" "${ops_per_s}" >> "${CSV_PATH}"
fi
