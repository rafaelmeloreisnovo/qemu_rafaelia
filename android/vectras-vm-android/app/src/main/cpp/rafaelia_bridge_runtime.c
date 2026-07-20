/*
 * rafaelia_bridge_runtime.c — Android JNI bridge: real QEMU process launch
 * and /proc-based metrics collection.
 *
 * Dispatch path:
 *   rafaelia_bridge_start_vm(vm_id)
 *     → reads  <files_dir>/vms/<vm_id>/qemu-argv.json  (argv array, one arg per line)
 *     → posix_spawn() the QEMU binary
 *     → tracks pid for stop/metrics
 *
 * Metrics:
 *   /proc/<pid>/stat  → utime/stime (cpu ticks)
 *   /proc/<pid>/status → VmRSS (memory)
 *   real values, not synthetic
 *
 * ARM32/ARM64 compatibility: all syscalls used here are available on
 * Android API 21+ (armeabi-v7a and arm64-v8a).
 */

#include "rafaelia_bridge_api.h"

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TAG            "RafaeliaBridge"
#define LOGI(...)      __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...)      __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* Maximum argv tokens we'll parse from the config file */
#define MAX_ARGV       256
/* Maximum path length */
#define MAX_PATH       512

/* --------------------------------------------------------------- */
/* Module state — guarded by g_lock                                */
/* --------------------------------------------------------------- */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pid_t           g_qemu_pid      = -1;
static bool            g_vm_started    = false;
static uint64_t        g_start_seconds = 0;
/* CPU accounting for rate computation */
static uint64_t        g_last_cpu_ticks = 0;
static uint64_t        g_last_cpu_wall  = 0;

/* --------------------------------------------------------------- */
/* Helpers                                                          */
/* --------------------------------------------------------------- */

static uint64_t monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
}

/* Locate the QEMU binary: check standard Vectras extraction paths */
static bool find_qemu_binary(char *out, size_t outsz, const char *files_dir)
{
    const char *names[] = {
        "qemu-system-x86_64",
        "qemu-system-aarch64",
        "qemu-system-arm",
        "qemu-system-i386",
        NULL,
    };
    for (int i = 0; names[i]; i++) {
        snprintf(out, outsz, "%s/usr/bin/%s", files_dir, names[i]);
        if (access(out, X_OK) == 0) return true;
    }
    /* fallback: /data/data package path */
    for (int i = 0; names[i]; i++) {
        snprintf(out, outsz, "/data/data/com.vectras.vm/files/usr/bin/%s", names[i]);
        if (access(out, X_OK) == 0) return true;
    }
    return false;
}

/*
 * Read argv from <files_dir>/vms/<vm_id>/qemu-argv.json
 * Format: JSON array of strings, one per line inside the array brackets.
 * Minimal parse: extracts quoted tokens between [ ] delimiters.
 * Returns number of tokens placed in argv[], or -1 on error.
 * Caller owns the argv[i] strings (free individually).
 */
static int read_vm_argv(const char *files_dir, const char *vm_id,
                        char **argv, int max_args)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/vms/%s/qemu-argv.json", files_dir, vm_id);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOGE("argv config not found: %s (errno=%d)", path, errno);
        return -1;
    }

    int count = 0;
    char line[1024];
    while (count < max_args && fgets(line, sizeof(line), fp)) {
        /* Locate opening quote */
        char *p = strchr(line, '"');
        if (!p) continue;
        p++;
        /* Locate closing quote (skip escaped quotes) */
        char *end = p;
        while (*end && !(*end == '"' && *(end - 1) != '\\')) end++;
        if (*end != '"') continue;
        size_t len = (size_t)(end - p);
        argv[count] = malloc(len + 1);
        if (!argv[count]) break;
        memcpy(argv[count], p, len);
        argv[count][len] = '\0';
        count++;
    }
    fclose(fp);
    return count;
}

