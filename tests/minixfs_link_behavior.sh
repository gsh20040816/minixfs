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
require_cmd stat
require_cmd rm
require_cmd cp
require_cmd ln
require_cmd cat
require_cmd grep
require_cmd mkdir
require_cmd sync

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

SRC_FILE="${FUSE_MNT}/hello.txt"
HARD_LINK="${FUSE_MNT}/hello.hard"
EXISTING_TARGET="${FUSE_MNT}/existing_target.txt"
MISSING_SOURCE="${FUSE_MNT}/missing_source.txt"
DIR_SOURCE="${FUSE_MNT}/dir_a"
DIR_LINK="${FUSE_MNT}/dir_a.hard"

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

mount_fuse() {
    "${FUSE_BIN}" -f --device="${IMG_RUN}" "${FUSE_MNT}" >"${FUSE_LOG}" 2>&1 &
    FUSE_PID=$!
    for _ in $(seq 1 50); do
        if mountpoint -q "${FUSE_MNT}"; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

remount_fuse() {
    fusermount3 -u -z "${FUSE_MNT}"
    wait "${FUSE_PID}" >/dev/null 2>&1 || true
    FUSE_PID=""
    if ! mount_fuse; then
        echo "FAIL: fuse remount did not come up; log:" >&2
        sed -n '1,120p' "${FUSE_LOG}" >&2 || true
        exit 1
    fi
}

rm -rf "${WORK_DIR}"
mkdir -p "${FUSE_MNT}"
cp "${IMG_SRC}" "${IMG_RUN}"

if ! mount_fuse; then
    echo "FAIL: fuse mount did not come up; log:" >&2
    sed -n '1,120p' "${FUSE_LOG}" >&2 || true
    exit 1
fi

ORIG_CONTENT="$(cat "${SRC_FILE}")"

ln "${SRC_FILE}" "${HARD_LINK}"

if [[ ! -f "${HARD_LINK}" ]]; then
    echo "FAIL: hard link path was not created" >&2
    exit 1
fi
if [[ "$(cat "${HARD_LINK}")" != "${ORIG_CONTENT}" ]]; then
    echo "FAIL: hard link initial content mismatch with source" >&2
    exit 1
fi

printf "LINK-WRITE\n" >> "${HARD_LINK}"
if ! grep -q "LINK-WRITE" "${SRC_FILE}"; then
    echo "FAIL: write via hard link did not reflect on original file" >&2
    exit 1
fi

rm "${SRC_FILE}"
if [[ -e "${SRC_FILE}" ]]; then
    echo "FAIL: original path should be removed after unlink" >&2
    exit 1
fi
if [[ ! -f "${HARD_LINK}" ]]; then
    echo "FAIL: hard link path should remain after unlinking original" >&2
    exit 1
fi
if ! grep -q "LINK-WRITE" "${HARD_LINK}"; then
    echo "FAIL: hard link content lost after unlinking original" >&2
    exit 1
fi

printf "EXISTING\n" > "${EXISTING_TARGET}"
if ln "${HARD_LINK}" "${EXISTING_TARGET}" 2>/dev/null; then
    echo "FAIL: link should fail when destination exists" >&2
    exit 1
fi

if ln "${MISSING_SOURCE}" "${FUSE_MNT}/missing_dest.txt" 2>/dev/null; then
    echo "FAIL: link should fail when source does not exist" >&2
    exit 1
fi

if ln "${DIR_SOURCE}" "${DIR_LINK}" 2>/dev/null; then
    echo "FAIL: linking a directory should fail" >&2
    exit 1
fi
if [[ -e "${DIR_LINK}" ]]; then
    echo "FAIL: destination should not appear when linking directory fails" >&2
    exit 1
fi

sync
remount_fuse

if [[ ! -f "${HARD_LINK}" ]]; then
    echo "FAIL: hard link path missing after remount" >&2
    exit 1
fi
if ! grep -q "LINK-WRITE" "${HARD_LINK}"; then
    echo "FAIL: hard link content mismatch after remount" >&2
    exit 1
fi
if [[ -e "${SRC_FILE}" ]]; then
    echo "FAIL: original path unexpectedly reappeared after remount" >&2
    exit 1
fi

echo "PASS: hard link behavior is correct"
