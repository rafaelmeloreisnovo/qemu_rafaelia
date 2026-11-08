/* SPDX-License-Identifier: MIT */
#include "raf_engine.h"
int r_run(rengine *e, r32 budget, r32 *retired)
{
    r32 i;
    int result;
    if (!e || !retired) return R_BAD_STATE;
    *retired=0;
    for (i=0;i<budget;++i) {
        result=r_step(e);
        if (result!=R_OK) return result;
        ++*retired;
    }
    return R_BUDGET;
}
