/*
 * QEMU Process Health Check and Auto-Recovery
 *
 * Copyright (c) 2026 QEMU contributors
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/timer.h"
#include "qemu/thread.h"
#include "qemu/main-loop.h"
#include "qemu/error-report.h"
#include "hw/core/cpu.h"
#include "system/runstate.h"
#include "system/cpus.h"
#include "system/process-monitor.h"
#include "system/process-health.h"

/*
 * Health check and auto-recovery system for QEMU processes
 * Monitors system health and attempts automatic recovery from degraded states
 */

#define HEALTH_CHECK_INTERVAL_MS 1000  /* 1 second */
#define MAIN_LOOP_STALL_THRESHOLD 5000 /* 5 seconds without progress */
#define CPU_KICK_EXCESSIVE_RATE 10000  /* kicks per second threshold */
#define BQL_CONTENTION_HIGH 1000       /* contentions per second threshold */

typedef struct ProcessHealthStatus {
    HealthState state;
    uint64_t last_main_loop_count;
    int64_t last_check_time;
    int consecutive_stalls;
    bool recovery_in_progress;
    uint64_t recovery_attempts;
    uint64_t successful_recoveries;
} ProcessHealthStatus;

static ProcessHealthStatus health_status = {
    .state = HEALTH_STATE_HEALTHY,
    .consecutive_stalls = 0,
    .recovery_in_progress = false,
};

static QemuMutex health_mutex;
static QEMUTimer *health_check_timer;
static bool health_check_enabled = false;

/*
 * Check if main loop is making progress
 */
static bool check_main_loop_progress(void)
{
    uint64_t current_loops = 0;
    qemu_process_monitor_get_stats(&current_loops, NULL, NULL, NULL);
    
    bool progressing = (current_loops > health_status.last_main_loop_count);
    health_status.last_main_loop_count = current_loops;
    
    return progressing;
}

/*
 * Check for excessive CPU kicks (indicates possible spinning)
 */
static bool check_cpu_kick_rate(void)
{
    double kick_rate = 0;
    qemu_process_monitor_get_rates(NULL, &kick_rate, NULL);
    
    return kick_rate < CPU_KICK_EXCESSIVE_RATE;
}

/*
 * Check BQL contention levels
 * TODO: Implement proper rate calculation based on time window
 */
static bool check_bql_contention(void)
{
    /* Placeholder - needs proper rate-based implementation */
    return true;
}

/*
 * Attempt to recover from a stalled main loop
 * Note: Called without holding health_mutex to avoid blocking
 */
static void attempt_main_loop_recovery(void)
{
    bool already_recovering;
    
    qemu_mutex_lock(&health_mutex);
    already_recovering = health_status.recovery_in_progress;
    if (!already_recovering) {
        health_status.recovery_in_progress = true;
        health_status.recovery_attempts++;
    }
    qemu_mutex_unlock(&health_mutex);
    
    if (already_recovering) {
        return; /* Already recovering */
    }
    
    error_report("Health check: Main loop appears stalled, attempting recovery");
    
    /* Recovery strategy: gentle nudge to the event loop */
    qemu_notify_event();
    
    /* If we have CPUs, try kicking them (no mutex held) */
    CPUState *cpu;
    CPU_FOREACH(cpu) {
        if (!cpu_thread_is_idle(cpu)) {
            qemu_cpu_kick(cpu);
        }
    }
    
    qemu_mutex_lock(&health_mutex);
    health_status.recovery_in_progress = false;
    health_status.successful_recoveries++;
    qemu_mutex_unlock(&health_mutex);
}

/*
 * Attempt to recover from excessive CPU spinning
 * Note: Uses brief sleep to allow system to stabilize
 */
static void attempt_cpu_spin_recovery(void)
{
    error_report("Health check: Excessive CPU activity detected");
    
    /* Strategy: Brief pause to let things settle
     * Note: This is safe as we're not holding critical locks during sleep
     */
    usleep(10000); /* 10ms pause */
}

/*
 * Main health check callback
 */
static void health_check_callback(void *opaque)
{
    if (!health_check_enabled) {
        return;
    }
    
    qemu_mutex_lock(&health_mutex);
    
    int64_t current_time_ms = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);
    HealthState old_state = health_status.state;
    
    /* Check main loop progress */
    bool main_loop_ok = check_main_loop_progress();
    bool cpu_kick_ok = check_cpu_kick_rate();
    bool bql_ok = check_bql_contention();
    
    /* Update health state */
    if (!main_loop_ok) {
        health_status.consecutive_stalls++;
        
        if (health_status.consecutive_stalls > 3) {
            health_status.state = HEALTH_STATE_CRITICAL;
            /* Release mutex before potentially blocking recovery */
            qemu_mutex_unlock(&health_mutex);
            attempt_main_loop_recovery();
            qemu_mutex_lock(&health_mutex);
        } else {
            health_status.state = HEALTH_STATE_DEGRADED;
        }
    } else {
        health_status.consecutive_stalls = 0;
        
        if (!cpu_kick_ok) {
            health_status.state = HEALTH_STATE_DEGRADED;
            /* Release mutex before sleep */
            qemu_mutex_unlock(&health_mutex);
            attempt_cpu_spin_recovery();
            qemu_mutex_lock(&health_mutex);
        } else if (!bql_ok) {
            health_status.state = HEALTH_STATE_DEGRADED;
        } else {
            health_status.state = HEALTH_STATE_HEALTHY;
        }
    }
    
    /* Log state changes */
    if (old_state != health_status.state) {
        const char *state_names[] = {"HEALTHY", "DEGRADED", "CRITICAL"};
        error_report("Health check: State changed from %s to %s",
                    state_names[old_state], state_names[health_status.state]);
    }
    
    health_status.last_check_time = current_time_ms;
    
    qemu_mutex_unlock(&health_mutex);
    
    /* Reschedule next check */
    timer_mod(health_check_timer, current_time_ms + HEALTH_CHECK_INTERVAL_MS);
}

/*
 * Initialize the health check subsystem
 * Note: Must be called after qemu_process_monitor_init()
 */
void qemu_process_health_init(void)
{
    qemu_mutex_init(&health_mutex);
    
    /* Ensure process monitor is initialized first */
    qemu_process_monitor_init();
    
    health_check_timer = timer_new_ms(QEMU_CLOCK_REALTIME,
                                      health_check_callback, NULL);
    
    health_status.last_check_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);
}

/*
 * Enable health checking
 */
void qemu_process_health_enable(void)
{
    if (!health_check_enabled) {
        health_check_enabled = true;
        int64_t current_time = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);
        timer_mod(health_check_timer, current_time + HEALTH_CHECK_INTERVAL_MS);
    }
}

/*
 * Disable health checking
 */
void qemu_process_health_disable(void)
{
    if (health_check_enabled) {
        health_check_enabled = false;
        timer_del(health_check_timer);
    }
}

/*
 * Get current health status
 */
void qemu_process_health_get_status(int *state, int *stalls,
                                    uint64_t *attempts, uint64_t *successes)
{
    qemu_mutex_lock(&health_mutex);
    
    if (state) {
        *state = health_status.state;
    }
    if (stalls) {
        *stalls = health_status.consecutive_stalls;
    }
    if (attempts) {
        *attempts = health_status.recovery_attempts;
    }
    if (successes) {
        *successes = health_status.successful_recoveries;
    }
    
    qemu_mutex_unlock(&health_mutex);
}

/*
 * Manual trigger of health check (for testing)
 */
void qemu_process_health_check_now(void)
{
    health_check_callback(NULL);
}
