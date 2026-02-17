#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
RUN_DIR="${ROOT_DIR}/run"
MNT_DIR="${RUN_DIR}/mnt"
OUT_DIR="${RUN_DIR}/benchmarks"
IMAGE_PATH="${RUN_DIR}/benchmark.img"
RESULT_CSV="${OUT_DIR}/dd_results.csv"
SUMMARY_CSV="${OUT_DIR}/dd_summary.csv"
COMPARE_CSV="${OUT_DIR}/dd_compare.csv"
PLAN_FILE="${OUT_DIR}/run_plan.csv"

IMAGE_SIZE="${IMAGE_SIZE:-4G}"
INODES="${INODES:-4096}"
MOUNT_TIMEOUT_SEC="${MOUNT_TIMEOUT_SEC:-15}"
REPEAT="${REPEAT:-5}"
GLOBAL_WARMUP="${GLOBAL_WARMUP:-1}"
KEEP_IMAGES="${KEEP_IMAGES:-0}"
INCLUDE_BUFFERED="${INCLUDE_BUFFERED:-1}"
INCLUDE_KERNEL_COMPARE="${INCLUDE_KERNEL_COMPARE:-1}"

CASES=(
	"1K:16384"
	"4K:16384"
    "64K:16384"
    "256K:4096"
    "1M:1024"
    "16M:64"
    "256M:4"
)

if [[ -n "${CASES_CSV:-}" ]]; then
    IFS=',' read -r -a CASES <<< "${CASES_CSV}"
fi

MODES=("sync" "fdatasync")
if [[ "${INCLUDE_BUFFERED}" == "1" ]]; then
    MODES+=("buffered")
fi

BACKENDS=("fuse")
if [[ "${INCLUDE_KERNEL_COMPARE}" == "1" ]]; then
    BACKENDS+=("kernel")
fi
if [[ -n "${BACKENDS_CSV:-}" ]]; then
    IFS=',' read -r -a BACKENDS <<< "${BACKENDS_CSV}"
fi

CURRENT_FUSE_PID=""
CURRENT_BACKEND=""
ROOT_RUNNER=()

init_root_runner() {
    if [[ $EUID -eq 0 ]]; then
        ROOT_RUNNER=()
        return
    fi
    if ! command -v sudo >/dev/null 2>&1; then
        echo "Kernel mount requires root or sudo." >&2
        exit 1
    fi
    if ! sudo -n true >/dev/null 2>&1; then
        echo "Kernel mount requires passwordless sudo for this script." >&2
        exit 1
    fi
    ROOT_RUNNER=(sudo -n)
}

run_as_root() {
    if [[ "${#ROOT_RUNNER[@]}" -eq 0 ]]; then
        "$@"
    else
        "${ROOT_RUNNER[@]}" "$@"
    fi
}

check_kernel_minix_support() {
    if ! grep -qw minix /proc/filesystems; then
        if command -v modprobe >/dev/null 2>&1; then
            run_as_root modprobe minix >/dev/null 2>&1 || true
        fi
    fi
    if ! grep -qw minix /proc/filesystems; then
        echo "Kernel does not report minix filesystem support (/proc/filesystems)." >&2
        exit 1
    fi
}

wait_for_mount() {
    local pid="${1:-}"
    for ((i = 0; i < MOUNT_TIMEOUT_SEC * 10; i++)); do
        if [[ -n "${pid}" ]] && ! kill -0 "${pid}" >/dev/null 2>&1; then
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
        if [[ "${CURRENT_BACKEND}" == "fuse" ]] && command -v fusermount3 >/dev/null 2>&1; then
            fusermount3 -u "${MNT_DIR}" >/dev/null 2>&1
        fi
        if mountpoint -q "${MNT_DIR}"; then
            run_as_root umount "${MNT_DIR}" >/dev/null 2>&1
        fi
    fi

    if [[ -n "${CURRENT_FUSE_PID}" ]] && kill -0 "${CURRENT_FUSE_PID}" >/dev/null 2>&1; then
        kill "${CURRENT_FUSE_PID}" >/dev/null 2>&1
        wait "${CURRENT_FUSE_PID}" 2>/dev/null
    fi

    if [[ -f "${IMAGE_PATH}" && "${KEEP_IMAGES}" != "1" ]]; then
        rm -f "${IMAGE_PATH}"
    fi

    CURRENT_FUSE_PID=""
    CURRENT_BACKEND=""
    set -e
}

cleanup_on_exit() {
    local rc=$?
    cleanup_current_sample
    return "${rc}"
}
trap cleanup_on_exit EXIT INT TERM

