/*
 * RAFAELIA RMR - Low-overhead memory and runtime tuning helpers
 */

#include "qemu/osdep.h"
#include "qemu/cacheinfo.h"
#include "qemu/log.h"
#include "qemu/memalign.h"
#include "hw/core/rafaelia-rmr.h"
#include "hw/core/rafaelia-rmr-lowlevel.h"
#include "hw/core/rafaelia-rmr-instr.h"

#ifdef CONFIG_POSIX
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if defined(CONFIG_LINUX)
#include <sys/sysinfo.h>
#endif

static rafaelia_rmr_memalign_fn rafaelia_rmr_memalign_alloc = qemu_memalign;

void rafaelia_rmr_pool_set_memalign_for_test(rafaelia_rmr_memalign_fn fn)
{
    rafaelia_rmr_memalign_alloc = fn ? fn : qemu_memalign;
}

void rafaelia_rmr_pool_reset_memalign_for_test(void)
{
    rafaelia_rmr_memalign_alloc = qemu_memalign;
}

static size_t rafaelia_rmr_round_up(size_t value, size_t align)
{
    if (align == 0) {
        return value;
    }
    return (value + align - 1) / align * align;
}

static uint32_t rafaelia_rmr_default_alignment(void)
{
    if (qemu_dcache_linesize > 0) {
        return (uint32_t)qemu_dcache_linesize;
    }
    return 64;
}

rafaelia_rmr_pool_t *rafaelia_rmr_pool_create(size_t element_size,
                                              uint32_t capacity,
                                              uint32_t alignment)
{
    rafaelia_rmr_pool_t *pool;
    size_t stride;
    size_t total;
    uint8_t *cursor;
    void **slot;

    if (element_size == 0 || capacity == 0) {
        return NULL;
    }

    stride = element_size;
    if (stride < sizeof(void *)) {
        stride = sizeof(void *);
    }

    if (alignment == 0) {
        alignment = rafaelia_rmr_default_alignment();
    }

    stride = rafaelia_rmr_round_up(stride, alignment);

    if (capacity > SIZE_MAX / stride) {
        return NULL;
    }

    total = stride * capacity;

    pool = g_new0(rafaelia_rmr_pool_t, 1);
    pool->buffer = rafaelia_rmr_memalign_alloc(alignment, total);
    if (!pool->buffer) {
        g_free(pool);
        return NULL;
    }

    pool->element_size = element_size;
    pool->stride = stride;
    pool->capacity = capacity;
    pool->alignment = alignment;
    pool->begin = (uintptr_t)pool->buffer;
    pool->end = pool->begin + total;

    cursor = (uint8_t *)pool->buffer;
    for (uint32_t i = 0; i < capacity; i++) {
        slot = (void **)cursor;
        *slot = pool->free_list;
        pool->free_list = slot;
        cursor += stride;
    }

    return pool;
}

void rafaelia_rmr_pool_destroy(rafaelia_rmr_pool_t *pool)
{
    if (!pool) {
        return;
    }
    qemu_vfree(pool->buffer);
    g_free(pool);
}

void *rafaelia_rmr_pool_alloc(rafaelia_rmr_pool_t *pool)
{
    void *entry;

    if (!pool || !pool->free_list) {
        return NULL;
    }

    entry = rafaelia_rmr_pool_alloc_uninitialized(pool);
    if (entry) {
        rafaelia_rmr_memzero(entry, pool->element_size);
    }
    return entry;
}

void *rafaelia_rmr_pool_alloc_uninitialized(rafaelia_rmr_pool_t *pool)
{
    void *entry;

    if (!pool || !pool->free_list) {
        return NULL;
    }

    entry = pool->free_list;
    pool->free_list = *(void **)entry;
    pool->in_use++;
    return entry;
}

