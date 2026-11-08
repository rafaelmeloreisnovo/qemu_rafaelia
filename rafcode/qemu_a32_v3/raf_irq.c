/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
int r_irq_raise(rirq *irq)
{
    if (!irq) return R_BAD_STATE;
    irq->pending=1; return R_OK;
}
int r_irq_enter(rcpu *c, rirq *irq)
{
    if (!c || !irq) return R_BAD_STATE;
    if (!irq->pending || (c->cpsr & R_I)) return R_OK;
    if ((c->cpsr & R_MODE)!=R_SYS || (c->cpsr & R_T))
        return R_UNSUPPORTED;
    /* A32 IRQ boundary: LR_irq=next instruction +4, SPSR_irq=CPSR. */
    c->spsr_irq=c->cpsr;
    c->saved_sp=c->r[13]; c->saved_lr=c->r[14];
    c->r[13]=c->sp_irq; c->r[14]=c->r[15]+4u;
    c->lr_irq=c->r[14];
    c->cpsr=(c->cpsr & ~(R_MODE|R_T)) | R_IRQ_MODE | R_I;
    c->r[15]=c->vector_base+0x18u;
    irq->pending=0;
    return R_IRQ;
}
int r_irq_return(rcpu *c)
{
    r32 pc;
    if (!c || (c->cpsr & R_MODE)!=R_IRQ_MODE) return R_BAD_STATE;
    if ((c->spsr_irq & R_MODE)!=R_SYS || (c->spsr_irq & R_T))
        return R_UNSUPPORTED;
    pc=c->r[14]-4u;
    c->sp_irq=c->r[13]; c->lr_irq=c->r[14];
    c->r[13]=c->saved_sp; c->r[14]=c->saved_lr;
    c->r[15]=pc; c->cpsr=c->spsr_irq;
    return R_OK;
}
