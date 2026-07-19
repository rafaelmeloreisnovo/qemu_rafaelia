/*
 * QEMU Process Performance Monitor
 *
 * Copyright (c) 2026 QEMU contributors
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/timer.h"
#include "qemu/thread.h"
#include "system/runstate.h"
#include "system/cpus.h"
#include "system/process-monitor.h"

/*
 * Performance monitoring for QEMU core processes
 * Tracks key metrics without external dependencies
 */

typedef struct ProcessStats {
    uint64_t main_loop_iterations;
    uint64_t cpu_kicks;
    uint64_t runstate_transitions;
    uint64_t bql_contentions;
    int64_t last_update_time;
} ProcessStats;

static ProcessStats process_stats = {0};
static QemuMutex stats_mutex;
static bool stats_initialized = false;

void qemu_process_monitor_init(void)
{
    if (!stats_initialized) {
        qemu_mutex_init(&stats_mutex);
        process_stats.last_update_time = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
        stats_initialized = true;
    }
}

void qemu_process_monitor_record_main_loop(void)
{
    if (stats_initialized) {
        qatomic_inc(&process_stats.main_loop_iterations);
    }
}

void qemu_process_monitor_record_cpu_kick(void)
{
    if (stats_initialized) {
        qatomic_inc(&process_stats.cpu_kicks);
    }
}

void qemu_process_monitor_record_runstate_change(void)
{
    if (stats_initialized) {
        qatomic_inc(&process_stats.runstate_transitions);
    }
}

void qemu_process_monitor_record_bql_contention(void)
{
    if (stats_initialized) {
        qatomic_inc(&process_stats.bql_contentions);
    }
}

void qemu_process_monitor_get_stats(uint64_t *loops, uint64_t *kicks,
                                    uint64_t *transitions, uint64_t *contentions)
{
    if (!stats_initialized) {
        return;
    }

    qemu_mutex_lock(&stats_mutex);
    if (loops) {
        *loops = process_stats.main_loop_iterations;
    }
    if (kicks) {
        *kicks = process_stats.cpu_kicks;
    }
    if (transitions) {
        *transitions = process_stats.runstate_transitions;
    }
    if (contentions) {
        *contentions = process_stats.bql_contentions;
    }
    qemu_mutex_unlock(&stats_mutex);
}

void qemu_process_monitor_reset(void)
{
    if (!stats_initialized) {
        return;
    }

    qemu_mutex_lock(&stats_mutex);
    process_stats.main_loop_iterations = 0;
    process_stats.cpu_kicks = 0;
    process_stats.runstate_transitions = 0;
    process_stats.bql_contentions = 0;
    process_stats.last_update_time = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
    qemu_mutex_unlock(&stats_mutex);
}

/*
 * Get performance metrics as a rate (per second)
 */
void qemu_process_monitor_get_rates(double *loop_rate, double *kick_rate,
                                    double *transition_rate)
{
    if (!stats_initialized) {
        return;
    }

    qemu_mutex_lock(&stats_mutex);
    
    int64_t current_time = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
    int64_t last_time = process_stats.last_update_time;
    int64_t elapsed_ns = current_time - last_time;
    
    if (elapsed_ns > 0) {
        double elapsed_sec = elapsed_ns / 1000000000.0;
        
        if (loop_rate) {
            *loop_rate = process_stats.main_loop_iterations / elapsed_sec;
        }
        if (kick_rate) {
            *kick_rate = process_stats.cpu_kicks / elapsed_sec;
        }
        if (transition_rate) {
            *transition_rate = process_stats.runstate_transitions / elapsed_sec;
        }
    }
    
    qemu_mutex_unlock(&stats_mutex);
}
