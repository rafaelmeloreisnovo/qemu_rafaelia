/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
static r32 r_rotate(r32 x, r32 shift)
{
    return shift ? (x>>shift)|(x<<(32u-shift)) : x;
}
int r_decode(r32 w, rdecoded *o)
{
    r32 op;
    if (!o) return R_BAD_STATE;
    o->type=0; o->cond=(r8)(w>>28);
    if (o->cond!=14u && o->cond!=0u && o->cond!=1u)
        return R_UNSUPPORTED;
    o->rn=(r8)((w>>16)&15u); o->rd=(r8)((w>>12)&15u);
    o->s=(r8)((w>>20)&1u); o->up=(r8)((w>>23)&1u);
    o->imm=0;
    if ((w & 0x0f000000u)==0x0f000000u) {
        o->type=R_SVC_INS; o->imm=w&0x00ffffffu; return R_OK;
    }
    if ((w & 0x0e000000u)==0x0a000000u) {
        if (w&0x01000000u) return R_UNSUPPORTED; /* BL not yet supported. */
        o->type=R_B; o->imm=((w&0x00ffffffu)<<2);
        if (o->imm & 0x02000000u) o->imm |= 0xfc000000u;
        return R_OK;
    }
    if ((w & 0x0e000000u)==0x02000000u) {
        op=(w>>21)&15u;
        o->imm=r_rotate(w&255u, ((w>>8)&15u)*2u);
        if (op==2u && o->s && o->rn==14u && o->rd==15u &&
            o->imm==4u) { o->type=R_IRQ_RETURN; return R_OK; }
        if (o->rn==15u || o->rd==15u) return R_UNSUPPORTED;
        if (op==13u && !o->s) { o->type=R_MOV; return R_OK; }
        if (op==4u && !o->s) { o->type=R_ADD; return R_OK; }
        if (op==2u && !o->s) { o->type=R_SUB; return R_OK; }
        if (op==10u && o->s && o->rd==0u) {
            o->type=R_CMP; return R_OK;
        }
        return R_UNSUPPORTED;
    }
    if ((w & 0x0e000000u)==0x04000000u) {
        /* A32 immediate word transfer: preindexed, no writeback, no byte. */
        if (!(w&0x01000000u) || (w&0x00600000u) ||
            o->rn==15u || o->rd==15u) return R_UNSUPPORTED;
        o->imm=w&4095u; o->type=(r8)(o->s ? R_LDR : R_STR);
        return R_OK;
    }
    return R_UNSUPPORTED;
}
