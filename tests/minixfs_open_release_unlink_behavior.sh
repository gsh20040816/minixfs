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
require_cmd cat

if [[ ! -x "${FUSE_BIN}" ]]; then
    echo "FAIL: fuse binary not executable: ${FUSE_BIN}" >&2
    exit 1
fi

if [[ ! -r "${FIXTURE_DIR}/disk.img" ]]; then
    echo "SKIP: fixture not found, run tests/generate_minixfs_write_fixture.sh first" >&2
    exit 77
fi
if [[ ! -r "${FIXTURE_DIR}/expected/hello.txt" ]]; then
    echo "SKIP: fixture missing expected/hello.txt, run tests/generate_minixfs_write_fixture.sh first" >&2
    exit 77
fi

IMG_SRC="${FIXTURE_DIR}/disk.img"
IMG_RUN="${WORK_DIR}/disk.img"
FUSE_MNT="${WORK_DIR}/fuse_mnt"
FUSE_LOG="${WORK_DIR}/fuse.log"
HELLO_EXPECTED_CONTENT="$(cat "${FIXTURE_DIR}/expected/hello.txt")"

FUSE_PID=""
cleanup() {
    set +e
    if [[ -d "${FUSE_MNT}" ]]; then
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

if [[ -d "${FUSE_MNT}" ]]; then
    fusermount3 -u -z "${FUSE_MNT}" >/dev/null 2>&1 || true
fi
rm -rf "${WORK_DIR}"
mkdir -p "${FUSE_MNT}"
cp "${IMG_SRC}" "${IMG_RUN}"

if ! mount_fuse; then
    echo "FAIL: fuse mount did not come up; log:" >&2
    sed -n '1,120p' "${FUSE_LOG}" >&2 || true
    exit 1
fi

UNLINK_TARGET="${FUSE_MNT}/unlink_regular_case.txt"
FFREE_BEFORE_UNLINK="$(stat -f -c '%d' "${FUSE_MNT}")"
printf "UNLINK-ME" > "${UNLINK_TARGET}"
rm "${UNLINK_TARGET}"
if [[ -e "${UNLINK_TARGET}" ]]; then
    echo "FAIL: regular file unlink should remove entry" >&2
    exit 1
fi
sync
FFREE_AFTER_UNLINK="$(stat -f -c '%d' "${FUSE_MNT}")"
if [[ "${FFREE_AFTER_UNLINK}" != "${FFREE_BEFORE_UNLINK}" ]]; then
    echo "FAIL: create+unlink should not leak free inodes: before=${FFREE_BEFORE_UNLINK}, after=${FFREE_AFTER_UNLINK}" >&2
    exit 1
fi

DIR_TARGET="${FUSE_MNT}/dir_a"
if rm "${DIR_TARGET}" 2>/dev/null; then
    echo "FAIL: unlink directory should fail with EISDIR" >&2
    exit 1
fi
if [[ ! -d "${DIR_TARGET}" ]]; then
    echo "FAIL: directory should remain after failed unlink" >&2
    exit 1
fi

REAL_TARGET="${FUSE_MNT}/hello.txt"
LINK_TARGET="${FUSE_MNT}/hello.link"
rm "${LINK_TARGET}"
if [[ -e "${LINK_TARGET}" || -L "${LINK_TARGET}" ]]; then
    echo "FAIL: symlink unlink should remove link entry" >&2
    exit 1
fi
if [[ ! -f "${REAL_TARGET}" ]]; then
    echo "FAIL: symlink target should stay after unlinking link" >&2
    exit 1
fi
if [[ "$(cat "${REAL_TARGET}")" != "${HELLO_EXPECTED_CONTENT}" ]]; then
    echo "FAIL: symlink target content changed unexpectedly" >&2
    exit 1
fi

MISSING_PARENT_TARGET="${FUSE_MNT}/not_exists_parent/src_dir"
if mkdir "${MISSING_PARENT_TARGET}" 2>/dev/null; then
    echo "FAIL: mkdir should fail when parent directory does not exist" >&2
    exit 1
fi
if [[ -e "${FUSE_MNT}/not_exists_parent" ]]; then
    echo "FAIL: mkdir with missing parent should not create intermediate directory" >&2
    exit 1
fi

TRUNC_TARGET="${FUSE_MNT}/open_trunc_case.txt"
printf "ABCDEFG" > "${TRUNC_TARGET}"
exec 3>"${TRUNC_TARGET}"
if [[ "$(stat -c '%s' "${TRUNC_TARGET}")" != "0" ]]; then
    echo "FAIL: open with O_TRUNC should make file size zero immediately" >&2
    exit 1
fi
printf "Z" >&3
exec 3>&-
if [[ "$(cat "${TRUNC_TARGET}")" != "Z" ]]; then
    echo "FAIL: open(O_TRUNC) write content mismatch" >&2
    exit 1
fi

sync
remount_fuse

if [[ -e "${UNLINK_TARGET}" || -e "${LINK_TARGET}" ]]; then
    echo "FAIL: deleted entries should stay deleted after remount" >&2
    exit 1
fi
if [[ ! -d "${DIR_TARGET}" ]]; then
    echo "FAIL: directory should persist after remount" >&2
    exit 1
fi
if [[ "$(cat "${TRUNC_TARGET}")" != "Z" ]]; then
    echo "FAIL: truncated file content mismatch after remount" >&2
    exit 1
fi
if [[ "$(cat "${REAL_TARGET}")" != "${HELLO_EXPECTED_CONTENT}" ]]; then
    echo "FAIL: symlink target content mismatch after remount" >&2
    exit 1
fi

echo "PASS: open/release/unlink behavior is correct"
