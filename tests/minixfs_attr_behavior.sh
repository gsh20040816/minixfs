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
require_cmd mkdir
require_cmd python3
require_cmd touch
require_cmd sync
require_cmd sleep

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
TARGET="${FUSE_MNT}/hello.txt"

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

if [[ ! -f "${TARGET}" ]]; then
    echo "FAIL: target file not found: ${TARGET}" >&2
    exit 1
fi

ORIG_UID="$(stat -c '%u' "${TARGET}")"
ORIG_GID="$(stat -c '%g' "${TARGET}")"

chmod 600 "${TARGET}"
MODE_AFTER_CHMOD="$(stat -c '%a' "${TARGET}")"
if [[ "${MODE_AFTER_CHMOD}" != "600" ]]; then
    echo "FAIL: chmod did not set expected mode: got=${MODE_AFTER_CHMOD}" >&2
    exit 1
fi

MINIXFS_ATTR_TARGET="${TARGET}" python3 - <<'PY'
import os

target = os.environ["MINIXFS_ATTR_TARGET"]
before = os.stat(target)
os.chown(target, -1, before.st_gid)
after = os.stat(target)

if after.st_uid != before.st_uid:
    raise SystemExit(
        f"FAIL: chown(-1, gid) changed uid: before={before.st_uid}, after={after.st_uid}"
    )
if after.st_gid != before.st_gid:
    raise SystemExit(
        f"FAIL: chown(-1, gid) changed gid unexpectedly: before={before.st_gid}, after={after.st_gid}"
    )
PY

MINIXFS_ATTR_TARGET="${TARGET}" python3 - <<'PY'
import os
import time

target = os.environ["MINIXFS_ATTR_TARGET"]
before = int(time.time())
os.utime(target, None)
after = int(time.time())
st = os.stat(target)
atime = int(st.st_atime)
mtime = int(st.st_mtime)

if atime < before - 1 or atime > after + 1:
    raise SystemExit(
        f"FAIL: utime(None) produced unexpected atime: atime={atime}, window=[{before}, {after}]"
    )
if mtime < before - 1 or mtime > after + 1:
    raise SystemExit(
        f"FAIL: utime(None) produced unexpected mtime: mtime={mtime}, window=[{before}, {after}]"
    )
PY

ATIME_BEFORE_TOUCH_A="$(stat -c '%X' "${TARGET}")"
MTIME_BEFORE_TOUCH_A="$(stat -c '%Y' "${TARGET}")"
sleep 1
touch -a "${TARGET}"
ATIME_AFTER_TOUCH_A="$(stat -c '%X' "${TARGET}")"
MTIME_AFTER_TOUCH_A="$(stat -c '%Y' "${TARGET}")"

if [[ "${MTIME_AFTER_TOUCH_A}" != "${MTIME_BEFORE_TOUCH_A}" ]]; then
    echo "FAIL: touch -a should not modify mtime: before=${MTIME_BEFORE_TOUCH_A}, after=${MTIME_AFTER_TOUCH_A}" >&2
    exit 1
fi
if (( ATIME_AFTER_TOUCH_A <= ATIME_BEFORE_TOUCH_A )); then
    echo "FAIL: touch -a should advance atime: before=${ATIME_BEFORE_TOUCH_A}, after=${ATIME_AFTER_TOUCH_A}" >&2
    exit 1
fi

ATIME_BEFORE_TOUCH_M="$(stat -c '%X' "${TARGET}")"
MTIME_BEFORE_TOUCH_M="$(stat -c '%Y' "${TARGET}")"
sleep 1
touch -m "${TARGET}"
ATIME_AFTER_TOUCH_M="$(stat -c '%X' "${TARGET}")"
MTIME_AFTER_TOUCH_M="$(stat -c '%Y' "${TARGET}")"

if [[ "${ATIME_AFTER_TOUCH_M}" != "${ATIME_BEFORE_TOUCH_M}" ]]; then
    echo "FAIL: touch -m should not modify atime: before=${ATIME_BEFORE_TOUCH_M}, after=${ATIME_AFTER_TOUCH_M}" >&2
    exit 1
fi
if (( MTIME_AFTER_TOUCH_M <= MTIME_BEFORE_TOUCH_M )); then
    echo "FAIL: touch -m should advance mtime: before=${MTIME_BEFORE_TOUCH_M}, after=${MTIME_AFTER_TOUCH_M}" >&2
    exit 1
fi

FINAL_UID="$(stat -c '%u' "${TARGET}")"
FINAL_GID="$(stat -c '%g' "${TARGET}")"
FINAL_MODE="$(stat -c '%a' "${TARGET}")"
FINAL_ATIME="$(stat -c '%X' "${TARGET}")"
FINAL_MTIME="$(stat -c '%Y' "${TARGET}")"

if [[ "${FINAL_UID}" != "${ORIG_UID}" ]]; then
    echo "FAIL: uid changed unexpectedly: original=${ORIG_UID}, final=${FINAL_UID}" >&2
    exit 1
fi
if [[ "${FINAL_GID}" != "${ORIG_GID}" ]]; then
    echo "FAIL: gid changed unexpectedly: original=${ORIG_GID}, final=${FINAL_GID}" >&2
    exit 1
fi
if [[ "${FINAL_MODE}" != "600" ]]; then
    echo "FAIL: final mode mismatch before remount: ${FINAL_MODE}" >&2
    exit 1
fi

sync
remount_fuse

REMOUNT_UID="$(stat -c '%u' "${TARGET}")"
REMOUNT_GID="$(stat -c '%g' "${TARGET}")"
REMOUNT_MODE="$(stat -c '%a' "${TARGET}")"
REMOUNT_ATIME="$(stat -c '%X' "${TARGET}")"
REMOUNT_MTIME="$(stat -c '%Y' "${TARGET}")"

if [[ "${REMOUNT_UID}" != "${FINAL_UID}" || "${REMOUNT_GID}" != "${FINAL_GID}" ]]; then
    echo "FAIL: uid/gid mismatch after remount: before=${FINAL_UID}:${FINAL_GID}, after=${REMOUNT_UID}:${REMOUNT_GID}" >&2
    exit 1
fi
if [[ "${REMOUNT_MODE}" != "${FINAL_MODE}" ]]; then
    echo "FAIL: mode mismatch after remount: before=${FINAL_MODE}, after=${REMOUNT_MODE}" >&2
    exit 1
fi
if [[ "${REMOUNT_ATIME}" != "${FINAL_ATIME}" || "${REMOUNT_MTIME}" != "${FINAL_MTIME}" ]]; then
    echo "FAIL: atime/mtime mismatch after remount: before=${FINAL_ATIME}:${FINAL_MTIME}, after=${REMOUNT_ATIME}:${REMOUNT_MTIME}" >&2
    exit 1
fi

echo "PASS: chmod/chown/utimens behavior is correct"
