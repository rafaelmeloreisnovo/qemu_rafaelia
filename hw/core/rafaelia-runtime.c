/*
 * RAFAELIA Runtime Hook
 */

#include "qemu/osdep.h"
#include <inttypes.h>
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/notify.h"
#include "qemu/timer.h"
#include "qemu/thread.h"
#include "system/runstate.h"
#include "hw/core/rafaelia-core.h"
#include "hw/core/rafaelia-integration.h"
#include "hw/core/rafaelia-runtime.h"
#include "trace.h"

#define RAFAELIA_RUNTIME_TICK_CAP 100u

/*
 * RAFAELIA runtime state is owned by the main loop thread.
 * Access from other threads must be serialized by the main loop.
 */
typedef struct rafaelia_runtime_state {
    rafaelia_runtime_config_t config;
    rafaelia_context_t context;
    rafaelia_core_t core;
    rafaelia_integration_hub_t hub;
    QemuMutex lock;
    bool lock_initialized;
    bool hub_initialized;
    bool initialized;
    bool running;
    uint64_t ticks_total;
    double entropy_last;
    double coherence_last;
    int64_t last_tick_ms;
    RunState last_runstate;
    Notifier shutdown_notifier;
    VMChangeStateEntry *vm_state_entry;
    QEMUTimer *tick_timer;
} rafaelia_runtime_state_t;

static rafaelia_runtime_state_t rafaelia_runtime_state;

rafaelia_runtime_config_t rafaelia_runtime_config_default(void)
{
    return (rafaelia_runtime_config_t){
        .enabled = false,
        .debug = false,
        .tick_ms = 100,
        .mode = RAFAELIA_RUNTIME_MODE_SILENT,
        .vm_name = NULL,
    };
}

static void rafaelia_runtime_sanitize_config(rafaelia_runtime_config_t *config)
{
    if (!config) {
        return;
    }

    if (config->tick_ms == 0) {
        config->tick_ms = 1;
    }
    if (config->tick_ms > 10000) {
        config->tick_ms = 10000;
    }
}

void rafaelia_runtime_set_config(const rafaelia_runtime_config_t *config)
{
    if (!config) {
        return;
    }

    if (!rafaelia_runtime_state.lock_initialized) {
        qemu_mutex_init(&rafaelia_runtime_state.lock);
        rafaelia_runtime_state.lock_initialized = true;
    }

    qemu_mutex_lock(&rafaelia_runtime_state.lock);
    rafaelia_runtime_state.config = *config;
    rafaelia_runtime_sanitize_config(&rafaelia_runtime_state.config);
    qemu_mutex_unlock(&rafaelia_runtime_state.lock);
}

static void rafaelia_runtime_log_tick(const rafaelia_runtime_state_t *state)
{
    if (!state) {
        return;
    }

    if (state->config.mode == RAFAELIA_RUNTIME_MODE_TRACE ||
        state->config.debug) {
        trace_rafaelia_runtime_tick(state->ticks_total,
                                    (int64_t)(state->entropy_last * 1e6),
                                    (int64_t)(state->coherence_last * 1e6),
                                    state->running,
                                    rafaelia_runtime_mode_name(state->config.mode));
    }

    if (state->config.mode == RAFAELIA_RUNTIME_MODE_LOG ||
        state->config.debug) {
        qemu_log_mask(LOG_TRACE,
                      "RAFAELIA runtime tick=%" PRIu64
                      " entropy=%.6f coherence=%.6f running=%d mode=%s\n",
                      state->ticks_total,
                      state->entropy_last,
                      state->coherence_last,
                      state->running,
                      rafaelia_runtime_mode_name(state->config.mode));
    }
}

static void rafaelia_runtime_log_state_change(const rafaelia_runtime_state_t *state)
{
    if (!state) {
        return;
    }

    if (state->config.mode == RAFAELIA_RUNTIME_MODE_TRACE ||
        state->config.debug) {
        trace_rafaelia_runtime_state_change(state->running,
                                            RunState_str(state->last_runstate));
    }
}

