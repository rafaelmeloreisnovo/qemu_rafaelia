/*
 * Rafaelia Numbase Connector — bridges qemu_rafaelia with the raf_numbase
 * mathematical library that lives in termux-app-rafacodephi.
 *
 * Payload command protocol (ASCII, null-terminated in msg.payload):
 *   "to_base:<n>:<base>"           — convert n (decimal) to base (2–36)
 *   "from_base:<str>:<base>"       — parse str from base, return decimal
 *   "fibonacci:<n>"                — n-th Fibonacci number
 *   "tribonacci:<n>"               — n-th Tribonacci number
 *   "primonacci:<n>"               — n-th Primonacci number
 *   "seq_mod:<type>:<n>:<m>"       — sequence value mod m (type 0/1/2)
 *   "pisano_period:<m>"            — Fibonacci period mod m
 *   "base_efficiency:<base>:<max>" — radix economy
 *   "prime_fluid_graph:<p,...>:<mod>" — prime fluid graph JSON
 *   "analyze_special:<n,...>:<b,...>" — multi-base analysis JSON
 *   "zero_curve_dual:<a>:<b>"      — Z/aZ ∩ Z/bZ coincidence JSON
 *
 * Results are written to stderr via rafaelia_ipc_log. Numeric results are
 * returned as the handler's int return value (truncated to int range).
 */

#include "hw/core/rafaelia-integration.h"
#include "hw/core/rafaelia-connector-ipc.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Embedded raf_numbase implementation (stdlib-only, no external deps)
 * ========================================================================= */

static char *nb_to_base(long long n, int base, char *buf, int buf_len)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char tmp[68];
    int pos = 0, neg = 0, out = 0;
    unsigned long long magnitude;

    if (!buf || buf_len < 2 || base < 2 || base > 36) return NULL;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return buf; }

    if (n < 0) { neg = 1; magnitude = 0ULL - (unsigned long long)n; }
    else        { magnitude = (unsigned long long)n; }

    while (magnitude > 0ULL && pos < 67) {
        tmp[pos++] = digits[(int)(magnitude % (unsigned int)base)];
        magnitude /= (unsigned int)base;
    }

    if (neg && out < buf_len - 1) buf[out++] = '-';
    for (int i = pos - 1; i >= 0 && out < buf_len - 1; i--) buf[out++] = tmp[i];
    buf[out] = '\0';
    return buf;
}

static long long nb_from_base(const char *s, int base)
{
    long long r = 0;
    int neg = 0, i = 0;
    if (!s || base < 2 || base > 36) return 0;
    if (s[0] == '-') { neg = 1; i = 1; }
    for (; s[i]; i++) {
        int d;
        if      (s[i] >= '0' && s[i] <= '9') d = s[i] - '0';
        else if (s[i] >= 'a' && s[i] <= 'z') d = s[i] - 'a' + 10;
        else if (s[i] >= 'A' && s[i] <= 'Z') d = s[i] - 'A' + 10;
        else break;
        if (d >= base) break;
        r = r * base + d;
    }
    return neg ? -r : r;
}

static long long nb_fibonacci(int n)
{
    if (n <= 0) return 0;
    if (n == 1) return 1;
    long long a = 0, b = 1;
    for (int i = 2; i <= n; i++) { long long c = a + b; a = b; b = c; }
    return b;
}

static long long nb_tribonacci(int n)
{
    if (n <= 0) return 0;
    if (n == 1) return 0;
    if (n == 2) return 1;
    long long a = 0, b = 0, c = 1;
    for (int i = 3; i <= n; i++) { long long d = a + b + c; a = b; b = c; c = d; }
    return c;
}

static int nb_is_prime(long long n)
{
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (long long i = 3; i <= n / i; i += 2) if (n % i == 0) return 0;
    return 1;
}

static long long nb_next_prime(long long n)
{
    if (n < 2) return 2;
    long long p = (n % 2 == 0) ? n + 1 : n + 2;
    while (!nb_is_prime(p)) p += 2;
    return p;
}

