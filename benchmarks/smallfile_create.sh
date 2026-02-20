#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 6 ]]; then
    echo "Usage: $0 <mount_dir> <backend> <mode> <bs> <count> <sample_id> [csv_path]" >&2
    echo "backend: fuse | kernel" >&2
    echo "mode: sync | fdatasync | buffered" >&2
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
WORKLOAD="smallfile_create"
SAFE_SAMPLE_ID="${SAMPLE_ID//[^a-zA-Z0-9]/_}"
SAFE_SAMPLE_ID="${SAFE_SAMPLE_ID:0:16}"
if [[ -z "${SAFE_SAMPLE_ID}" ]]; then
    SAFE_SAMPLE_ID="sample"
fi

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

BENCH_DIR="${MNT_DIR}/smallfile_${MODE}_${BS}_${SAFE_SAMPLE_ID}"

cleanup() {
    rm -rf "${BENCH_DIR}"
}
trap cleanup EXIT INT TERM

mkdir -p "${BENCH_DIR}"
bs_bytes="$(parse_size_to_bytes "${BS}")"

start_ns=$(date +%s%N)
for ((i = 0; i < COUNT; i++)); do
    file_path="${BENCH_DIR}/file_$(printf '%06d' "${i}").bin"
    declare -a DD_ARGS
    DD_ARGS=("if=/dev/zero" "of=${file_path}" "bs=${BS}" "count=1" "status=none")
    case "${MODE}" in
        sync)
            DD_ARGS+=("oflag=sync")
            ;;
        fdatasync)
            DD_ARGS+=("conv=fdatasync")
            ;;
        buffered)
            ;;
    esac
    dd "${DD_ARGS[@]}"
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
