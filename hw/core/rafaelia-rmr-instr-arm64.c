#include "qemu/osdep.h"
#include "hw/core/rafaelia-rmr-instr.h"

#if defined(CONFIG_LINUX) && defined(__aarch64__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

bool rafaelia_rmr_collect_instruments_arm64(rafaelia_rmr_instrument_snapshot_t *snapshot)
{
    if (!snapshot) {
        return false;
    }

#if defined(__aarch64__)
#if defined(CONFIG_LINUX) && defined(AT_HWCAP)
    unsigned long hwcap = getauxval(AT_HWCAP);
#ifdef HWCAP_ATOMICS
    if ((hwcap & HWCAP_ATOMICS) != 0u && snapshot->cpu_online < 2u) {
        snapshot->cpu_online = 2u;
    }
#endif
#endif

    if (snapshot->page_bytes < 4096u) {
        snapshot->page_bytes = 4096u;
    }
#else
    (void)snapshot;
#endif

    return true;
}