static long long nb_primonacci(int n)
{
    if (n <= 0) return 2;
    if (n == 1) return 3;
    long long a = 2, b = 3;
    for (int i = 2; i <= n; i++) {
        long long sum = a + b;
        long long p = nb_is_prime(sum) ? sum : nb_next_prime(sum - 1);
        a = b; b = p;
    }
    return b;
}

static long long nb_seq_mod(int type, int n, int m)
{
    long long v;
    if (m <= 0) return 0;
    switch (type) {
    case 0: v = nb_fibonacci(n);  break;
    case 1: v = nb_tribonacci(n); break;
    case 2: v = nb_primonacci(n); break;
    default: return 0;
    }
    return ((v % m) + m) % m;
}

static int nb_pisano_period(int m)
{
    if (m <= 1) return 1;
    long long a = 0, b = 1;
    for (int i = 0; i < 6 * m; i++) {
        long long c = (a + b) % m;
        a = b; b = c;
        if (a == 0 && b == 1) return i + 1;
    }
    return 0;
}

static double nb_base_efficiency(int base, long long n_max)
{
    double digits;
    if (base < 2 || n_max <= 0) return 0.0;
    digits = ceil(log((double)n_max) / log((double)base));
    if (digits < 1.0) digits = 1.0;
    return digits * base;
}

#define NB_BUF 4096

static void japp(char *buf, int *pos, const char *s)
{
    while (*s && *pos < NB_BUF - 1) buf[(*pos)++] = *s++;
    buf[*pos] = '\0';
}

static void jprf(char *buf, int *pos, const char *fmt, ...)
{
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    japp(buf, pos, tmp);
}

static int nb_prime_fluid_graph(const int *primes, int n_primes, int mod,
                                 char *buf)
{
    int pos = 0, first = 1;
    if (!primes || n_primes <= 0) return -1;
    japp(buf, &pos, "{\"nodes\":[");
    for (int i = 0; i < n_primes; i++) {
        if (i > 0) japp(buf, &pos, ",");
        jprf(buf, &pos, "%d", primes[i]);
    }
    jprf(buf, &pos, "],\"mod\":%d,\"edges\":[", mod);
    for (int i = 0; i < n_primes; i++) {
        for (int j = i + 1; j < n_primes; j++) {
            int diff = primes[j] - primes[i];
            if (mod > 0 && diff % mod == 0) {
                if (!first) japp(buf, &pos, ",");
                jprf(buf, &pos,
                    "{\"from\":%d,\"to\":%d,\"diff\":%d,\"weight\":%.6f}",
                    primes[i], primes[j], diff, 1.0 / diff);
                first = 0;
            }
        }
    }
    japp(buf, &pos, "]}");
    return pos;
}

static int nb_zero_curve_dual(int base_a, int base_b, char *buf)
{
    int pos = 0, first = 1;
    int g = base_a, r = base_b;
    if (base_a < 2 || base_b < 2) return -1;
    while (r) { int t = r; r = g % r; g = t; }
    int lcm = (base_a / g) * base_b;

    jprf(buf, &pos,
        "{\"base_a\":%d,\"base_b\":%d,\"lcm\":%d,"
        "\"pisano_a\":%d,\"pisano_b\":%d,",
        base_a, base_b, lcm,
        nb_pisano_period(base_a), nb_pisano_period(base_b));

    japp(buf, &pos, "\"ring_a\":[");
    for (int i = 0; i < base_a; i++) {
        if (i > 0) japp(buf, &pos, ",");
        jprf(buf, &pos, "%d", i);
    }
    japp(buf, &pos, "],\"ring_b\":[");
    for (int i = 0; i < base_b; i++) {
        if (i > 0) japp(buf, &pos, ",");
        jprf(buf, &pos, "%d", i);
    }
    japp(buf, &pos, "],\"coincidences\":[");
    for (int i = 0; i <= lcm; i++) {
        if (i % base_a == 0 && i % base_b == 0) {
            if (!first) japp(buf, &pos, ",");
            jprf(buf, &pos, "%d", i);
            first = 0;
        }
    }
    japp(buf, &pos, "]}");
    return pos;
}

