#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
export LANG=C

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 <minixfs-fuse-bin> <fixture-dir> <work-dir>" >&2
    exit 2
fi

FUSE_BIN="$1"
FIXTURE_DIR="$2"
WORK_DIR="$3"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "SKIP: missing command '$1'" >&2
        exit 77
    fi
}

require_cmd mountpoint
require_cmd fusermount3
require_cmd truncate
require_cmd stat
require_cmd dd

if [[ ! -x "${FUSE_BIN}" ]]; then
    echo "FAIL: fuse binary not executable: ${FUSE_BIN}" >&2
    exit 1
fi

if [[ ! -r "${FIXTURE_DIR}/disk.img" ]]; then
    echo "SKIP: fixture not found, run tests/generate_minixfs_write_fixture.sh first" >&2
    exit 77
fi

IMG_SRC="${FIXTURE_DIR}/disk.img"
IMG_RUN="${WORK_DIR}/disk.img"
FUSE_MNT="${WORK_DIR}/fuse_mnt"
FUSE_LOG="${WORK_DIR}/fuse.log"

FUSE_PID=""
cleanup() {
    set +e
    if mountpoint -q "${FUSE_MNT}"; then
        fusermount3 -u -z "${FUSE_MNT}" >/dev/null 2>&1 || true
    fi
    if [[ -n "${FUSE_PID}" ]]; then
        kill "${FUSE_PID}" >/dev/null 2>&1 || true
        wait "${FUSE_PID}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

rm -rf "${WORK_DIR}"
mkdir -p "${FUSE_MNT}"
cp "${IMG_SRC}" "${IMG_RUN}"

"${FUSE_BIN}" -f --device="${IMG_RUN}" "${FUSE_MNT}" >"${FUSE_LOG}" 2>&1 &
FUSE_PID=$!
for _ in $(seq 1 50); do
    if mountpoint -q "${FUSE_MNT}"; then
        break
    fi
    sleep 0.1
done
if ! mountpoint -q "${FUSE_MNT}"; then
    echo "FAIL: fuse mount did not come up; log:" >&2
    sed -n '1,120p' "${FUSE_LOG}" >&2 || true
    exit 1
fi

TARGET="${FUSE_MNT}/truncate_case.txt"

printf "ABCDEFGHIJ" > "${TARGET}"
truncate -s 4 "${TARGET}"
size="$(stat -c '%s' "${TARGET}")"
if [[ "${size}" != "4" ]]; then
    echo "FAIL: truncate shrink failed, size=${size}" >&2
    exit 1
fi
content="$(cat "${TARGET}")"
if [[ "${content}" != "ABCD" ]]; then
    echo "FAIL: truncate shrink content mismatch: ${content}" >&2
    exit 1
fi

truncate -s 8192 "${TARGET}"
size="$(stat -c '%s' "${TARGET}")"
if [[ "${size}" != "8192" ]]; then
    echo "FAIL: truncate extend failed, size=${size}" >&2
    exit 1
fi
content="$(dd if="${TARGET}" bs=1 count=4 status=none)"
if [[ "${content}" != "ABCD" ]]; then
    echo "FAIL: truncate extend prefix mismatch: ${content}" >&2
    exit 1
fi

truncate -s 0 "${TARGET}"
size="$(stat -c '%s' "${TARGET}")"
if [[ "${size}" != "0" ]]; then
    echo "FAIL: truncate zero failed, size=${size}" >&2
    exit 1
fi

printf "LONG-CONTENT" > "${TARGET}"
printf "X" > "${TARGET}"
size="$(stat -c '%s' "${TARGET}")"
if [[ "${size}" != "1" ]]; then
    echo "FAIL: open(O_TRUNC) path failed, size=${size}" >&2
    exit 1
fi
content="$(cat "${TARGET}")"
if [[ "${content}" != "X" ]]; then
    echo "FAIL: open(O_TRUNC) content mismatch: ${content}" >&2
    exit 1
fi

echo "PASS: truncate behavior is correct"
