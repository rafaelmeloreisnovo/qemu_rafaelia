/*
 * RAFAELIA Runtime Hook API
 */

#ifndef HW_RAFAELIA_RUNTIME_H
#define HW_RAFAELIA_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum rafaelia_runtime_mode {
    RAFAELIA_RUNTIME_MODE_SILENT = 0,
    RAFAELIA_RUNTIME_MODE_LOG,
    RAFAELIA_RUNTIME_MODE_TRACE,
    RAFAELIA_RUNTIME_MODE_SYMBIOSIS,
    RAFAELIA_RUNTIME_MODE_AUDIT,
    RAFAELIA_RUNTIME_MODE_BENCH,
} rafaelia_runtime_mode_t;

typedef struct rafaelia_runtime_config {
    bool enabled;
    bool debug;
    uint32_t tick_ms;
    rafaelia_runtime_mode_t mode;
    const char *vm_name;
} rafaelia_runtime_config_t;

rafaelia_runtime_config_t rafaelia_runtime_config_default(void);
const char *rafaelia_runtime_mode_name(rafaelia_runtime_mode_t mode);
bool rafaelia_runtime_parse_mode(const char *mode_str,
                                 rafaelia_runtime_mode_t *mode);

void rafaelia_runtime_set_config(const rafaelia_runtime_config_t *config);
void rafaelia_runtime_init(void);
void rafaelia_runtime_tick(uint32_t dt_ms);
void rafaelia_runtime_shutdown(void);

#endif /* HW_RAFAELIA_RUNTIME_H */
