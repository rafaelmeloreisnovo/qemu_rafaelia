/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
static int same(const r8 *a, const r8 *b, rsz n)
{
    rsz i; for(i=0;i<n;++i) if(a[i]!=b[i])return 0;
    return 1;
}
int main(void)
{
    r8 ram_buf[8192]={0},disk_buf[1024]={0},prior[512]={0};
    raf_guest_window ram={0};
    rblock disk={0};
    rengine e={0};
    r32 count=0, decoded_word=0x9a2e48b5u;
    r64 pa=0xFFFFFFFFFFFFFFFFULL;
    rsz i;
    const r32 program[7]={
        0xe3a00004u, /* MOV r0,#4 */
        0xe2801003u, /* ADD r1,r0,#3 */
        0xe5821000u, /* STR r1,[r2] */
        0xe5923000u, /* LDR r3,[r2] */
        0xe3530007u, /* CMP r3,#7 */
        0x1a000000u, /* BNE +0 (must be skipped by Z flag) */
        0xef000000u  /* SVC #0 -> explicit host trap */
    };
    if (raf_guest_init(&ram,ram_buf,8192u,0)) return 1;
    if (r_block_init(&disk,disk_buf,1024u)) return 2;
    e.ram=&ram; e.disk=&disk; r_cpu_init(&e.cpu,0,0x80u);
    r_mmu_init(&e.mmu); e.irq.pending=0;
    if (r_mmu_map(&e.mmu,&ram,0,0,R_READ|R_EXEC))return 3;
    if (r_mmu_map(&e.mmu,&ram,4096u,4096u,R_READ|R_WRITE))return 4;
    if (r_mmu_map(&e.mmu,&ram,0,4096u,R_READ) != R_BAD_STATE)return 5;
    if (r_mmu_translate(&e.mmu,0,R_WRITE,&pa)!=R_MEMFAULT ||
        pa!=0xFFFFFFFFFFFFFFFFULL)return 6;
    for (i=0;i<7u;++i)
        if(raf_guest_store_le32(&ram,(r64)(i*4u),program[i]))return 7;
    e.cpu.r[2]=4096u;
    if (r_run(&e,100u,&count)!=R_SVC || count!=6u ||
        e.cpu.r[0]!=4u || e.cpu.r[1]!=7u || e.cpu.r[3]!=7u ||
        e.cpu.r[15]!=24u || !(e.cpu.cpsr & R_Z))return 8;
    if (ram_buf[4096]!=7u)return 9;
    if (r_mmu_translate(&e.mmu,4096u,R_EXEC,&pa)!=R_MEMFAULT)return 10;
    if (r_mmu_translate(&e.mmu,4096u,R_READ,&pa) ||
        pa!=4096u)return 11;
    /* Fault does not modify flags, PC or output registers. */
    e.cpu.r[15]=4096u;
    if (r_step(&e)!=R_MEMFAULT || e.cpu.r[15]!=4096u)return 12;
    e.cpu.r[15]=0;
    /* IRQ at instruction boundary vectors to 0x98. */
    if (raf_guest_store_le32(&ram,0x98u,0xe25ef004u))return 13;
    e.cpu.r[13]=0x2000u; e.cpu.r[14]=0x777u;
    if (r_irq_raise(&e.irq) || r_step(&e)!=R_IRQ ||
        e.cpu.r[15]!=0x98u || e.cpu.r[14]!=4u ||
        !(e.cpu.cpsr & R_I) || (e.cpu.cpsr & R_MODE)!=R_IRQ_MODE)return 14;
    if (r_step(&e)!=R_OK || e.cpu.r[15]!=0 ||
        e.cpu.r[13]!=0x2000u || e.cpu.r[14]!=0x777u ||
        (e.cpu.cpsr & R_MODE)!=R_SYS)return 15;
    /* MMU permission prevents DMA to executable-only code. */
    for(i=0;i<512u;++i) { disk_buf[i]=(r8)(i&255u); prior[i]=ram_buf[i]; }
    if (r_dma_sector(&e,0,0,1)!=R_MEMFAULT ||
        !same(ram_buf,prior,512u))return 16;
    if (r_dma_sector(&e,4096u,0,1)!=R_OK ||
        !same(ram_buf+4096,disk_buf,512u))return 17;
    if (r_dma_sector(&e,4096u,1,0)!=R_OK ||
        !same(disk_buf+512,disk_buf,512u))return 18;
    if (r_dma_sector(&e,4096u,2,0)!=R_BAD_STATE ||
        r_dma_sector(&e,4097u,1,1)!=R_BAD_STATE)return 19;
    /* Unsupported A32 opcode and no implicit PC advancement. */
    if (raf_guest_store_le32(&ram,0,0xe7f000f0u))return 20;
    e.cpu.r[15]=0;
    if (r_step(&e)!=R_UNSUPPORTED || e.cpu.r[15]!=0)return 21;
    if (r_decode(0xef000000u,0)!=R_BAD_STATE)return 22;
    if (r_run(&e,0u,&count)!=R_BUDGET || count!=0)return 23;
    if (r_mmu_map(&e.mmu,&ram,8192u,8192u,R_READ)!=R_BAD_STATE)return 24;
    (void)decoded_word;
    return 0;
}