void rafaelia_rmr_pool_free(rafaelia_rmr_pool_t *pool, void *ptr)
{
    if (!pool || !ptr) {
        return;
    }

    if (!rafaelia_rmr_pool_owns(pool, ptr)) {
#ifdef DEBUG_RAFAELIA_RMR
        qemu_log_mask(LOG_GUEST_ERROR,
                      "rafaelia-rmr: ignoring free of non-pool pointer %p\n",
                      ptr);
#endif
        return;
    }

    *(void **)ptr = pool->free_list;
    pool->free_list = ptr;
    if (pool->in_use > 0) {
        pool->in_use--;
    }
}

bool rafaelia_rmr_pool_owns(const rafaelia_rmr_pool_t *pool, const void *ptr)
{
    uintptr_t addr;

    if (!pool || !ptr) {
        return false;
    }

    addr = (uintptr_t)ptr;
    if (addr < pool->begin || addr >= pool->end) {
        return false;
    }

    return ((addr - pool->begin) % pool->stride) == 0;
}

void rafaelia_rmr_detect(rafaelia_rmr_hw_profile_t *profile)
{
    if (!profile) {
        return;
    }

    rafaelia_rmr_memzero(profile, sizeof(*profile));

#if defined(__x86_64__) || defined(_M_X64)
    profile->arch = "x86_64";
#elif defined(__aarch64__)
    profile->arch = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
    profile->arch = "arm";
#elif defined(__ppc64__) || defined(__powerpc64__)
    profile->arch = "ppc64";
#elif defined(__riscv)
    profile->arch = "riscv";
#else
    profile->arch = "unknown";
#endif

    profile->pointer_bits = (uint32_t)(sizeof(void *) * 8);
    profile->cacheline_bytes = rafaelia_rmr_default_alignment();
    profile->alignment_bytes = profile->cacheline_bytes;
#ifdef CONFIG_POSIX
    profile->page_bytes = (uint32_t)getpagesize();
#else
    profile->page_bytes = 4096u;
#endif

#if defined(__GNUC__) || defined(__clang__)
    profile->has_prefetch = true;
#else
    profile->has_prefetch = false;
#endif
}

bool rafaelia_rmr_collect_instruments(rafaelia_rmr_instrument_snapshot_t *snapshot)
{
#ifdef CONFIG_POSIX
    struct utsname uts;
#endif
#if defined(CONFIG_LINUX)
    struct sysinfo info;
#endif

    if (!snapshot) {
        return false;
    }

    rafaelia_rmr_memzero(snapshot, sizeof(*snapshot));

#if defined(__x86_64__) || defined(_M_X64)
    snapshot->arch = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    snapshot->arch = "x86";
#elif defined(__aarch64__)
    snapshot->arch = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
    snapshot->arch = "arm";
#elif defined(__powerpc64__) || defined(__ppc64__)
    snapshot->arch = "ppc64";
#elif defined(__riscv)
    snapshot->arch = "riscv";
#else
    snapshot->arch = "unknown";
#endif

#if defined(CONFIG_LINUX)
    snapshot->os = "linux";
#elif defined(CONFIG_DARWIN)
    snapshot->os = "darwin";
#elif defined(CONFIG_WIN32)
    snapshot->os = "windows";
#else
    snapshot->os = "unknown";
#endif

    snapshot->pointer_bits = (uint32_t)(sizeof(void *) * 8);
#ifdef CONFIG_POSIX
    snapshot->page_bytes = (uint32_t)getpagesize();
#else
    snapshot->page_bytes = 4096u;
#endif

#if defined(_SC_NPROCESSORS_ONLN)
    {
        long cpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpus > 0 && cpus <= UINT32_MAX) {
            snapshot->cpu_online = (uint32_t)cpus;
        }
    }
#endif

#ifdef CONFIG_POSIX
    if (uname(&uts) == 0) {
        rafaelia_rmr_strlcpy(snapshot->kernel_release, uts.release,
                             sizeof(snapshot->kernel_release));
        rafaelia_rmr_strlcpy(snapshot->machine, uts.machine,
                             sizeof(snapshot->machine));
    }
#endif

#if defined(CONFIG_LINUX)
    if (sysinfo(&info) == 0) {
        snapshot->uptime_seconds = (uint64_t)info.uptime;
        snapshot->total_ram_kib = ((uint64_t)info.totalram * info.mem_unit) / 1024u;
        snapshot->free_ram_kib = ((uint64_t)info.freeram * info.mem_unit) / 1024u;
    }
