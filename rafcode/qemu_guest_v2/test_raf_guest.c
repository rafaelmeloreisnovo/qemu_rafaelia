/* SPDX-License-Identifier: MIT */
#include "raf_guest.h"

static int same(const raf_g8 *a, const raf_g8 *b, raf_gsize n)
{
    raf_gsize k;
    for (k = 0; k < n; ++k) {
        if (a[k] != b[k]) return 0;
    }
    return 1;
}

static void oracle(raf_g8 *p, raf_gsize d, raf_gsize s, raf_gsize n)
{
    raf_g8 tmp[64];
    raf_gsize k;
    for (k = 0; k < n; ++k) tmp[k] = p[s + k];
    for (k = 0; k < n; ++k) p[d + k] = tmp[k];
}

int main(void)
{
    raf_g8 a[64] = {0};
    raf_g8 b[64] = {0};
    raf_guest_window w = {0};
    raf_g32 v = 0xfacebeefu;
    raf_g32 r = 0x13579bdfu;
    raf_gsize i, t;
    const raf_g64 base = 0x1234000000000000ULL;
    if (raf_guest_init(&w, a, 64, base)) return 1;
    if (raf_guest_store_le32(&w, base + 60u, 0x01234567u)) return 2;
    if (a[60] != 0x67u || a[61] != 0x45u ||
        a[62] != 0x23u || a[63] != 0x01u) return 3;
    if (raf_guest_load_le32(&w, base + 60u, &v) || v != 0x01234567u) return 4;
    v = 0xfacebeefu;
    if (raf_guest_load_le32(&w, base + 61u, &v) != -1 || v != 0xfacebeefu) return 5;
    if (raf_guest_load_le32(&w, base - 1u, &v) != -1 || v != 0xfacebeefu) return 6;
    if (raf_guest_store_le32(&w, __UINT64_MAX__, 3u) != -1 || a[63] != 0x01u) return 7;
    if (raf_guest_move(&w, base + 63u, base, 2u) != -1) return 8;
    if (raf_guest_init(&w, a, 64, __UINT64_MAX__ - 1u) != -1) return 9;
    if (w.guest_base != base || w.length != 64u) return 10;
    if (raf_guest_init(0, a, 64, base) != -1 ||
        raf_guest_init(&w, 0, 64, base) != -1 ||
        raf_guest_init(&w, a, 0, base) != -1) return 11;
    if (raf_guest_move(0, base, base, 0) != -1 ||
        raf_guest_load_le32(&w, base, 0) != -1) return 12;
    {
        raf_guest_window top = {0};
        raf_g8 hi[2] = {0x12, 0x34};
        if (raf_guest_init(&top, hi, 2u, __UINT64_MAX__ - 1u)) return 15;
        if (raf_guest_move(&top, __UINT64_MAX__, __UINT64_MAX__ - 1u, 1u) ||
            hi[0] != 0x12 || hi[1] != 0x12) return 16;
        if (raf_guest_move(&top, __UINT64_MAX__, __UINT64_MAX__, 2u) != -1 ||
            hi[1] != 0x12) return 17;
    }
    for (t = 0; t < 32768u; ++t) {
        raf_gsize s, d, n;
        r ^= r << 13; r ^= r >> 17; r ^= r << 5; s = (raf_gsize)(r % 65u);
        r ^= r << 13; r ^= r >> 17; r ^= r << 5; d = (raf_gsize)(r % 65u);
        r ^= r << 13; r ^= r >> 17; r ^= r << 5; n = (raf_gsize)(r % 65u);
        for (i = 0; i < 64u; ++i) {
            a[i] = (raf_g8)((i * 19u + t) & 255u);
            b[i] = a[i];
        }
        if (n <= 64u - s && n <= 64u - d) {
            oracle(b, d, s, n);
            if (raf_guest_move(&w, base + (raf_g64)d, base + (raf_g64)s, n) ||
                !same(a, b, 64)) return 13;
        } else if (raf_guest_move(&w, base + (raf_g64)d, base + (raf_g64)s, n) != -1 ||
                   !same(a, b, 64)) return 14;
    }
    return 0;
}