/* =========================================================================
 * Connector state and handler
 * ========================================================================= */

typedef struct {
    bool connected;
    pthread_mutex_t lock;
    rafaelia_ipc_runtime_t ipc;
    uint64_t ops_total;
} numbase_state_t;

static numbase_state_t numbase_state;
static uint64_t numbase_msg_seq;

static int numbase_dispatch(const char *cmd, char *result, int result_len)
{
    char base_buf[72];
    char json_buf[NB_BUF];
    int primes[64];
    long long nums[64];
    int bases[16];

    /* "to_base:<n>:<base>" */
    if (strncmp(cmd, "to_base:", 8) == 0) {
        long long n; int base;
        if (sscanf(cmd + 8, "%lld:%d", &n, &base) != 2) return -EINVAL;
        if (!nb_to_base(n, base, base_buf, (int)sizeof(base_buf))) return -EINVAL;
        snprintf(result, result_len, "%s", base_buf);
        return 0;
    }

    /* "from_base:<str>:<base>" */
    if (strncmp(cmd, "from_base:", 10) == 0) {
        char str[68]; int base;
        if (sscanf(cmd + 10, "%67[^:]:%d", str, &base) != 2) return -EINVAL;
        long long v = nb_from_base(str, base);
        snprintf(result, result_len, "%lld", v);
        return 0;
    }

    /* "fibonacci:<n>" */
    if (strncmp(cmd, "fibonacci:", 10) == 0) {
        int n = atoi(cmd + 10);
        snprintf(result, result_len, "%lld", nb_fibonacci(n));
        return 0;
    }

    /* "tribonacci:<n>" */
    if (strncmp(cmd, "tribonacci:", 11) == 0) {
        int n = atoi(cmd + 11);
        snprintf(result, result_len, "%lld", nb_tribonacci(n));
        return 0;
    }

    /* "primonacci:<n>" */
    if (strncmp(cmd, "primonacci:", 11) == 0) {
        int n = atoi(cmd + 11);
        snprintf(result, result_len, "%lld", nb_primonacci(n));
        return 0;
    }

    /* "seq_mod:<type>:<n>:<m>" */
    if (strncmp(cmd, "seq_mod:", 8) == 0) {
        int type, n, m;
        if (sscanf(cmd + 8, "%d:%d:%d", &type, &n, &m) != 3) return -EINVAL;
        snprintf(result, result_len, "%lld", nb_seq_mod(type, n, m));
        return 0;
    }

    /* "pisano_period:<m>" */
    if (strncmp(cmd, "pisano_period:", 14) == 0) {
        int m = atoi(cmd + 14);
        snprintf(result, result_len, "%d", nb_pisano_period(m));
        return 0;
    }

    /* "base_efficiency:<base>:<max>" */
    if (strncmp(cmd, "base_efficiency:", 16) == 0) {
        int base; long long n_max;
        if (sscanf(cmd + 16, "%d:%lld", &base, &n_max) != 2) return -EINVAL;
        snprintf(result, result_len, "%.6f", nb_base_efficiency(base, n_max));
        return 0;
    }

    /* "prime_fluid_graph:<p,...>:<mod>" */
    if (strncmp(cmd, "prime_fluid_graph:", 18) == 0) {
        int n_primes = 0, mod = 0;
        const char *p = cmd + 18;
        while (n_primes < 64 && *p && *p != ':') {
            primes[n_primes++] = (int)strtol(p, (char **)&p, 10);
            if (*p == ',') p++;
        }
        if (*p == ':') mod = atoi(p + 1);
        int len = nb_prime_fluid_graph(primes, n_primes, mod, json_buf);
        if (len < 0) return -EINVAL;
        snprintf(result, result_len, "%s", json_buf);
        return 0;
    }

    /* "analyze_special:<n,...>:<b,...>" */
    if (strncmp(cmd, "analyze_special:", 16) == 0) {
        int n_nums = 0, n_bases = 0;
        const char *p = cmd + 16;
        while (n_nums < 64 && *p && *p != ':') {
            nums[n_nums++] = strtoll(p, (char **)&p, 10);
            if (*p == ',') p++;
        }
        if (*p == ':') {
            p++;
            while (n_bases < 16 && *p) {
                bases[n_bases++] = (int)strtol(p, (char **)&p, 10);
                if (*p == ',') p++;
            }
        }
        /* Inline analyze_special to avoid raf_compile_contract dependency */
        static const int MODS[] = {7, 10, 14, 70};
        int pos = 0;
        char num_buf[72];
        japp(json_buf, &pos, "[");
        for (int i = 0; i < n_nums; i++) {
            if (i > 0) japp(json_buf, &pos, ",");
            long long n = nums[i];
            jprf(json_buf, &pos, "{\"n\":%lld,\"bases\":{", n);
            for (int b = 0; b < n_bases; b++) {
                if (b > 0) japp(json_buf, &pos, ",");
                nb_to_base(n, bases[b], num_buf, (int)sizeof(num_buf));
                jprf(json_buf, &pos, "\"%d\":\"%s\"", bases[b], num_buf);
            }
            japp(json_buf, &pos, "},\"mod\":{");
            for (int m = 0; m < 4; m++) {
                if (m > 0) japp(json_buf, &pos, ",");
                long long r = ((n % MODS[m]) + MODS[m]) % MODS[m];
                jprf(json_buf, &pos, "\"%d\":%lld", MODS[m], r);
            }
            int fib_idx = -1;
            long long fa = 0, fb = 1;
            for (int k = 0; k <= 86; k++) {
                if (fa == n) { fib_idx = k; break; }
                if (fa > n) break;
                long long next = fa + fb; fa = fb; fb = next;
            }
            jprf(json_buf, &pos, "},\"fib_index\":%d}", fib_idx);
        }
        japp(json_buf, &pos, "]");
        snprintf(result, result_len, "%s", json_buf);
        return 0;
    }

    /* "zero_curve_dual:<a>:<b>" */
    if (strncmp(cmd, "zero_curve_dual:", 16) == 0) {
        int a, b;
        if (sscanf(cmd + 16, "%d:%d", &a, &b) != 2) return -EINVAL;
        int len = nb_zero_curve_dual(a, b, json_buf);
        if (len < 0) return -EINVAL;
        snprintf(result, result_len, "%s", json_buf);
        return 0;
    }

    return -ENOTSUP;
}

