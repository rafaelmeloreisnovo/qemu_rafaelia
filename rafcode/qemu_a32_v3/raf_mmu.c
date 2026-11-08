/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
void r_mmu_init(rmmu *m)
{
    rsz i;
    if (!m) return;
    for (i=0; i<R_MAPS; ++i) {
        m->entry[i].vpn=0; m->entry[i].pfn=0;
        m->entry[i].perm=0; m->entry[i].used=0;
    }
}
int r_mmu_map(rmmu *m, const raf_guest_window *ram,
              r32 va, r64 pa, r8 perm)
{
    rsz i, free_slot=R_MAPS;
    if (!m || !ram || !ram->bytes || (va & (R_PAGE-1u)) ||
        (pa & (R_PAGE-1u)) || !(perm & (R_READ|R_WRITE|R_EXEC)) ||
        (perm & (r8)~(R_READ|R_WRITE|R_EXEC)) || pa < ram->guest_base)
        return R_BAD_STATE;
    if (pa-ram->guest_base > (r64)ram->length ||
        (rsz)R_PAGE > ram->length-(rsz)(pa-ram->guest_base))
        return R_BAD_STATE;
    for (i=0; i<R_MAPS; ++i) {
        if (m->entry[i].used && m->entry[i].vpn==(va>>12))
            return R_BAD_STATE;
        if (!m->entry[i].used && free_slot==R_MAPS) free_slot=i;
    }
    if (free_slot==R_MAPS) return R_BAD_STATE;
    m->entry[free_slot].vpn=va>>12; m->entry[free_slot].pfn=pa;
    m->entry[free_slot].perm=perm; m->entry[free_slot].used=1;
    return R_OK;
}
int r_mmu_translate(const rmmu *m, r32 va, r8 perm, r64 *pa)
{
    rsz i;
    if (!m || !pa || !perm || (perm & (r8)~(R_READ|R_WRITE|R_EXEC)))
        return R_BAD_STATE;
    for (i=0; i<R_MAPS; ++i)
        if (m->entry[i].used && m->entry[i].vpn==(va>>12)) {
            if ((m->entry[i].perm & perm)!=perm) return R_MEMFAULT;
            *pa=m->entry[i].pfn+(r64)(va & (R_PAGE-1u));
            return R_OK;
        }
    return R_MEMFAULT;
}
