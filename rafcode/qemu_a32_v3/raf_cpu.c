/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
void r_cpu_init(rcpu *c, r32 pc, r32 vector)
{
    rsz i;
    if (!c) return;
    for (i=0; i<16; ++i) c->r[i]=0;
    c->cpsr=R_SYS; c->spsr_irq=0; c->sp_irq=0; c->lr_irq=0;
    c->saved_sp=0; c->saved_lr=0;
    c->vector_base=vector; c->retired=0; c->r[15]=pc;
}