static int numbase_backend_execute(const rafaelia_ipc_message_t *msg, void *opaque)
{
    numbase_state_t *st = opaque;
    char cmd[RAFAELIA_IPC_PAYLOAD_MAX + 1];
    char result[NB_BUF];
    int rc;

    pthread_mutex_lock(&st->lock);
    if (!st->connected) {
        pthread_mutex_unlock(&st->lock);
        return -ENOTCONN;
    }
    st->ops_total++;
    pthread_mutex_unlock(&st->lock);

    if (msg->payload_size == 0) return -EINVAL;
    memcpy(cmd, msg->payload, msg->payload_size);
    cmd[msg->payload_size] = '\0';

    result[0] = '\0';
    rc = numbase_dispatch(cmd, result, (int)sizeof(result));
    if (rc == 0 && result[0] != '\0') {
        fprintf(stderr, "[rafaelia][numbase] id=%" PRIu64 " cmd=%s result=%s\n",
                msg->message_id, cmd, result);
    }
    return rc;
}

static int numbase_connect(void *config)
{
    (void)config;

    pthread_mutex_lock(&numbase_state.lock);
    if (numbase_state.connected) {
        pthread_mutex_unlock(&numbase_state.lock);
        return 0;
    }

    memset(&numbase_state.ipc, 0, sizeof(numbase_state.ipc));
    numbase_state.ops_total = 0;

    if (rafaelia_ipc_runtime_init(&numbase_state.ipc,
                                   "numbase",
                                   numbase_backend_execute,
                                   &numbase_state) != 0) {
        pthread_mutex_unlock(&numbase_state.lock);
        return -1;
    }

    numbase_state.connected = true;
    pthread_mutex_unlock(&numbase_state.lock);
    return 0;
}

