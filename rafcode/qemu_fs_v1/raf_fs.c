/* SPDX-License-Identifier: MIT */
#include "raf_fs.h"

static int raf_fs_range(raf_size length, raf_size offset, raf_size span)
{
    return offset <= length && span <= length - offset;
}

int raf_fs_load_le32(const void *base, raf_size length, raf_size offset,
                     raf_u32 *out)
{
    const raf_u8 *p;
    raf_u32 v;
    if (!base || !out || !raf_fs_range(length, offset, 4)) {
        return -1;
    }
    p = (const raf_u8 *)base + offset;
    v = (raf_u32)p[0] | (raf_u32)p[1] << 8 |
        (raf_u32)p[2] << 16 | (raf_u32)p[3] << 24;
    *out = v;
    return 0;
}

int raf_fs_load_be32(const void *base, raf_size length, raf_size offset,
                     raf_u32 *out)
{
    const raf_u8 *p;
    raf_u32 v;
    if (!base || !out || !raf_fs_range(length, offset, 4)) {
        return -1;
    }
    p = (const raf_u8 *)base + offset;
    v = (raf_u32)p[3] | (raf_u32)p[2] << 8 |
        (raf_u32)p[1] << 16 | (raf_u32)p[0] << 24;
    *out = v;
    return 0;
}

int raf_fs_store_le32(void *base, raf_size length, raf_size offset,
                      raf_u32 value)
{
    raf_u8 *p;
    if (!base || !raf_fs_range(length, offset, 4)) {
        return -1;
    }
    p = (raf_u8 *)base + offset;
    p[0] = (raf_u8)value;
    p[1] = (raf_u8)(value >> 8);
    p[2] = (raf_u8)(value >> 16);
    p[3] = (raf_u8)(value >> 24);
    return 0;
}

int raf_fs_store_be32(void *base, raf_size length, raf_size offset,
                      raf_u32 value)
{
    raf_u8 *p;
    if (!base || !raf_fs_range(length, offset, 4)) {
        return -1;
    }
    p = (raf_u8 *)base + offset;
    p[3] = (raf_u8)value;
    p[2] = (raf_u8)(value >> 8);
    p[1] = (raf_u8)(value >> 16);
    p[0] = (raf_u8)(value >> 24);
    return 0;
}

int raf_fs_move(void *arena, raf_size length, raf_size destination,
                raf_size source, raf_size count)
{
    raf_u8 *p;
    raf_size i;
    if (!arena || !raf_fs_range(length, destination, count) ||
        !raf_fs_range(length, source, count)) {
        return -1;
    }
    p = (raf_u8 *)arena;
    if (destination > source && destination - source < count) {
        i = count;
        while (i) {
            --i;
            p[destination + i] = p[source + i];
        }
    } else {
        for (i = 0; i < count; ++i) {
            p[destination + i] = p[source + i];
        }
    }
    return 0;
}
