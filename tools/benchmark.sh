#!/usr/bin/env bash

set -euo pipefail

usage() {
    echo "Usage: sudo $0 <device_or_image>" >&2
    echo "Example (block): sudo $0 /dev/nvme0n1p7" >&2
    echo "Example (image): sudo $0 run/benchmark.img" >&2
    echo "For image path, a new image will be created if it does not exist." >&2
}

if [[ $# -lt 1 ]]; then
    usage
    exit 1
fi

TARGET_PATH="$1"
TARGET_KIND=""
if [[ -b "${TARGET_PATH}" ]]; then
    TARGET_KIND="block"
elif [[ -f "${TARGET_PATH}" ]]; then
    TARGET_KIND="image"
elif [[ ! -e "${TARGET_PATH}" ]]; then
    TARGET_KIND="image"
else
    echo "Path is neither a block device nor a regular image file: ${TARGET_PATH}" >&2
    exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
    echo "Please run with sudo: sudo $0 ${TARGET_PATH}" >&2
    exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
RUN_DIR="${ROOT_DIR}/run"
MNT_DIR="${RUN_DIR}/mnt"
BENCHMARK_TS="${BENCHMARK_TS:-$(date +%Y%m%d_%H%M%S)}"
OUT_DIR_BASE="${OUT_DIR_BASE:-${RUN_DIR}/benchmarks}"
OUT_DIR="${OUT_DIR_BASE}_${BENCHMARK_TS}"
RESULT_CSV="${OUT_DIR}/benchmark_results.csv"
SUMMARY_CSV="${OUT_DIR}/benchmark_summary.csv"
COMPARE_CSV="${OUT_DIR}/benchmark_compare.csv"
PLAN_FILE="${OUT_DIR}/run_plan.csv"

INODES="${INODES:-4096}"
IMAGE_SIZE="${IMAGE_SIZE:-4G}"
MOUNT_TIMEOUT_SEC="${MOUNT_TIMEOUT_SEC:-15}"
REPEAT="${REPEAT:-5}"
GLOBAL_WARMUP="${GLOBAL_WARMUP:-1}"
INCLUDE_BUFFERED="${INCLUDE_BUFFERED:-1}"
INCLUDE_KERNEL_COMPARE="${INCLUDE_KERNEL_COMPARE:-1}"
SKIP_BUILD="${SKIP_BUILD:-0}"

SEQ_CASES=(
    "1K:16384"
    "4K:16384"
    "64K:16384"
    "256K:4096"
    "1M:1024"
    "16M:64"
    "256M:4"
)

RAND_CASES=(
    "4K:1024"
    "64K:512"
    "1M:64"
)

SMALLFILE_CASES=(
    "1K:1024"
    "4K:512"
    "16K:256"
)

if [[ -n "${SEQ_CASES_CSV:-}" ]]; then
    IFS=',' read -r -a SEQ_CASES <<< "${SEQ_CASES_CSV}"
elif [[ -n "${CASES_CSV:-}" ]]; then
    # Backward compatibility with previous CASES_CSV.
    IFS=',' read -r -a SEQ_CASES <<< "${CASES_CSV}"
fi

if [[ -n "${RAND_CASES_CSV:-}" ]]; then
    IFS=',' read -r -a RAND_CASES <<< "${RAND_CASES_CSV}"
fi

if [[ -n "${SMALLFILE_CASES_CSV:-}" ]]; then
    IFS=',' read -r -a SMALLFILE_CASES <<< "${SMALLFILE_CASES_CSV}"
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

WORKLOADS=("seq_write" "seq_read" "rand_write" "rand_read" "smallfile_create")
if [[ -n "${WORKLOADS_CSV:-}" ]]; then
    IFS=',' read -r -a WORKLOADS <<< "${WORKLOADS_CSV}"
fi

CURRENT_FUSE_PID=""
CURRENT_BACKEND=""

build_project() {
    if [[ "${SKIP_BUILD}" == "1" ]]; then
        return
    fi
    echo "==> Building Release target..."
    if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
        sudo -u "${SUDO_USER}" cmake --build "${BUILD_DIR}" --target all -j --config Release
    else
        cmake --build "${BUILD_DIR}" --target all -j --config Release
    fi
}

check_kernel_minix_support() {
    if ! grep -qw minix /proc/filesystems; then
        if command -v modprobe >/dev/null 2>&1; then
            modprobe minix >/dev/null 2>&1 || true
        fi
    fi
    if ! grep -qw minix /proc/filesystems; then
        echo "Kernel does not report minix filesystem support (/proc/filesystems)." >&2
        exit 1
    fi
}

ensure_device_not_mounted_elsewhere() {
    local target
    while IFS= read -r target; do
        if [[ -z "${target}" ]]; then
            continue
        fi
        if [[ "${target}" != "${MNT_DIR}" ]]; then
            echo "Target is already mounted at: ${target}" >&2
            echo "Refuse to run destructive benchmark on mounted target ${TARGET_PATH}." >&2
            exit 1
        fi
    done < <(findmnt -rn -S "${TARGET_PATH}" -o TARGET || true)
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
            umount "${MNT_DIR}" >/dev/null 2>&1
        fi
    fi

    if [[ -n "${CURRENT_FUSE_PID}" ]] && kill -0 "${CURRENT_FUSE_PID}" >/dev/null 2>&1; then
        kill "${CURRENT_FUSE_PID}" >/dev/null 2>&1
        wait "${CURRENT_FUSE_PID}" 2>/dev/null
    fi

    CURRENT_FUSE_PID=""
    CURRENT_BACKEND=""
    set -e
}

cleanup_on_exit() {
    local rc=$?
    cleanup_current_sample
    if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
        chown -R "${SUDO_USER}:${SUDO_USER}" "${OUT_DIR}" >/dev/null 2>&1 || true
    fi
    return "${rc}"
}
trap cleanup_on_exit EXIT INT TERM

format_device() {
    if [[ "${TARGET_KIND}" == "image" ]]; then
        mkdir -p "$(dirname "${TARGET_PATH}")"
        # Keep the image size stable across runs for fair comparison.
        truncate -s "${IMAGE_SIZE}" "${TARGET_PATH}"
    fi
    mkfs.minix -3 "${TARGET_PATH}" --inodes "${INODES}" >/dev/null
}

mount_backend() {
    local backend="$1"
    local sample_id="$2"

    CURRENT_BACKEND="${backend}"
    if [[ "${backend}" == "fuse" ]]; then
        "${BUILD_DIR}/minixfs-fuse" --device="${TARGET_PATH}" "${MNT_DIR}" -f -o auto_unmount &
        CURRENT_FUSE_PID=$!
        if ! wait_for_mount "${CURRENT_FUSE_PID}"; then
            echo "FUSE mount failed for sample ${sample_id}" >&2
            return 1
        fi
    elif [[ "${backend}" == "kernel" ]]; then
        if [[ "${TARGET_KIND}" == "image" ]]; then
            mount -t minix -o loop "${TARGET_PATH}" "${MNT_DIR}"
        else
            mount -t minix "${TARGET_PATH}" "${MNT_DIR}"
        fi
        if ! wait_for_mount ""; then
            echo "Kernel mount failed for sample ${sample_id}" >&2
            return 1
        fi
    else
        echo "Unknown backend: ${backend}" >&2
        return 1
    fi
}

run_sample() {
    local backend="$1"
    local workload="$2"
    local mode="$3"
    local bs="$4"
    local count="$5"
    local sample_id="$6"
    local csv_path="${7:-}"
    local workload_script_path

    cleanup_current_sample
    ensure_device_not_mounted_elsewhere
    format_device
    mount_backend "${backend}" "${sample_id}"

    workload_script_path="$(workload_script "${workload}")"
    bash "${workload_script_path}" "${MNT_DIR}" "${backend}" "${mode}" "${bs}" "${count}" "${sample_id}" "${csv_path}"
    cleanup_current_sample
}

prepare_backend_environment() {
    local backend="$1"
    if [[ "${backend}" == "kernel" ]]; then
        check_kernel_minix_support
    fi
}

workload_script() {
    local workload="$1"
    case "${workload}" in
        seq_write)
            echo "${ROOT_DIR}/benchmarks/dd.sh"
            ;;
        seq_read)
            echo "${ROOT_DIR}/benchmarks/dd_seq_read.sh"
            ;;
        rand_write)
            echo "${ROOT_DIR}/benchmarks/dd_rand_write.sh"
            ;;
        rand_read)
            echo "${ROOT_DIR}/benchmarks/dd_rand_read.sh"
            ;;
        smallfile_create)
            echo "${ROOT_DIR}/benchmarks/smallfile_create.sh"
            ;;
        *)
            echo "Unknown workload: ${workload}" >&2
            return 1
            ;;
    esac
}

