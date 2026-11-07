/* SPDX-License-Identifier: MIT */
/* RAFCODE-Φ: isolated freestanding guest-address window, NOT QEMU runtime. */
#ifndef RAF_QEMU_GUEST_V2_H
#define RAF_QEMU_GUEST_V2_H
#if !defined(__UINT8_TYPE__) || !defined(__UINT32_TYPE__) || \
    !defined(__UINT64_TYPE__) || !defined(__UINT64_MAX__) || !defined(__SIZE_TYPE__)
#error "Compiler built-in unsigned types required (GCC/Clang)"
#endif

typedef __UINT8_TYPE__ raf_g8;
typedef __UINT32_TYPE__ raf_g32;
typedef __UINT64_TYPE__ raf_g64;
typedef __SIZE_TYPE__ raf_gsize;
_Static_assert(sizeof(raf_g8) == 1, "8-bit byte required");
_Static_assert(sizeof(raf_g32) == 4, "32-bit word required");
_Static_assert(sizeof(raf_g64) == 8, "64-bit address required");
_Static_assert(sizeof(raf_gsize) <= sizeof(raf_g64), "size_t wider than guest address");

typedef struct {
    raf_g8 *bytes;           /* Caller owns storage of at least `length` bytes. */
    raf_gsize length;
    raf_g64 guest_base;      /* Inclusive first guest physical address. */
} raf_guest_window;

/* No memory changes on failure; `window` is reset only on success.
 * 0 success, -1 bad pointers, zero length, or a 64-bit address wrap.
 */
int raf_guest_init(raf_guest_window *window, void *storage,
                   raf_gsize length, raf_g64 guest_base);

/* Exact bounds first, no partially successful operations. `out` unchanged
 * on errors. Caller must not alias `out` with the window's stored bytes.
 */
int raf_guest_load_le32(const raf_guest_window *window,
                        raf_g64 guest_addr, raf_g32 *out);
int raf_guest_store_le32(const raf_guest_window *window,
                         raf_g64 guest_addr, raf_g32 value);

/* memmove semantics between TWO locations IN THE SAME window.
 * Overlaps allowed. Invalid span produces no modifications.
 */
int raf_guest_move(const raf_guest_window *window, raf_g64 guest_dst,
                   raf_g64 guest_src, raf_gsize count);
#endif
