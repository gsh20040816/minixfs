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
require_cmd ln
require_cmd readlink
require_cmd rm
require_cmd cat
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
REAL_TARGET="${FUSE_MNT}/hello.txt"
NEW_LINK="${FUSE_MNT}/created_new.link"
DANGLING_LINK="${FUSE_MNT}/created_dangling.link"
MISSING_PARENT_LINK="${FUSE_MNT}/missing_parent/new.link"

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

if [[ ! -f "${REAL_TARGET}" ]]; then
    echo "FAIL: missing target file ${REAL_TARGET}" >&2
    exit 1
fi

REAL_TARGET_CONTENT="$(cat "${REAL_TARGET}")"
FFREE_BEFORE_CREATE="$(stat -f -c '%d' "${FUSE_MNT}")"

ln -s "hello.txt" "${NEW_LINK}"
if [[ ! -L "${NEW_LINK}" ]]; then
    echo "FAIL: symlink was not created for existing target" >&2
    exit 1
fi
if [[ "$(readlink "${NEW_LINK}")" != "hello.txt" ]]; then
    echo "FAIL: symlink target mismatch for ${NEW_LINK}" >&2
    exit 1
fi
if [[ "$(cat "${NEW_LINK}")" != "${REAL_TARGET_CONTENT}" ]]; then
    echo "FAIL: reading symlink to existing file returned unexpected content" >&2
    exit 1
fi

ln -s "not_exists_created_by_test.txt" "${DANGLING_LINK}"
if [[ ! -L "${DANGLING_LINK}" ]]; then
    echo "FAIL: dangling symlink was not created" >&2
    exit 1
fi
if [[ "$(readlink "${DANGLING_LINK}")" != "not_exists_created_by_test.txt" ]]; then
    echo "FAIL: dangling symlink target mismatch" >&2
    exit 1
fi

FFREE_AFTER_CREATE="$(stat -f -c '%d' "${FUSE_MNT}")"
if (( FFREE_AFTER_CREATE != FFREE_BEFORE_CREATE - 2 )); then
    echo "FAIL: creating two symlinks should consume two inodes: before=${FFREE_BEFORE_CREATE}, after=${FFREE_AFTER_CREATE}" >&2
    exit 1
fi

if ln -s "hello.txt" "${NEW_LINK}" 2>/dev/null; then
    echo "FAIL: creating symlink over existing path should fail" >&2
    exit 1
fi
if [[ "$(readlink "${NEW_LINK}")" != "hello.txt" ]]; then
    echo "FAIL: existing symlink target changed after failed duplicate create" >&2
    exit 1
fi

if ln -s "hello.txt" "${MISSING_PARENT_LINK}" 2>/dev/null; then
    echo "FAIL: creating symlink with missing parent should fail" >&2
    exit 1
fi
if [[ -e "${FUSE_MNT}/missing_parent" || -L "${FUSE_MNT}/missing_parent" ]]; then
    echo "FAIL: missing parent path should not appear after failed symlink create" >&2
    exit 1
fi

FFREE_AFTER_FAILED_CREATE="$(stat -f -c '%d' "${FUSE_MNT}")"
if [[ "${FFREE_AFTER_FAILED_CREATE}" != "${FFREE_AFTER_CREATE}" ]]; then
    echo "FAIL: failed symlink creates should not change free inode count: after_create=${FFREE_AFTER_CREATE}, after_fail=${FFREE_AFTER_FAILED_CREATE}" >&2
    exit 1
fi

rm "${NEW_LINK}"
if [[ -e "${NEW_LINK}" || -L "${NEW_LINK}" ]]; then
    echo "FAIL: unlinking symlink should remove link path" >&2
    exit 1
fi
if [[ "$(cat "${REAL_TARGET}")" != "${REAL_TARGET_CONTENT}" ]]; then
    echo "FAIL: unlinking symlink changed real target content unexpectedly" >&2
    exit 1
fi

FFREE_AFTER_UNLINK="$(stat -f -c '%d' "${FUSE_MNT}")"
EXPECTED_FFREE_AFTER_UNLINK=$((FFREE_AFTER_CREATE + 1))
if [[ "${FFREE_AFTER_UNLINK}" != "${EXPECTED_FFREE_AFTER_UNLINK}" ]]; then
    echo "FAIL: unlinking one symlink should free one inode: after_create=${FFREE_AFTER_CREATE}, after_unlink=${FFREE_AFTER_UNLINK}" >&2
    exit 1
fi

sync
remount_fuse

if [[ -e "${NEW_LINK}" || -L "${NEW_LINK}" ]]; then
    echo "FAIL: deleted symlink should stay deleted after remount" >&2
    exit 1
fi
if [[ ! -L "${DANGLING_LINK}" ]]; then
    echo "FAIL: dangling symlink missing after remount" >&2
    exit 1
fi
if [[ "$(readlink "${DANGLING_LINK}")" != "not_exists_created_by_test.txt" ]]; then
    echo "FAIL: dangling symlink target mismatch after remount" >&2
    exit 1
fi
if [[ "$(cat "${REAL_TARGET}")" != "${REAL_TARGET_CONTENT}" ]]; then
    echo "FAIL: real target content mismatch after remount" >&2
    exit 1
fi

echo "PASS: symlink create behavior is correct"
