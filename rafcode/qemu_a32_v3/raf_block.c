/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
int r_block_init(rblock *b, void *p, rsz n)
{
    if (!b || !p || !n || (n % R_SECTOR)) return R_BAD_STATE;
    b->bytes=(r8 *)p; b->length=n; return R_OK;
}
/* A one-sector DMA, constrained within one mapped 4K page.
 * Storage and RAM must refer to disjoint caller-owned arrays.
 * Prefight all bounds before writing any byte. */
int r_dma_sector(rengine *e, r32 va, rsz sector, r8 to_ram)
{
    r64 pa, base;
    rsz i, roff, soff;
    int status;
    if (!e || !e->disk || !e->ram || !e->disk->bytes ||
        !e->ram->bytes || (va & (R_SECTOR-1u)) ||
        (va & (R_PAGE-1u))>R_PAGE-R_SECTOR ||
        sector>=e->disk->length/R_SECTOR || (to_ram!=0 && to_ram!=1))
        return R_BAD_STATE;
    status=r_mmu_translate(&e->mmu,va,to_ram ? R_WRITE : R_READ,&pa);
    if (status) return status;
    base=e->ram->guest_base;
    if (pa<base || pa-base>(r64)e->ram->length ||
        R_SECTOR>e->ram->length-(rsz)(pa-base)) return R_MEMFAULT;
    roff=(rsz)(pa-base); soff=sector*R_SECTOR;
    for (i=0; i<R_SECTOR; ++i) {
        if (to_ram) e->ram->bytes[roff+i]=e->disk->bytes[soff+i];
        else e->disk->bytes[soff+i]=e->ram->bytes[roff+i];
    }
    return R_OK;
}