workload_cases() {
    local workload="$1"
    case "${workload}" in
        seq_write | seq_read)
            printf "%s\n" "${SEQ_CASES[@]}"
            ;;
        rand_write | rand_read)
            printf "%s\n" "${RAND_CASES[@]}"
            ;;
        smallfile_create)
            printf "%s\n" "${SMALLFILE_CASES[@]}"
            ;;
        *)
            echo "Unknown workload for cases: ${workload}" >&2
            return 1
            ;;
    esac
}

workload_modes() {
    local workload="$1"
    case "${workload}" in
        seq_read | rand_read)
            # Read workload does not have meaningful sync/fdatasync variants.
            printf "buffered\n"
            ;;
        seq_write | rand_write | smallfile_create)
            printf "%s\n" "${MODES[@]}"
            ;;
        *)
            echo "Unknown workload for modes: ${workload}" >&2
            return 1
            ;;
    esac
}

validate_workloads() {
    local workload
    local script_path
    for workload in "${WORKLOADS[@]}"; do
        script_path="$(workload_script "${workload}")"
        if [[ ! -f "${script_path}" ]]; then
            echo "Workload script not found: ${script_path}" >&2
            exit 1
        fi
    done
}

build_project

mkdir -p "${MNT_DIR}" "${OUT_DIR}"
cleanup_current_sample
rm -f "${RESULT_CSV}" "${SUMMARY_CSV}" "${COMPARE_CSV}" "${PLAN_FILE}"

