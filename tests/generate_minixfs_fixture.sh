#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
export LANG=C

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <fixture-dir>" >&2
    exit 2
fi

FIXTURE_DIR="$1"
IMG="${FIXTURE_DIR}/disk.img"
EXPECTED_DIR="${FIXTURE_DIR}/expected"
METADATA_FILE="${FIXTURE_DIR}/metadata.txt"
WORK_DIR="${FIXTURE_DIR}/.work"
SEED_DIR="${WORK_DIR}/seed"
KERNEL_MNT="${WORK_DIR}/kernel_mnt"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "FAIL: missing command '$1'" >&2
        exit 1
    fi
}

require_cmd mkfs.minix
require_cmd mount
require_cmd umount
require_cmd mountpoint
require_cmd cp
require_cmd truncate
require_cmd python3
require_cmd chown

SUDO=()
if [[ "${EUID}" -ne 0 ]]; then
    if command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
        SUDO=(sudo -n)
    else
        echo "FAIL: need root or passwordless sudo to mount minix image" >&2
        exit 1
    fi
fi

# If launched via sudo, keep fixture ownership as the original user.
OWNER_UID="${SUDO_UID:-$(id -u)}"
OWNER_GID="${SUDO_GID:-$(id -g)}"

cleanup() {
    set +e
    if mountpoint -q "${KERNEL_MNT}"; then
        "${SUDO[@]}" umount "${KERNEL_MNT}" >/dev/null 2>&1 || "${SUDO[@]}" umount -l "${KERNEL_MNT}" >/dev/null 2>&1
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
python3 - <<'PY' > "${SEED_DIR}/blob.bin"
import sys
payload = bytes((i * 37) % 256 for i in range(4096))
sys.stdout.buffer.write(payload)
PY

"${SUDO[@]}" mount -t minix -o loop "${IMG}" "${KERNEL_MNT}"
"${SUDO[@]}" cp -a "${SEED_DIR}/." "${KERNEL_MNT}/"
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

"${SUDO[@]}" chown -R "${OWNER_UID}:${OWNER_GID}" "${FIXTURE_DIR}"

rm -rf "${WORK_DIR}"
echo "Generated fixture at: ${FIXTURE_DIR}"