run_sample() {
    local backend="$1"
    local mode="$2"
    local bs="$3"
    local count="$4"
    local sample_id="$5"
    local csv_path="${6:-}"

    CURRENT_BACKEND="${backend}"
    rm -f "${IMAGE_PATH}"
    truncate -s "${IMAGE_SIZE}" "${IMAGE_PATH}"
    mkfs.minix -3 "${IMAGE_PATH}" --inodes "${INODES}" >/dev/null

    if [[ "${backend}" == "fuse" ]]; then
        "${BUILD_DIR}/minixfs-fuse" --device="${IMAGE_PATH}" "${MNT_DIR}" -f -o auto_unmount &
        CURRENT_FUSE_PID=$!
        if ! wait_for_mount "${CURRENT_FUSE_PID}"; then
            echo "FUSE mount failed for sample ${sample_id}" >&2
            return 1
        fi
    elif [[ "${backend}" == "kernel" ]]; then
        run_as_root mount -t minix -o loop "${IMAGE_PATH}" "${MNT_DIR}"
        if ! wait_for_mount ""; then
            echo "Kernel mount failed for sample ${sample_id}" >&2
            return 1
        fi
    else
        echo "Unknown backend: ${backend}" >&2
        return 1
    fi

    bash "${ROOT_DIR}/benchmarks/dd.sh" "${MNT_DIR}" "${backend}" "${mode}" "${bs}" "${count}" "${sample_id}" "${csv_path}"
    cleanup_current_sample
}

prepare_backend_environment() {
    local backend="$1"
    if [[ "${backend}" == "kernel" ]]; then
        init_root_runner
        check_kernel_minix_support
    fi
}

echo "==> Building Release target..."
cmake --build "${BUILD_DIR}" --target all -j --config Release

mkdir -p "${MNT_DIR}" "${OUT_DIR}"
cleanup_current_sample
rm -f "${RESULT_CSV}" "${SUMMARY_CSV}" "${COMPARE_CSV}" "${PLAN_FILE}"

for backend in "${BACKENDS[@]}"; do
    prepare_backend_environment "${backend}"
done

if [[ "${GLOBAL_WARMUP}" == "1" ]]; then
    echo "==> Running warmup sample..."
    for backend in "${BACKENDS[@]}"; do
        run_sample "${backend}" "fdatasync" "1M" "256" "warmup_${backend}" ""
    done
fi

echo "==> Creating randomized run plan..."
tmp_plan="${PLAN_FILE}.tmp"
: > "${tmp_plan}"
for backend in "${BACKENDS[@]}"; do
    for mode in "${MODES[@]}"; do
        for entry in "${CASES[@]}"; do
            bs="${entry%%:*}"
            count="${entry##*:}"
            for ((rep = 1; rep <= REPEAT; rep++)); do
                printf "%s,%s,%s,%s,%s\n" "${backend}" "${mode}" "${bs}" "${count}" "${rep}" >> "${tmp_plan}"
            done
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
while IFS=, read -r backend mode bs count rep; do
    current_run=$((current_run + 1))
    sample_id=$(printf "%04d_%s_%s_%s_r%s" "${current_run}" "${backend}" "${mode}" "${bs}" "${rep}")
    echo "[${current_run}/${total_runs}] backend=${backend} mode=${mode} bs=${bs} repeat=${rep}"
    run_sample "${backend}" "${mode}" "${bs}" "${count}" "${sample_id}" "${RESULT_CSV}"
done < "${PLAN_FILE}"

echo "==> Aggregating summary..."
{
    echo "backend,mode,bs,samples,median_mib_per_s,avg_mib_per_s,min_mib_per_s,max_mib_per_s,p90_mib_per_s"
    tail -n +2 "${RESULT_CSV}" | sort -t',' -k2,2 -k3,3 -k4,4 -k9,9n | awk -F',' '
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
            printf "%s,%s,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f\n", curBackend, curMode, curBs, count, median, avg, minv, maxv, p90
            delete speed
            count = 0
            sum = 0
            minv = 0
            maxv = 0
        }
        {
            backend = $2
            mode = $3
            bs = $4
            s = $9 + 0
            if (count == 0) {
                curBackend = backend
                curMode = mode
                curBs = bs
                minv = s
                maxv = s
            } else if (backend != curBackend || mode != curMode || bs != curBs) {
                flush_group()
                curBackend = backend
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

{
    echo "mode,bs,fuse_median_mib_per_s,kernel_median_mib_per_s,fuse_to_kernel_ratio"
    awk -F',' '
        NR == 1 {
            next
        }
        {
            key = $2 "," $3
            if ($1 == "fuse") {
                fuse[key] = $5 + 0
            } else if ($1 == "kernel") {
                kernel[key] = $5 + 0
            }
            keys[key] = 1
        }
        END {
            for (k in keys) {
                split(k, p, ",")
                fm = (k in fuse) ? fuse[k] : 0
                km = (k in kernel) ? kernel[k] : 0
                if (fm > 0 && km > 0) {
                    ratio = fm / km
                    printf "%s,%s,%.2f,%.2f,%.4f\n", p[1], p[2], fm, km, ratio
                } else {
                    printf "%s,%s,%.2f,%.2f,\n", p[1], p[2], fm, km
                }
            }
        }
    ' "${SUMMARY_CSV}" | sort -t',' -k1,1 -k2,2
} > "${COMPARE_CSV}"

echo "==> Benchmark finished."
echo "    Results: ${RESULT_CSV}"
echo "    Summary: ${SUMMARY_CSV}"
echo "    Compare: ${COMPARE_CSV}"
echo "    Plan: ${PLAN_FILE}"
