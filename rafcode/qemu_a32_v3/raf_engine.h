/* SPDX-License-Identifier: MIT */
#ifndef RAF_A32_V3_ENGINE_H
#define RAF_A32_V3_ENGINE_H
#include "../qemu_guest_v2/raf_guest.h"
typedef raf_g8 r8;
typedef raf_g32 r32;
typedef raf_g64 r64;
typedef raf_gsize rsz;
enum { R_OK=0, R_IRQ=1, R_SVC=2, R_BUDGET=3, R_UNSUPPORTED=-1,
       R_MEMFAULT=-2, R_BAD_STATE=-3 };
enum { R_READ=1, R_WRITE=2, R_EXEC=4, R_PAGE=4096, R_MAPS=8, R_SECTOR=512 };
enum { R_C=0x20000000u, R_V=0x10000000u, R_Z=0x40000000u,
       R_N=0x80000000u, R_I=0x80u, R_T=0x20u, R_MODE=0x1fu,
       R_SYS=0x1fu, R_IRQ_MODE=0x12u };
typedef struct { r32 vpn; r64 pfn; r8 perm; r8 used; } rmap;
typedef struct { rmap entry[R_MAPS]; } rmmu;
typedef struct { r32 r[16]; r32 cpsr, spsr_irq, sp_irq, lr_irq;
                 r32 saved_sp, saved_lr, vector_base; r64 retired; } rcpu;
typedef struct { r8 pending; } rirq;
typedef struct { r8 *bytes; rsz length; } rblock;
typedef struct { rcpu cpu; rmmu mmu; rirq irq; raf_guest_window *ram;
                 rblock *disk; } rengine;
typedef struct { r8 type, cond, rn, rd, s, up; r32 imm; } rdecoded;
enum { R_MOV=1, R_ADD=2, R_SUB=3, R_CMP=4, R_B=5, R_LDR=6,
       R_STR=7, R_SVC_INS=8, R_IRQ_RETURN=9 };
void r_cpu_init(rcpu *cpu, r32 pc, r32 vector);
void r_mmu_init(rmmu *mmu);
int r_mmu_map(rmmu *mmu, const raf_guest_window *ram,
              r32 virtual_page, r64 physical_page, r8 perms);
int r_mmu_translate(const rmmu *mmu, r32 virtual_address,
                    r8 requested, r64 *physical_address);
int r_irq_raise(rirq *irq);
int r_irq_enter(rcpu *cpu, rirq *irq);
int r_irq_return(rcpu *cpu);
int r_decode(r32 instruction, rdecoded *out);
int r_block_init(rblock *disk, void *buffer, rsz length);
int r_dma_sector(rengine *e, r32 virtual_address, rsz sector, r8 to_ram);
int r_step(rengine *engine);
int r_run(rengine *engine, r32 budget, r32 *retired);
#endif
