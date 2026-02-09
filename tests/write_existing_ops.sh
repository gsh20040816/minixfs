#!/usr/bin/env bash

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

apply_write_existing_ops() {
    local root="$1"

    # Partial overwrite in-place.
    printf "MUTATE" | dd of="${root}/hello.txt" bs=1 seek=6 conv=notrunc status=none

    # Cross-block overwrite in a large existing file.
    emit_pattern_bytes 7000 "WRITE-LARGE-PAYLOAD-0123456789abcdef" \
        | dd of="${root}/large.bin" bs=1 seek=$((1024 * 1024 + 17)) conv=notrunc status=none

    # Write beyond EOF on an existing file (hole + tail).
    printf "TAIL-END" | dd of="${root}/dir_a/dir_b/nested.txt" bs=1 seek=$((8192 + 5)) conv=notrunc status=none

    # In-place overwrite in sparse file.
    emit_pattern_bytes 4096 "SPARSE-OVERWRITE-BLOCK-abcdefghijklmnopqrstuvwxyz" \
        | dd of="${root}/sparse.bin" bs=1 seek=$((2 * 1024 * 1024 + 33)) conv=notrunc status=none
}
