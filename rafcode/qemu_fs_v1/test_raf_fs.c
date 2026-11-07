/* SPDX-License-Identifier: MIT */
#include "raf_fs.h"

/* Independent branch-and-byte oracle, without headers or CRT I/O. */
static int equal(const raf_u8 *a, const raf_u8 *b, raf_size n)
{
    raf_size k;
    for (k = 0; k < n; ++k) {
        if (a[k] != b[k]) return 0;
    }
    return 1;
}

static void oracle_move(raf_u8 *a, raf_size dst, raf_size src, raf_size n)
{
    raf_u8 temp[64];
    raf_size k;
    for (k = 0; k < n; ++k) temp[k] = a[src + k];
    for (k = 0; k < n; ++k) a[dst + k] = temp[k];
}

int main(void)
{
    raf_u8 a[64] = {0};
    raf_u8 b[64] = {0};
    raf_u32 out = 0xdeadbeefu;
    raf_u32 rng = 0x53a71u;
    raf_size t, i;
    if (raf_fs_load_le32(a, 3, 0, &out) != -1 || out != 0xdeadbeefu) return 1;
    if (raf_fs_load_le32(a, 64, (raf_size)-1, &out) != -1) return 2;
    if (raf_fs_load_le32(0, 64, 0, &out) != -1) return 3;
    if (raf_fs_load_le32(a, 64, 0, 0) != -1) return 4;
    if (raf_fs_store_le32(a, 64, 61, 0x12345678u) != -1) return 5;
    if (raf_fs_store_le32(a, 64, 60, 0x12345678u) != 0) return 6;
    if (a[60] != 0x78 || a[61] != 0x56 || a[62] != 0x34 || a[63] != 0x12) return 7;
    if (raf_fs_load_le32(a, 64, 60, &out) || out != 0x12345678u) return 8;
    if (raf_fs_load_be32(a, 64, 60, &out) || out != 0x78563412u) return 9;
    if (raf_fs_store_be32(a, 64, 60, 0x01020304u)) return 10;
    if (a[60] != 1 || a[61] != 2 || a[62] != 3 || a[63] != 4) return 11;
    if (raf_fs_load_be32(a, 64, 60, &out) || out != 0x01020304u) return 12;
    if (raf_fs_move(a, 64, 63, 0, 2) != -1) return 13;
    if (raf_fs_move(0, 64, 0, 0, 0) != -1) return 14;
    for (t = 0; t < 32768; ++t) {
        raf_size s, d, n;
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        s = (raf_size)(rng % 65u);
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        d = (raf_size)(rng % 65u);
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        n = (raf_size)(rng % 65u);
        for (i = 0; i < 64; ++i) {
            a[i] = (raf_u8)((i * 37u + t) & 255u);
            b[i] = a[i];
        }
        if (n <= 64 - s && n <= 64 - d) {
            oracle_move(b, d, s, n);
            if (raf_fs_move(a, 64, d, s, n) || !equal(a, b, 64)) return 15;
        } else if (raf_fs_move(a, 64, d, s, n) != -1 || !equal(a, b, 64)) {
            return 16;
        }
    }
    return 0;
}