static void rafaelia_runtime_log_event(const rafaelia_runtime_state_t *state,
                                       const char *event)
{
    if (!state || !event) {
        return;
    }

    if (state->config.mode == RAFAELIA_RUNTIME_MODE_LOG ||
        state->config.debug) {
        qemu_log_mask(LOG_TRACE, "RAFAELIA runtime %s mode=%s\n",
                      event,
                      rafaelia_runtime_mode_name(state->config.mode));
    }
}

static void rafaelia_runtime_tick_once(rafaelia_runtime_state_t *state)
{
    rafaelia_loop_step(&state->context, &state->core);
    state->ticks_total++;
    state->entropy_last = rafaelia_cycle_measure(&state->core.cycle);
    state->coherence_last = state->core.phi_ethica.coerencia;

    rafaelia_runtime_log_tick(state);
}

static uint32_t rafaelia_runtime_tick_interval_ms(const rafaelia_runtime_state_t *state)
{
    uint32_t tick_ms = state->config.tick_ms;

    if (state->running) {
        return tick_ms;
    }

    if (tick_ms > (UINT32_MAX / 5)) {
        return tick_ms;
    }

    return tick_ms * 5;
}

static void rafaelia_runtime_maybe_tick(rafaelia_runtime_state_t *state,
                                        int64_t now_ms)
{
    int64_t elapsed_ms;
    uint32_t interval_ms;
    uint64_t ticks_to_run;

    if (!state || !state->initialized || !state->config.enabled) {
        return;
    }

    interval_ms = rafaelia_runtime_tick_interval_ms(state);
    if (state->last_tick_ms == 0) {
        state->last_tick_ms = now_ms;
        return;
    }

    elapsed_ms = now_ms - state->last_tick_ms;
    if (elapsed_ms < (int64_t)interval_ms) {
        return;
    }

    ticks_to_run = elapsed_ms / interval_ms;
    if (ticks_to_run > RAFAELIA_RUNTIME_TICK_CAP) {
        ticks_to_run = RAFAELIA_RUNTIME_TICK_CAP;
    }

    for (uint64_t i = 0; i < ticks_to_run; i++) {
        rafaelia_runtime_tick_once(state);
    }

    state->last_tick_ms += (int64_t)ticks_to_run * interval_ms;
}

static void rafaelia_runtime_schedule_next(rafaelia_runtime_state_t *state,
                                           int64_t now_ms)
{
    uint32_t interval_ms;
    int64_t next_ms;

    if (!state || !state->tick_timer || !state->initialized ||
        !state->config.enabled) {
        return;
    }

    interval_ms = rafaelia_runtime_tick_interval_ms(state);
    if (state->last_tick_ms != 0) {
        next_ms = state->last_tick_ms + interval_ms;
    } else {
        next_ms = now_ms + interval_ms;
    }

    timer_mod(state->tick_timer, next_ms);
}

static void rafaelia_runtime_tick_timer(void *opaque)
{
    rafaelia_runtime_state_t *state = opaque;
    int64_t now_ms;

    if (!state) {
        return;
    }

    qemu_mutex_lock(&state->lock);
    if (!state->initialized || !state->config.enabled) {
        qemu_mutex_unlock(&state->lock);
        return;
    }

    now_ms = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);
    rafaelia_runtime_maybe_tick(state, now_ms);
    rafaelia_runtime_schedule_next(state, now_ms);
    qemu_mutex_unlock(&state->lock);
}

static void rafaelia_runtime_vm_state_cb(void *opaque, bool running,
                                        RunState state)
{
    rafaelia_runtime_state_t *rt = opaque;

    if (!rt) {
        return;
    }

    qemu_mutex_lock(&rt->lock);
    rt->running = running;
    rt->last_runstate = state;
    rt->last_tick_ms = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);

    rafaelia_runtime_log_state_change(rt);
    rafaelia_runtime_schedule_next(rt, rt->last_tick_ms);
    qemu_mutex_unlock(&rt->lock);
}

static void rafaelia_runtime_shutdown_notify(Notifier *notifier, void *data)
{
    rafaelia_runtime_shutdown();
}

