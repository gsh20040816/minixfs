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
require_cmd diff
require_cmd stat

if [[ ! -x "$FUSE_BIN" ]]; then
    echo "FAIL: fuse binary not executable: $FUSE_BIN" >&2
    exit 1
fi

if [[ ! -r "${FIXTURE_DIR}/disk.img" || ! -d "${FIXTURE_DIR}/expected" || ! -r "${FIXTURE_DIR}/metadata.txt" ]]; then
    echo "SKIP: fixture not found, run tests/generate_minixfs_fixture.sh first" >&2
    exit 77
fi

IMG="${FIXTURE_DIR}/disk.img"
EXPECTED_DIR="${FIXTURE_DIR}/expected"
FIXTURE_METADATA="${FIXTURE_DIR}/metadata.txt"
FUSE_MNT="${WORK_DIR}/fuse_mnt"
FUSE_LOG="${WORK_DIR}/fuse.log"
META_FUSE="${WORK_DIR}/fuse.meta"

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

"${FUSE_BIN}" -f --device="${IMG}" "${FUSE_MNT}" >"${FUSE_LOG}" 2>&1 &
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

diff -ruN "${EXPECTED_DIR}" "${FUSE_MNT}"

make_manifest() {
    local root="$1"
    local out="$2"
    : > "${out}"
    while IFS= read -r rel; do
        local path="${root}/${rel}"
        local mode size type
        mode="$(stat -c '%a' "${path}")"
        size="$(stat -c '%s' "${path}")"
        type="$(stat -c '%F' "${path}")"
        printf '%s|%s|%s|%s\n' "${rel}" "${type}" "${mode}" "${size}" >> "${out}"
    done < <(cd "${root}" && find . -mindepth 1 -printf '%P\n' | sort)
}

make_manifest "${FUSE_MNT}" "${META_FUSE}"
diff -u "${FIXTURE_METADATA}" "${META_FUSE}"

echo "PASS: kernel minix and fuse views are consistent"
