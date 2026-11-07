/* SPDX-License-Identifier: MIT */
#include "raf_guest.h"

/* Subtractive checks avoid addr + count wrap and untrusted pointer math. */
static int raf_guest_range(const raf_guest_window *w,
                           raf_g64 addr, raf_gsize count, raf_gsize *offset)
{
    raf_g64 d;
    if (!w || !w->bytes || !w->length || !offset || addr < w->guest_base) {
        return -1;
    }
    d = addr - w->guest_base;
    if (d > (raf_g64)w->length || count > w->length - (raf_gsize)d) {
        return -1;
    }
    /* No invalid pointers are constructed before all range checks pass. */
    *offset = (raf_gsize)d;
    return 0;
}

int raf_guest_init(raf_guest_window *w, void *storage,
                   raf_gsize length, raf_g64 guest_base)
{
    if (!w || !storage || !length ||
        (raf_g64)(length - 1u) > (__UINT64_MAX__ - guest_base)) {
        return -1;
    }
    w->bytes = (raf_g8 *)storage;
    w->length = length;
    w->guest_base = guest_base;
    return 0;
}

int raf_guest_load_le32(const raf_guest_window *w,
                        raf_g64 addr, raf_g32 *out)
{
    raf_gsize offset;
    const raf_g8 *p;
    raf_g32 value;
    if (!out || raf_guest_range(w, addr, 4u, &offset)) {
        return -1;
    }
    p = w->bytes + offset;
    value = (raf_g32)p[0] | ((raf_g32)p[1] << 8) |
            ((raf_g32)p[2] << 16) | ((raf_g32)p[3] << 24);
    *out = value;
    return 0;
}

int raf_guest_store_le32(const raf_guest_window *w,
                         raf_g64 addr, raf_g32 value)
{
    raf_gsize offset;
    raf_g8 *p;
    if (raf_guest_range(w, addr, 4u, &offset)) {
        return -1;
    }
    p = w->bytes + offset;
    p[0] = (raf_g8)value;
    p[1] = (raf_g8)(value >> 8);
    p[2] = (raf_g8)(value >> 16);
    p[3] = (raf_g8)(value >> 24);
    return 0;
}

int raf_guest_move(const raf_guest_window *w, raf_g64 dst,
                   raf_g64 src, raf_gsize count)
{
    raf_gsize doff, soff, i;
    if (raf_guest_range(w, dst, count, &doff) ||
        raf_guest_range(w, src, count, &soff)) {
        return -1;
    }
    if (doff > soff && doff - soff < count) {
        i = count;
        while (i) {
            --i;
            w->bytes[doff + i] = w->bytes[soff + i];
        }
    } else {
        for (i = 0; i < count; ++i) {
            w->bytes[doff + i] = w->bytes[soff + i];
        }
    }
    return 0;
}
