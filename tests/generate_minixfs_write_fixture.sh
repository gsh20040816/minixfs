#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
export LANG=C

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <fixture-dir>" >&2
    exit 2
fi

FIXTURE_ARG="$1"
if [[ "${FIXTURE_ARG}" = /* ]]; then
    FIXTURE_DIR="${FIXTURE_ARG}"
else
    FIXTURE_DIR="${PWD}/${FIXTURE_ARG}"
fi
IMG="${FIXTURE_DIR}/disk.img"
EXPECTED_DIR="${FIXTURE_DIR}/expected"
METADATA_FILE="${FIXTURE_DIR}/metadata.txt"
WORK_DIR="${FIXTURE_DIR}/.work"
SEED_DIR="${WORK_DIR}/seed"
KERNEL_MNT="${WORK_DIR}/kernel_mnt"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OPS_FILE="${SCRIPT_DIR}/write_existing_ops.sh"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "FAIL: missing command '$1'" >&2
        exit 1
    fi
}

emit_pattern_bytes() {
    local size="$1"
    local pattern="$2"
    awk -v n="${size}" -v pat="${pattern}" '
BEGIN {
    l = length(pat);
    for (i = 0; i < n; i++) {
        printf "%s", substr(pat, (i % l) + 1, 1);
    }
}'
}

require_cmd mkfs.minix
require_cmd mount
require_cmd umount
require_cmd mountpoint
require_cmd cp
require_cmd truncate
require_cmd chown
require_cmd dd
require_cmd awk
require_cmd losetup

if [[ ! -r "${OPS_FILE}" ]]; then
    echo "FAIL: missing ops script: ${OPS_FILE}" >&2
    exit 1
fi

SUDO=()
if [[ "${EUID}" -ne 0 ]]; then
    if command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
        SUDO=(sudo -n)
    else
        echo "FAIL: need root or passwordless sudo to mount minix image" >&2
        exit 1
    fi
fi

OWNER_UID="${SUDO_UID:-$(id -u)}"
OWNER_GID="${SUDO_GID:-$(id -g)}"
LOOP_DEV=""

ensure_loop_node() {
    local candidate
    candidate="$("${SUDO[@]}" losetup -f 2>/dev/null || true)"
    if [[ -z "${candidate}" || -e "${candidate}" ]]; then
        return 0
    fi
    local idx="${candidate#/dev/loop}"
    if [[ "${idx}" =~ ^[0-9]+$ ]]; then
        "${SUDO[@]}" mknod -m 660 "${candidate}" b 7 "${idx}" >/dev/null 2>&1 || true
        "${SUDO[@]}" chgrp disk "${candidate}" >/dev/null 2>&1 || true
    fi
}

cleanup() {
    set +e
    if mountpoint -q "${KERNEL_MNT}"; then
        "${SUDO[@]}" umount "${KERNEL_MNT}" >/dev/null 2>&1 || "${SUDO[@]}" umount -l "${KERNEL_MNT}" >/dev/null 2>&1
    fi
    if [[ -n "${LOOP_DEV}" ]]; then
        "${SUDO[@]}" losetup -d "${LOOP_DEV}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

rm -rf "${WORK_DIR}" "${EXPECTED_DIR}"
mkdir -p "${SEED_DIR}/dir_a/dir_b" "${KERNEL_MNT}" "${FIXTURE_DIR}"

truncate -s 64M "${IMG}"
mkfs.minix -3 "${IMG}" >/dev/null

printf "hello from minixfs\n" > "${SEED_DIR}/hello.txt"
printf "nested text\n" > "${SEED_DIR}/dir_a/dir_b/nested.txt"
: > "${SEED_DIR}/empty.txt"

emit_pattern_bytes $((3 * 1024 * 1024 + 321)) "LARGE-SEED-DATA-0123456789abcdef" > "${SEED_DIR}/large.bin"

printf "SPARSE-BASE\n" > "${SEED_DIR}/sparse.bin"
printf "SPARSE-END\n" | dd of="${SEED_DIR}/sparse.bin" bs=1 seek=$((2 * 1024 * 1024 + 1)) conv=notrunc status=none

ensure_loop_node
if ! LOOP_DEV="$("${SUDO[@]}" losetup -f --show "${IMG}" 2>&1)"; then
    echo "FAIL: cannot setup loop device for ${IMG}" >&2
    echo "${LOOP_DEV}" >&2
    echo "HINT: ensure loop module is loaded and /dev/loopN exists." >&2
    exit 1
fi
"${SUDO[@]}" mount -t minix "${LOOP_DEV}" "${KERNEL_MNT}"
"${SUDO[@]}" cp -a "${SEED_DIR}/." "${KERNEL_MNT}/"

# shellcheck disable=SC2016
"${SUDO[@]}" bash -c 'source "'"${OPS_FILE}"'"; apply_write_existing_ops "'"${KERNEL_MNT}"'"'
sync

make_manifest() {
    local root="$1"
    local out="$2"
    : > "${out}"
    while IFS= read -r rel; do
        local path="${root}/${rel}"
        local mode size type
        mode="$("${SUDO[@]}" stat -c '%a' "${path}")"
        size="$("${SUDO[@]}" stat -c '%s' "${path}")"
        type="$("${SUDO[@]}" stat -c '%F' "${path}")"
        printf '%s|%s|%s|%s\n' "${rel}" "${type}" "${mode}" "${size}" >> "${out}"
    done < <("${SUDO[@]}" bash -c "cd \"${root}\" && find . -mindepth 1 -printf '%P\n' | sort")
}

make_manifest "${KERNEL_MNT}" "${METADATA_FILE}"
mkdir -p "${EXPECTED_DIR}"
"${SUDO[@]}" cp -a "${KERNEL_MNT}/." "${EXPECTED_DIR}/"
"${SUDO[@]}" umount "${KERNEL_MNT}"
"${SUDO[@]}" losetup -d "${LOOP_DEV}"
LOOP_DEV=""

"${SUDO[@]}" chown -R "${OWNER_UID}:${OWNER_GID}" "${FIXTURE_DIR}"
rm -rf "${WORK_DIR}"
echo "Generated write fixture at: ${FIXTURE_DIR}"
