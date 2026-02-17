#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
RUN_DIR="${ROOT_DIR}/run"
MNT_DIR="${RUN_DIR}/mnt"
IMG_PATH="${RUN_DIR}/benchmark.img"
OUT_DIR="${RUN_DIR}/benchmarks"

IMAGE_SIZE="${IMAGE_SIZE:-16G}"
INODES="${INODES:-1024}"
MOUNT_TIMEOUT_SEC="${MOUNT_TIMEOUT_SEC:-15}"
KEEP_IMAGE="${KEEP_IMAGE:-0}"

FUSE_PID=""

cleanup() {
    local rc=$?
    set +e

    if mountpoint -q "${MNT_DIR}"; then
        if command -v fusermount3 >/dev/null 2>&1; then
            fusermount3 -u "${MNT_DIR}" >/dev/null 2>&1
        else
            umount "${MNT_DIR}" >/dev/null 2>&1
        fi
    fi

    if [[ -n "${FUSE_PID}" ]] && kill -0 "${FUSE_PID}" >/dev/null 2>&1; then
        kill "${FUSE_PID}" >/dev/null 2>&1
        wait "${FUSE_PID}" 2>/dev/null
    fi

    if [[ "${KEEP_IMAGE}" != "1" ]]; then
        rm -f "${IMG_PATH}"
    fi

    return "${rc}"
}
trap cleanup EXIT INT TERM

echo "==> Building Release target..."
cmake --build "${BUILD_DIR}" --target all -j --config Release

mkdir -p "${MNT_DIR}" "${OUT_DIR}"
rm -f "${IMG_PATH}"

echo "==> Preparing image (${IMAGE_SIZE})..."
truncate -s "${IMAGE_SIZE}" "${IMG_PATH}"
mkfs.minix -3 "${IMG_PATH}" --inodes "${INODES}" >/dev/null

echo "==> Starting minixfs-fuse in foreground mode..."
"${BUILD_DIR}/minixfs-fuse" --device="${IMG_PATH}" "${MNT_DIR}" -f -o auto_unmount &
FUSE_PID=$!
echo "    PID: ${FUSE_PID}"

echo "==> Waiting for mount to become ready..."
mounted=0
for ((i = 0; i < MOUNT_TIMEOUT_SEC * 10; i++)); do
    if ! kill -0 "${FUSE_PID}" >/dev/null 2>&1; then
        echo "FUSE process exited before mount was ready."
        exit 1
    fi
    if mountpoint -q "${MNT_DIR}"; then
        mounted=1
        break
    fi
    sleep 0.1
done

if [[ "${mounted}" -ne 1 ]]; then
    echo "Mount timeout: ${MOUNT_TIMEOUT_SEC}s"
    exit 1
fi

echo "==> Running dd benchmark..."
bash "${ROOT_DIR}/benchmarks/dd.sh" "${MNT_DIR}" "${OUT_DIR}"
echo "==> Benchmark finished. Results: ${OUT_DIR}/dd_results.csv"
