/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
static int r_cond(r32 cpsr, r8 cond)
{
    if (cond==14u) return 1;
    if (cond==0u) return !!(cpsr & R_Z);
    if (cond==1u) return !(cpsr & R_Z);
    return -1;
}
static int r_data_addr(r32 base, r32 offset, r8 up, r32 *out)
{
    if (!out) return R_BAD_STATE;
    *out=up ? base+offset : base-offset;
    return (*out & 3u) ? R_MEMFAULT : R_OK;
}
int r_step(rengine *e)
{
    r32 pc, word, addr, a, b, value, result;
    r64 physical;
    rdecoded d;
    int status, test;
    rcpu *cpu;
    if (!e || !e->ram || !e->ram->bytes) return R_BAD_STATE;
    cpu=&e->cpu;
    if ((cpu->cpsr & R_T) ||
        ((cpu->cpsr & R_MODE)!=R_SYS && (cpu->cpsr & R_MODE)!=R_IRQ_MODE))
        return R_UNSUPPORTED;
    status=r_irq_enter(cpu,&e->irq);
    if (status!=R_OK) return status;
    pc=cpu->r[15];
    if (pc&3u) return R_MEMFAULT;
    status=r_mmu_translate(&e->mmu,pc,R_EXEC,&physical);
    if (status) return status;
    /* Instruction may not cross a 4K mapped page. */
    if ((pc & (R_PAGE-1u))>R_PAGE-4u) return R_MEMFAULT;
    if (raf_guest_load_le32(e->ram,physical,&word)) return R_MEMFAULT;
    status=r_decode(word,&d);
    if (status) return status;
    test=r_cond(cpu->cpsr,d.cond);
    if (test<0) return R_UNSUPPORTED;
    if (!test) { cpu->r[15]=pc+4u; ++cpu->retired; return R_OK; }
    if (d.type==R_IRQ_RETURN) {
        status=r_irq_return(cpu);
        if (!status) ++cpu->retired;
        return status;
    }
    if (d.type==R_SVC_INS) return R_SVC; /* Host-visible trap, no SVC vector emulated. */
    if (d.type==R_MOV || d.type==R_ADD || d.type==R_SUB || d.type==R_CMP) {
        a=cpu->r[d.rn]; b=d.imm;
        if (d.type==R_MOV) cpu->r[d.rd]=b;
        if (d.type==R_ADD) cpu->r[d.rd]=a+b;
        if (d.type==R_SUB) cpu->r[d.rd]=a-b;
        if (d.type==R_CMP) {
            result=a-b;
            cpu->cpsr &= ~(R_N|R_Z|R_C|R_V);
            if (result&0x80000000u) cpu->cpsr |= R_N;
            if (!result) cpu->cpsr |= R_Z;
            if (a>=b) cpu->cpsr |= R_C;
            if (((a^b)&(a^result)&0x80000000u)!=0u) cpu->cpsr |= R_V;
        }
    } else if (d.type==R_B) {
        cpu->r[15]=pc+8u+d.imm;
        ++cpu->retired; return R_OK;
    } else if (d.type==R_LDR || d.type==R_STR) {
        status=r_data_addr(cpu->r[d.rn],d.imm,d.up,&addr);
        if (status) return status;
        status=r_mmu_translate(&e->mmu,addr,
                               d.type==R_LDR ? R_READ : R_WRITE,&physical);
        if (status) return status;
        if ((addr & (R_PAGE-1u))>R_PAGE-4u) return R_MEMFAULT;
        if (d.type==R_LDR) {
            status=raf_guest_load_le32(e->ram,physical,&value);
            if (status) return R_MEMFAULT;
            cpu->r[d.rd]=value;
        } else {
            if (raf_guest_store_le32(e->ram,physical,cpu->r[d.rd]))
                return R_MEMFAULT;
        }
    } else return R_UNSUPPORTED;
    cpu->r[15]=pc+4u; ++cpu->retired;
    return R_OK;
}