validate_workloads

for backend in "${BACKENDS[@]}"; do
    prepare_backend_environment "${backend}"
done

if [[ "${GLOBAL_WARMUP}" == "1" ]]; then
    echo "==> Running warmup sample..."
    for backend in "${BACKENDS[@]}"; do
        run_sample "${backend}" "seq_write" "fdatasync" "1M" "256" "warmup_${backend}" ""
    done
fi

echo "==> Creating randomized run plan..."
tmp_plan="${PLAN_FILE}.tmp"
: > "${tmp_plan}"
for backend in "${BACKENDS[@]}"; do
    for workload in "${WORKLOADS[@]}"; do
        mapfile -t workload_mode_list < <(workload_modes "${workload}")
        mapfile -t workload_case_list < <(workload_cases "${workload}")
        for mode in "${workload_mode_list[@]}"; do
            for entry in "${workload_case_list[@]}"; do
                bs="${entry%%:*}"
                count="${entry##*:}"
                for ((rep = 1; rep <= REPEAT; rep++)); do
                    printf "%s,%s,%s,%s,%s,%s\n" "${backend}" "${workload}" "${mode}" "${bs}" "${count}" "${rep}" >> "${tmp_plan}"
                done
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
while IFS=, read -r backend workload mode bs count rep; do
    current_run=$((current_run + 1))
    sample_id=$(printf "s%04d_r%s" "${current_run}" "${rep}")
    echo "[${current_run}/${total_runs}] backend=${backend} workload=${workload} mode=${mode} bs=${bs} repeat=${rep}"
    run_sample "${backend}" "${workload}" "${mode}" "${bs}" "${count}" "${sample_id}" "${RESULT_CSV}"
