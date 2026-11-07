/* SPDX-License-Identifier: MIT */
/* Original RAFCODE freestanding byte-range kernel; no QEMU headers. */
#ifndef RAF_QEMU_FS_V1_H
#define RAF_QEMU_FS_V1_H

#if !defined(__UINT8_TYPE__) || !defined(__UINT32_TYPE__) || !defined(__SIZE_TYPE__)
#error "A GCC/Clang-compatible C compiler with builtin integer types is required"
#endif

typedef __UINT8_TYPE__ raf_u8;
typedef __UINT32_TYPE__ raf_u32;
typedef __SIZE_TYPE__ raf_size;

_Static_assert(sizeof(raf_u8) == 1, "8-bit bytes required");
_Static_assert(sizeof(raf_u32) == 4, "32-bit words required");

/* Return 0 on success, -1 for invalid pointers or bounds.
 * No output is changed on error. Input and output buffers are caller-owned.
 */
int raf_fs_load_le32(const void *base, raf_size length, raf_size offset,
                     raf_u32 *out);
int raf_fs_load_be32(const void *base, raf_size length, raf_size offset,
                     raf_u32 *out);
int raf_fs_store_le32(void *base, raf_size length, raf_size offset,
                      raf_u32 value);
int raf_fs_store_be32(void *base, raf_size length, raf_size offset,
                      raf_u32 value);
/* memmove semantics inside a single caller-owned arena; no allocation. */
int raf_fs_move(void *arena, raf_size length, raf_size destination,
                raf_size source, raf_size count);
#endif