/* Read utime+stime from /proc/<pid>/stat (fields 14 and 15, 1-indexed) */
static bool proc_cpu_ticks(pid_t pid, uint64_t *ticks_out)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    unsigned long utime = 0, stime = 0;
    /* stat format: pid (comm) state ppid pgroup session tty_nr tpgid flags
     *              minflt cminflt majflt cmajflt utime stime ...
     * Skip comm (may contain spaces inside parens) then read positional fields */
    char buf[512];
    bool ok = false;
    if (fgets(buf, sizeof(buf), fp)) {
        char *end_paren = strrchr(buf, ')');
        if (end_paren) {
            /* After ')' we have: state ppid pgroup session tty_nr tpgid flags
             *                    minflt cminflt majflt cmajflt utime stime
             * That is 12 more space-separated fields; utime is field index 11 */
            char *tok = strtok(end_paren + 2, " \t\n");
            int idx = 0;
            while (tok) {
                if (idx == 11) { utime = strtoul(tok, NULL, 10); }
                if (idx == 12) { stime = strtoul(tok, NULL, 10); ok = true; break; }
                idx++;
                tok = strtok(NULL, " \t\n");
            }
        }
    }
    fclose(fp);
    if (ok) *ticks_out = (uint64_t)utime + (uint64_t)stime;
    return ok;
}

/* Read VmRSS from /proc/<pid>/status (in kB) */
static bool proc_rss_kb(pid_t pid, uint64_t *kb_out)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[128];
    bool ok = false;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            char *p = line + 6;
            while (*p == ' ' || *p == '\t') p++;
            *kb_out = (uint64_t)strtoull(p, NULL, 10);
            ok = true;
            break;
        }
    }
    fclose(fp);
    return ok;
}

/* --------------------------------------------------------------- */
/* Public API                                                       */
/* --------------------------------------------------------------- */

bool rafaelia_bridge_start_vm(const char *vm_id)
{
    pthread_mutex_lock(&g_lock);

    if (g_vm_started) {
        LOGE("start_vm called while VM already running (pid=%d)", (int)g_qemu_pid);
        pthread_mutex_unlock(&g_lock);
        return false;
    }

    if (!vm_id || vm_id[0] == '\0') {
        LOGE("start_vm: empty vm_id");
        pthread_mutex_unlock(&g_lock);
        return false;
    }

    /* Determine files directory — prefer env var set by caller, fall back to
     * well-known Vectras package path. */
    const char *files_dir = getenv("RAFAELIA_FILES_DIR");
    if (!files_dir || files_dir[0] == '\0') {
        files_dir = "/data/data/com.vectras.vm/files";
    }

    char qemu_bin[MAX_PATH];
    if (!find_qemu_binary(qemu_bin, sizeof(qemu_bin), files_dir)) {
        LOGE("start_vm: QEMU binary not found under %s", files_dir);
        pthread_mutex_unlock(&g_lock);
        return false;
    }
    LOGI("start_vm: using binary %s for vm_id=%s", qemu_bin, vm_id);

    /* Read per-VM argv list */
    char *vm_argv[MAX_ARGV];
    memset(vm_argv, 0, sizeof(vm_argv));
    int arg_count = read_vm_argv(files_dir, vm_id, vm_argv, MAX_ARGV - 1);
    if (arg_count < 0) {
        /* No config file — fall back to bare binary with no extra args */
        LOGI("start_vm: no argv config found; launching bare binary");
        arg_count = 0;
    }

    /* Build final posix_spawn argv: [qemu_bin, vm_argv[0..N-1], NULL] */
    char *spawn_argv[MAX_ARGV + 2];
    spawn_argv[0] = qemu_bin;
    for (int i = 0; i < arg_count; i++) spawn_argv[i + 1] = vm_argv[i];
    spawn_argv[arg_count + 1] = NULL;

    /* Inherit environment from the current process */
    extern char **environ;

    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    /* Redirect stdout/stderr to logcat via /dev/null (Android logging happens
     * through __android_log_print; no PTY needed at this layer) */
    posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO,
                                     "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO,
                                     "/dev/null", O_WRONLY, 0);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    /* Reset signal mask so QEMU is not blocked by parent mask */
    sigset_t empty;
    sigemptyset(&empty);
    posix_spawnattr_setsigmask(&attr, &empty);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK);

    pid_t pid = -1;
    int rc = posix_spawn(&pid, qemu_bin, &file_actions, &attr, spawn_argv, environ);

    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&file_actions);
    for (int i = 0; i < arg_count; i++) { free(vm_argv[i]); vm_argv[i] = NULL; }

    if (rc != 0) {
        LOGE("start_vm: posix_spawn failed: %s (rc=%d)", strerror(rc), rc);
        pthread_mutex_unlock(&g_lock);
        return false;
    }

    LOGI("start_vm: launched pid=%d", (int)pid);
    g_qemu_pid      = pid;
    g_vm_started    = true;
    g_start_seconds = monotonic_ms() / 1000ULL;
    g_last_cpu_ticks = 0;
    g_last_cpu_wall  = monotonic_ms();

    pthread_mutex_unlock(&g_lock);
    return true;
}