done < "${PLAN_FILE}"

echo "==> Aggregating summary..."
{
    echo "workload,backend,mode,bs,samples,median_mib_per_s,avg_mib_per_s,min_mib_per_s,max_mib_per_s,p90_mib_per_s,avg_ops_per_s"
    tail -n +2 "${RESULT_CSV}" | sort -t',' -k2,2 -k3,3 -k4,4 -k5,5 -k11,11n | awk -F',' '
        function flush_group(    median, p90idx, p90, avg, avgOps) {
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
            avgOps = opsSum / count
            printf "%s,%s,%s,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", curWorkload, curBackend, curMode, curBs, count, median, avg, minv, maxv, p90, avgOps
            delete speed
            delete ops
            count = 0
            sum = 0
            opsSum = 0
            minv = 0
            maxv = 0
        }
        {
            workload = $2
            backend = $3
            mode = $4
            bs = $5
            s = $11 + 0
            o = $12 + 0
            if (count == 0) {
                curWorkload = workload
                curBackend = backend
                curMode = mode
                curBs = bs
                minv = s
                maxv = s
            } else if (workload != curWorkload || backend != curBackend || mode != curMode || bs != curBs) {
                flush_group()
                curWorkload = workload
                curBackend = backend
                curMode = mode
                curBs = bs
                minv = s
                maxv = s
            }
            count += 1
            speed[count] = s
            ops[count] = o
            sum += s
            opsSum += o
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
    echo "workload,mode,bs,fuse_median_mib_per_s,kernel_median_mib_per_s,fuse_to_kernel_ratio,fuse_avg_ops_per_s,kernel_avg_ops_per_s,fuse_to_kernel_ops_ratio"
    awk -F',' '
        NR == 1 {
            next
        }
        {
            key = $1 "," $3 "," $4
            if ($2 == "fuse") {
                fuseSpeed[key] = $6 + 0
                fuseOps[key] = $11 + 0
            } else if ($2 == "kernel") {
                kernelSpeed[key] = $6 + 0
                kernelOps[key] = $11 + 0
            }
            keys[key] = 1
        }
        END {
            for (k in keys) {
                split(k, p, ",")
                fm = (k in fuseSpeed) ? fuseSpeed[k] : 0
                km = (k in kernelSpeed) ? kernelSpeed[k] : 0
                fo = (k in fuseOps) ? fuseOps[k] : 0
                ko = (k in kernelOps) ? kernelOps[k] : 0
                ratio = ""
                opsRatio = ""
                if (fm > 0 && km > 0) {
                    ratio = sprintf("%.4f", fm / km)
                }
                if (fo > 0 && ko > 0) {
                    opsRatio = sprintf("%.4f", fo / ko)
                }
                if (ratio == "") {
                    ratioOut = ""
                } else {
                    ratioOut = ratio
                }
                if (opsRatio == "") {
                    opsRatioOut = ""
                } else {
                    opsRatioOut = opsRatio
                }
                printf "%s,%s,%s,%.2f,%.2f,%s,%.2f,%.2f,%s\n", p[1], p[2], p[3], fm, km, ratioOut, fo, ko, opsRatioOut
            }
        }
    ' "${SUMMARY_CSV}" | sort -t',' -k1,1 -k2,2 -k3,3
} > "${COMPARE_CSV}"

echo "==> Benchmark finished."
echo "    Target: ${TARGET_PATH} (${TARGET_KIND})"
echo "    Workloads: ${WORKLOADS[*]}"
echo "    Results: ${RESULT_CSV}"
echo "    Summary: ${SUMMARY_CSV}"
echo "    Compare: ${COMPARE_CSV}"
echo "    Plan: ${PLAN_FILE}"