#endif

#if defined(CONFIG_LINUX)
    snapshot->has_kvm_accel = access("/dev/kvm", R_OK | W_OK) == 0;
#else
    snapshot->has_kvm_accel = false;
#endif

#if defined(__x86_64__) || defined(__i386__)
    rafaelia_rmr_collect_instruments_x86(snapshot);
#elif defined(__aarch64__)
    rafaelia_rmr_collect_instruments_arm64(snapshot);
#elif defined(__riscv)
    rafaelia_rmr_collect_instruments_riscv(snapshot);
#endif

#ifdef CONFIG_POSIX
    rafaelia_rmr_collect_instruments_posix(snapshot);
#endif

    return true;
}


static bool rafaelia_rmr_arch_is(const char *arch, const char *name)
{
    size_t arch_len;
    size_t name_len;

    if (!arch || !name) {
        return false;
    }

    arch_len = rafaelia_rmr_strlen(arch);
    name_len = rafaelia_rmr_strlen(name);
    if (arch_len != name_len) {
        return false;
    }

    return rafaelia_rmr_memcmp(arch, name, arch_len) == 0;
}

static uint32_t rafaelia_rmr_arch_rank(const char *arch)
{
    if (!arch) {
        return 0;
    }

    if (rafaelia_rmr_arch_is(arch, "x86_64")) {
        return 5;
    }
    if (rafaelia_rmr_arch_is(arch, "aarch64")) {
        return 5;
    }
    if (rafaelia_rmr_arch_is(arch, "riscv")) {
        return 4;
    }
    if (rafaelia_rmr_arch_is(arch, "ppc64")) {
        return 4;
    }
    if (rafaelia_rmr_arch_is(arch, "x86")) {
        return 3;
    }
    if (rafaelia_rmr_arch_is(arch, "arm")) {
        return 3;
    }

    return 1;
}

bool rafaelia_rmr_route_select(const rafaelia_rmr_instrument_snapshot_t *snapshot,
                               rafaelia_rmr_route_decision_t *decision)
{
    uint32_t score = 0;

    if (!snapshot || !decision) {
        return false;
    }

    rafaelia_rmr_memzero(decision, sizeof(*decision));

    score += rafaelia_rmr_arch_rank(snapshot->arch) * 10u;

    if (snapshot->pointer_bits >= 64) {
        score += 20u;
    }

    if (snapshot->cpu_online >= 16) {
        score += 20u;
    } else if (snapshot->cpu_online >= 8) {
        score += 14u;
    } else if (snapshot->cpu_online >= 4) {
        score += 8u;
    } else if (snapshot->cpu_online >= 2) {
        score += 4u;
    }

    if (snapshot->total_ram_kib >= (32u * 1024u * 1024u)) {
        score += 20u;
    } else if (snapshot->total_ram_kib >= (8u * 1024u * 1024u)) {
        score += 12u;
    } else if (snapshot->total_ram_kib >= (2u * 1024u * 1024u)) {
        score += 6u;
    }

    if (snapshot->page_bytes >= 16384u) {
        score += 6u;
        decision->prefers_large_pages = true;
    } else if (snapshot->page_bytes >= 4096u) {
        score += 3u;
    }

    if (snapshot->has_kvm_accel) {
        score += 30u;
        decision->route = RAFAELIA_RMR_ROUTE_KVM_ACCEL;
        decision->lane_id = 3u;
    } else if (score >= 70u) {
        decision->route = RAFAELIA_RMR_ROUTE_HOST_FAST;
        decision->lane_id = 2u;
    } else if (score >= 35u) {
        decision->route = RAFAELIA_RMR_ROUTE_PORTABLE;
        decision->lane_id = 1u;
    } else {
        decision->route = RAFAELIA_RMR_ROUTE_FALLBACK;
        decision->lane_id = 0u;
    }

    decision->route_score = score;
    return true;
}
