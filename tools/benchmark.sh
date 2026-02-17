#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
RUN_DIR="${ROOT_DIR}/run"
MNT_DIR="${RUN_DIR}/mnt"
OUT_DIR="${RUN_DIR}/benchmarks"
RESULT_CSV="${OUT_DIR}/dd_results.csv"
SUMMARY_CSV="${OUT_DIR}/dd_summary.csv"
PLAN_FILE="${OUT_DIR}/run_plan.csv"

IMAGE_SIZE="${IMAGE_SIZE:-4G}"
INODES="${INODES:-4096}"
MOUNT_TIMEOUT_SEC="${MOUNT_TIMEOUT_SEC:-15}"
REPEAT="${REPEAT:-5}"
GLOBAL_WARMUP="${GLOBAL_WARMUP:-1}"
KEEP_IMAGES="${KEEP_IMAGES:-0}"
INCLUDE_BUFFERED="${INCLUDE_BUFFERED:-1}"

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

if [[ -n "${CASES_CSV:-}" ]]; then
    IFS=',' read -r -a CASES <<< "${CASES_CSV}"
fi

MODES=("sync" "fdatasync")
if [[ "${INCLUDE_BUFFERED}" == "1" ]]; then
    MODES+=("buffered")
fi

CURRENT_FUSE_PID=""
CURRENT_IMG_PATH=""

wait_for_mount() {
    local pid="$1"
    for ((i = 0; i < MOUNT_TIMEOUT_SEC * 10; i++)); do
        if ! kill -0 "${pid}" >/dev/null 2>&1; then
            return 1
        fi
        if mountpoint -q "${MNT_DIR}"; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

cleanup_current_sample() {
    set +e

    if mountpoint -q "${MNT_DIR}"; then
        if command -v fusermount3 >/dev/null 2>&1; then
            fusermount3 -u "${MNT_DIR}" >/dev/null 2>&1
        else
            umount "${MNT_DIR}" >/dev/null 2>&1
        fi
    fi

    if [[ -n "${CURRENT_FUSE_PID}" ]] && kill -0 "${CURRENT_FUSE_PID}" >/dev/null 2>&1; then
        kill "${CURRENT_FUSE_PID}" >/dev/null 2>&1
        wait "${CURRENT_FUSE_PID}" 2>/dev/null
    fi

    if [[ -n "${CURRENT_IMG_PATH}" && "${KEEP_IMAGES}" != "1" ]]; then
        rm -f "${CURRENT_IMG_PATH}"
    fi

    CURRENT_FUSE_PID=""
    CURRENT_IMG_PATH=""
    set -e
}

cleanup_on_exit() {
    local rc=$?
    cleanup_current_sample
    return "${rc}"
}
trap cleanup_on_exit EXIT INT TERM

run_sample() {
    local mode="$1"
    local bs="$2"
    local count="$3"
    local sample_id="$4"
    local csv_path="${5:-}"

    CURRENT_IMG_PATH="${RUN_DIR}/benchmark_${sample_id}.img"
    rm -f "${CURRENT_IMG_PATH}"

    truncate -s "${IMAGE_SIZE}" "${CURRENT_IMG_PATH}"
    mkfs.minix -3 "${CURRENT_IMG_PATH}" --inodes "${INODES}" >/dev/null

    "${BUILD_DIR}/minixfs-fuse" --device="${CURRENT_IMG_PATH}" "${MNT_DIR}" -f -o auto_unmount &
    CURRENT_FUSE_PID=$!

    if ! wait_for_mount "${CURRENT_FUSE_PID}"; then
        echo "Mount failed for sample ${sample_id}" >&2
        return 1
    fi

    bash "${ROOT_DIR}/benchmarks/dd.sh" "${MNT_DIR}" "${mode}" "${bs}" "${count}" "${sample_id}" "${csv_path}"
    cleanup_current_sample
}

echo "==> Building Release target..."
cmake --build "${BUILD_DIR}" --target all -j --config Release

mkdir -p "${MNT_DIR}" "${OUT_DIR}"
cleanup_current_sample
rm -f "${RESULT_CSV}" "${SUMMARY_CSV}" "${PLAN_FILE}"

if [[ "${GLOBAL_WARMUP}" == "1" ]]; then
    echo "==> Running warmup sample..."
    run_sample "fdatasync" "1M" "256" "warmup" ""
fi

echo "==> Creating randomized run plan..."
tmp_plan="${PLAN_FILE}.tmp"
: > "${tmp_plan}"
for mode in "${MODES[@]}"; do
    for entry in "${CASES[@]}"; do
        bs="${entry%%:*}"
        count="${entry##*:}"
        for ((rep = 1; rep <= REPEAT; rep++)); do
            printf "%s,%s,%s,%s\n" "${mode}" "${bs}" "${count}" "${rep}" >> "${tmp_plan}"
        done
    done
done

if command -v shuf >/dev/null 2>&1; then
    shuf "${tmp_plan}" > "${PLAN_FILE}"
else
    cp "${tmp_plan}" "${PLAN_FILE}"
fi
rm -f "${tmp_plan}"

total_runs=$(wc -l < "${PLAN_FILE}")
current_run=0

echo "==> Running benchmark samples (${total_runs} runs)..."
while IFS=, read -r mode bs count rep; do
    current_run=$((current_run + 1))
    sample_id=$(printf "%04d_%s_%s_r%s" "${current_run}" "${mode}" "${bs}" "${rep}")
    echo "[${current_run}/${total_runs}] mode=${mode} bs=${bs} repeat=${rep}"
    run_sample "${mode}" "${bs}" "${count}" "${sample_id}" "${RESULT_CSV}"
done < "${PLAN_FILE}"

echo "==> Aggregating summary..."
{
    echo "mode,bs,samples,median_mib_per_s,avg_mib_per_s,min_mib_per_s,max_mib_per_s,p90_mib_per_s"
    tail -n +2 "${RESULT_CSV}" | sort -t',' -k2,2 -k3,3 -k8,8n | awk -F',' '
        function flush_group(    median, p90idx, p90, avg) {
            if (count == 0) {
                return
            }
            if (count % 2 == 1) {
                median = speed[int(count / 2) + 1]
            } else {
                median = (speed[count / 2] + speed[count / 2 + 1]) / 2
            }
            p90idx = int((count - 1) * 0.9) + 1
            p90 = speed[p90idx]
            avg = sum / count
            printf "%s,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f\n", curMode, curBs, count, median, avg, minv, maxv, p90
            delete speed
            count = 0
            sum = 0
            minv = 0
            maxv = 0
        }
        {
            mode = $2
            bs = $3
            s = $8 + 0
            if (count == 0) {
                curMode = mode
                curBs = bs
                minv = s
                maxv = s
            } else if (mode != curMode || bs != curBs) {
                flush_group()
                curMode = mode
                curBs = bs
                minv = s
                maxv = s
            }
            count += 1
            speed[count] = s
            sum += s
            if (s < minv) {
                minv = s
            }
            if (s > maxv) {
                maxv = s
            }
        }
        END {
            flush_group()
        }
    '
} > "${SUMMARY_CSV}"

echo "==> Benchmark finished."
echo "    Results: ${RESULT_CSV}"
echo "    Summary: ${SUMMARY_CSV}"
echo "    Plan: ${PLAN_FILE}"