static int numbase_disconnect(void)
{
    pthread_mutex_lock(&numbase_state.lock);
    if (!numbase_state.connected) {
        pthread_mutex_unlock(&numbase_state.lock);
        return -1;
    }
    numbase_state.connected = false;
    pthread_mutex_unlock(&numbase_state.lock);

    rafaelia_ipc_runtime_destroy(&numbase_state.ipc);
    return 0;
}

static int numbase_submit(rafaelia_message_type_t type,
                          const void *data, size_t data_size,
                          const rafaelia_request_t *req)
{
    rafaelia_ipc_message_t msg;
    rafaelia_ipc_task_t task;

    pthread_mutex_lock(&numbase_state.lock);
    if (!numbase_state.connected) {
        pthread_mutex_unlock(&numbase_state.lock);
        return -ENOTCONN;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.message_id = ++numbase_msg_seq;
    msg.source = REPO_QEMU_RAFAELIA;
    msg.target = REPO_USERLAND;
    msg.priority = req ? req->priority : PRIORITY_NORMAL;
    msg.capabilities = req ? req->capabilities_required : CAP_COMPUTE;
    msg.timestamp_us = rafaelia_ipc_now_us();
    msg.payload_size = (uint32_t)(data_size > RAFAELIA_IPC_PAYLOAD_MAX
                                  ? RAFAELIA_IPC_PAYLOAD_MAX : data_size);
    if (data && msg.payload_size > 0) {
        memcpy(msg.payload, data, msg.payload_size);
    }
    rafaelia_blake3_256((const uint8_t *)&msg,
                        sizeof(msg) - sizeof(msg.digest), msg.digest);
    pthread_mutex_unlock(&numbase_state.lock);

    rafaelia_ipc_task_init(&task, &msg);
    return rafaelia_ipc_submit_sync(&numbase_state.ipc, &task,
                                    req ? req->timeout_ms : 1000);
}

static int numbase_send_request(const rafaelia_request_t *req)
{
    if (!req) return -EINVAL;
    return numbase_submit(MSG_REQUEST, req->data, req->data_size, req);
}

static int numbase_send_event(const rafaelia_event_t *evt)
{
    if (!evt) return -EINVAL;
    return numbase_submit(MSG_EVENT, evt->data, evt->data_size, NULL);
}

static int numbase_health_check(void)
{
    int ret;
    pthread_mutex_lock(&numbase_state.lock);
    ret = numbase_state.connected ? 0 : -ENOTCONN;
    pthread_mutex_unlock(&numbase_state.lock);
    return ret;
}

int rafaelia_numbase_connector_init(rafaelia_connector_t *conn)
{
    if (!conn) return -1;

    memset(&numbase_state, 0, sizeof(numbase_state));
    pthread_mutex_init(&numbase_state.lock, NULL);

    conn->connect       = numbase_connect;
    conn->disconnect    = numbase_disconnect;
    conn->send_request  = numbase_send_request;
    conn->send_event    = numbase_send_event;
    conn->health_check  = numbase_health_check;
    return 0;
}

/* =========================================================================
 * Convenience API — usable directly without going through the IPC queue
 * (synchronous, no threading; safe to call before connector is connected)
 * ========================================================================= */

long long rafaelia_numbase_fibonacci(int n)  { return nb_fibonacci(n);  }
long long rafaelia_numbase_tribonacci(int n) { return nb_tribonacci(n); }
long long rafaelia_numbase_primonacci(int n) { return nb_primonacci(n); }
int       rafaelia_numbase_pisano_period(int m) { return nb_pisano_period(m); }
double    rafaelia_numbase_base_efficiency(int base, long long n_max)
          { return nb_base_efficiency(base, n_max); }

char *rafaelia_numbase_to_base(long long n, int base, char *buf, int len)
          { return nb_to_base(n, base, buf, len); }
long long rafaelia_numbase_from_base(const char *s, int base)
          { return nb_from_base(s, base); }