bool rafaelia_bridge_stop_vm(void)
{
    pthread_mutex_lock(&g_lock);

    if (!g_vm_started || g_qemu_pid <= 0) {
        pthread_mutex_unlock(&g_lock);
        return true;
    }

    pid_t pid = g_qemu_pid;
    int rc = kill(pid, SIGTERM);
    if (rc != 0 && errno != ESRCH) {
        LOGE("stop_vm: SIGTERM to pid %d failed: %s", (int)pid, strerror(errno));
    }

    /* Give it 3 s to exit, then SIGKILL */
    for (int i = 0; i < 6; i++) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 500000000L };
        nanosleep(&ts, NULL);
        int status = 0;
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) goto done;
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);

done:
    LOGI("stop_vm: pid %d stopped", (int)pid);
    g_qemu_pid   = -1;
    g_vm_started = false;

    pthread_mutex_unlock(&g_lock);
    return true;
}

bool rafaelia_bridge_collect_metrics(rafaelia_bridge_metrics_t *metrics)
{
    if (!metrics) return false;
    memset(metrics, 0, sizeof(*metrics));

    pthread_mutex_lock(&g_lock);
    if (!g_vm_started || g_qemu_pid <= 0) {
        pthread_mutex_unlock(&g_lock);
        return true; /* zeros = VM not running, valid state */
    }

    pid_t pid = g_qemu_pid;

    /* Check the process still exists */
    if (kill(pid, 0) != 0) {
        LOGI("collect_metrics: pid %d exited; resetting state", (int)pid);
        g_qemu_pid   = -1;
        g_vm_started = false;
        pthread_mutex_unlock(&g_lock);
        return true;
    }

    /* Memory: VmRSS from /proc/<pid>/status */
    uint64_t rss_kb = 0;
    proc_rss_kb(pid, &rss_kb);
    metrics->memory_used_mb  = rss_kb / 1024ULL;
    metrics->memory_total_mb = 4096ULL; /* report 4 GiB as total capacity */

    /* CPU: delta of (utime+stime) ticks over elapsed wall time */
    uint64_t now_ms     = monotonic_ms();
    uint64_t elapsed_ms = now_ms - g_last_cpu_wall;
    uint64_t cpu_ticks  = 0;
    if (proc_cpu_ticks(pid, &cpu_ticks) && elapsed_ms > 0) {
        uint64_t delta_ticks   = cpu_ticks - g_last_cpu_ticks;
        long hz = sysconf(_SC_CLK_TCK);
        if (hz <= 0) hz = 100;
        /* cpu% = (delta_ticks / hz) / (elapsed_ms / 1000) * 100 */
        double cpu_pct = (double)delta_ticks * 100000.0
                         / ((double)hz * (double)elapsed_ms);
        if (cpu_pct > 100.0) cpu_pct = 100.0;
        metrics->cpu_usage_percent = (float)cpu_pct;
        g_last_cpu_ticks = cpu_ticks;
        g_last_cpu_wall  = now_ms;
    }

    metrics->vnc_connected = true; /* assume connected while running; VNC state
                                      requires a socket probe beyond this scope */

    pthread_mutex_unlock(&g_lock);
    return true;
}
