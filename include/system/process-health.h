/*
 * QEMU Process Health Check and Auto-Recovery
 *
 * Copyright (c) 2026 QEMU contributors
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef QEMU_PROCESS_HEALTH_H
#define QEMU_PROCESS_HEALTH_H

typedef enum {
    HEALTH_STATE_HEALTHY = 0,
    HEALTH_STATE_DEGRADED,
    HEALTH_STATE_CRITICAL,
} HealthState;

/*
 * Initialize the health check subsystem
 */
void qemu_process_health_init(void);

/*
 * Enable periodic health checking
 */
void qemu_process_health_enable(void);

/*
 * Disable periodic health checking
 */
void qemu_process_health_disable(void);

/*
 * Get current health status
 * state: HEALTH_STATE_* value
 * stalls: consecutive main loop stalls detected
 * attempts: total recovery attempts
 * successes: successful recoveries
 */
void qemu_process_health_get_status(int *state, int *stalls,
                                    uint64_t *attempts, uint64_t *successes);

/*
 * Manually trigger a health check (mainly for testing)
 */
void qemu_process_health_check_now(void);

#endif /* QEMU_PROCESS_HEALTH_H */