void rafaelia_runtime_init(void)
{
    rafaelia_runtime_state_t *state = &rafaelia_runtime_state;
    int hub_rc;

    if (!state->config.enabled) {
        return;
    }

    if (!state->lock_initialized) {
        qemu_mutex_init(&state->lock);
        state->lock_initialized = true;
    }
    qemu_mutex_lock(&state->lock);
    if (state->initialized) {
        qemu_mutex_unlock(&state->lock);
        return;
    }

    rafaelia_context_init(&state->context);
    rafaelia_fiat_portal_init(&state->context, &state->core);
    hub_rc = rafaelia_integration_hub_init(&state->hub, &state->core);
    if (hub_rc == 0) {
        state->hub_initialized = true;
        rafaelia_integration_hub_start(&state->hub);
    } else {
        error_report("RAFAELIA runtime: integration hub init failed");
    }

    state->running = runstate_is_running();
    state->last_runstate = runstate_get();
    state->last_tick_ms = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);

    state->tick_timer = timer_new_ms(QEMU_CLOCK_REALTIME,
                                     rafaelia_runtime_tick_timer,
                                     state);
    rafaelia_runtime_schedule_next(state, state->last_tick_ms);

    state->vm_state_entry = qemu_add_vm_change_state_handler(
        rafaelia_runtime_vm_state_cb, state);

    state->shutdown_notifier.notify = rafaelia_runtime_shutdown_notify;
    qemu_register_shutdown_notifier(&state->shutdown_notifier);

    state->initialized = true;

    rafaelia_runtime_log_state_change(state);
    rafaelia_runtime_log_event(state, "init");
    qemu_mutex_unlock(&state->lock);
}

void rafaelia_runtime_tick(uint32_t dt_ms)
{
    rafaelia_runtime_state_t *state = &rafaelia_runtime_state;
    uint32_t interval_ms;
    uint64_t ticks_to_run;

    qemu_mutex_lock(&state->lock);
    if (!state->initialized || !state->config.enabled) {
        qemu_mutex_unlock(&state->lock);
        return;
    }

    interval_ms = state->config.tick_ms ? state->config.tick_ms : 1;
    if (dt_ms == 0) {
        dt_ms = interval_ms;
    }

    ticks_to_run = dt_ms / interval_ms;
    if (ticks_to_run == 0) {
        ticks_to_run = 1;
    }
    if (ticks_to_run > RAFAELIA_RUNTIME_TICK_CAP) {
        ticks_to_run = RAFAELIA_RUNTIME_TICK_CAP;
    }

    for (uint64_t i = 0; i < ticks_to_run; i++) {
        rafaelia_runtime_tick_once(state);
    }

    if (state->last_tick_ms == 0) {
        state->last_tick_ms = qemu_clock_get_ms(QEMU_CLOCK_REALTIME);
    }
    state->last_tick_ms += (int64_t)ticks_to_run * interval_ms;
    qemu_mutex_unlock(&state->lock);
}

void rafaelia_runtime_shutdown(void)
{
    rafaelia_runtime_state_t *state = &rafaelia_runtime_state;

    qemu_mutex_lock(&state->lock);
    if (!state->initialized) {
        qemu_mutex_unlock(&state->lock);
        return;
    }

    trace_rafaelia_runtime_shutdown(state->ticks_total,
                                    (int64_t)(state->entropy_last * 1e6),
                                    (int64_t)(state->coherence_last * 1e6));
    rafaelia_runtime_log_event(state, "shutdown");

    if (state->vm_state_entry) {
        qemu_del_vm_change_state_handler(state->vm_state_entry);
        state->vm_state_entry = NULL;
    }

    if (state->tick_timer) {
        timer_del(state->tick_timer);
        timer_free(state->tick_timer);
        state->tick_timer = NULL;
    }

    if (state->hub_initialized) {
        rafaelia_integration_hub_stop(&state->hub);
        rafaelia_integration_hub_cleanup(&state->hub);
        state->hub_initialized = false;
    }

    rafaelia_core_cleanup(&state->context, &state->core);
    rafaelia_context_cleanup(&state->context);
    state->initialized = false;
    qemu_mutex_unlock(&state->lock);
    if (state->lock_initialized) {
        qemu_mutex_destroy(&state->lock);
        state->lock_initialized = false;
    }
}
