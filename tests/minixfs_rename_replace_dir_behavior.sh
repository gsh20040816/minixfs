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
require_cmd cp
require_cmd mkdir
require_cmd python3

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
SRC_DIR="${FUSE_MNT}/rename_src_dir"
DST_DIR="${FUSE_MNT}/rename_dst_dir"

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

rm -rf "${WORK_DIR}"
mkdir -p "${FUSE_MNT}"
cp "${IMG_SRC}" "${IMG_RUN}"

if ! mount_fuse; then
    echo "FAIL: fuse mount did not come up; log:" >&2
    sed -n '1,120p' "${FUSE_LOG}" >&2 || true
    exit 1
fi

FFREE_BEFORE="$(stat -f -c '%d' "${FUSE_MNT}")"
mkdir "${SRC_DIR}"
mkdir "${DST_DIR}"
SRC_INO_BEFORE="$(stat -c '%i' "${SRC_DIR}")"
DST_INO_BEFORE="$(stat -c '%i' "${DST_DIR}")"
FFREE_AFTER_CREATE="$(stat -f -c '%d' "${FUSE_MNT}")"
if (( FFREE_AFTER_CREATE != FFREE_BEFORE - 2 )); then
    echo "FAIL: creating two directories should consume two inodes: before=${FFREE_BEFORE}, after_create=${FFREE_AFTER_CREATE}" >&2
    exit 1
fi

MINIXFS_RENAME_MNT="${FUSE_MNT}" python3 - <<'PY'
import os
mnt = os.environ["MINIXFS_RENAME_MNT"]
os.rename(f"{mnt}/rename_src_dir", f"{mnt}/rename_dst_dir")
PY

if [[ -e "${SRC_DIR}" ]]; then
    echo "FAIL: source directory should disappear after rename replace" >&2
    exit 1
fi
if [[ ! -d "${DST_DIR}" ]]; then
    echo "FAIL: destination directory should exist after rename replace" >&2
    exit 1
fi

DST_INO_AFTER="$(stat -c '%i' "${DST_DIR}")"
if [[ "${DST_INO_AFTER}" != "${SRC_INO_BEFORE}" ]]; then
    echo "FAIL: destination inode should become source inode after rename replace: src_ino=${SRC_INO_BEFORE}, dst_ino_after=${DST_INO_AFTER}" >&2
    exit 1
fi
if [[ "${DST_INO_AFTER}" == "${DST_INO_BEFORE}" ]]; then
    echo "FAIL: destination inode should not remain old inode after replace" >&2
    exit 1
fi

FFREE_AFTER_RENAME="$(stat -f -c '%d' "${FUSE_MNT}")"
EXPECTED_AFTER_RENAME=$((FFREE_AFTER_CREATE + 1))
if [[ "${FFREE_AFTER_RENAME}" != "${EXPECTED_AFTER_RENAME}" ]]; then
    echo "FAIL: rename replace should free one directory inode: after_create=${FFREE_AFTER_CREATE}, after_rename=${FFREE_AFTER_RENAME}" >&2
    exit 1
fi

sync
fusermount3 -u -z "${FUSE_MNT}"
wait "${FUSE_PID}" >/dev/null 2>&1 || true
FUSE_PID=""

if ! mount_fuse; then
    echo "FAIL: fuse remount did not come up; log:" >&2
    sed -n '1,120p' "${FUSE_LOG}" >&2 || true
    exit 1
fi

if [[ -e "${SRC_DIR}" || ! -d "${DST_DIR}" ]]; then
    echo "FAIL: rename replace result is not persistent after remount" >&2
    exit 1
fi

FFREE_AFTER_REMOUNT="$(stat -f -c '%d' "${FUSE_MNT}")"
if [[ "${FFREE_AFTER_REMOUNT}" != "${FFREE_AFTER_RENAME}" ]]; then
    echo "FAIL: ffree changed unexpectedly after remount: after_rename=${FFREE_AFTER_RENAME}, after_remount=${FFREE_AFTER_REMOUNT}" >&2
    exit 1
fi

echo "PASS: rename(dir -> existing empty dir) behavior is correct"
