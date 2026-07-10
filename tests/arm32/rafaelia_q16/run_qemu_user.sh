#!/usr/bin/env sh
set -eu

# RAFAELIA Q16 — QEMU ARM user-mode portability harness
# Author: Rafael Melo Reis — RAFCODE-Φ / ∆RafaelVerboΩ
#
# This harness validates the arithmetic contract independently from the
# Android /system/bin/linker PIE used in the original armv7l execution.
# It uses a tiny Linux ARM EABI executable with direct syscalls.

WORK=${WORK:-rafaelia-q16-qemu-work}
CC=${CC:-clang}
QEMU_ARM=${QEMU_ARM:-qemu-arm}
TARGET=${TARGET:-arm-linux-gnueabihf}

mkdir -p "$WORK"
cd "$WORK"

cat <<'EOF' > q16_qemu.c
typedef unsigned int u32;
typedef int s32;
typedef long long s64;

#define Q16_SHIFT 16
#define GEOM 56756
#define FORCE 203333
#define TARGET_VALUE 1517675
#define TOLERANCE 64

static s32 step(s32 x)
{
    return (s32)(((s64)x * (s64)GEOM) >> Q16_SHIFT) + FORCE;
}

static s32 iterate(s32 x, u32 n)
{
    u32 i;
    for (i = 0u; i < n; ++i) x = step(x);
    return x;
}

static long sys_write(u32 fd, const char *p, u32 n)
{
    register long r0 __asm__("r0") = (long)fd;
    register long r1 __asm__("r1") = (long)p;
    register long r2 __asm__("r2") = (long)n;
    register long r7 __asm__("r7") = 4;
    __asm__ volatile("svc #0" : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r7)
                     : "memory", "cc");
    return r0;
}

static void write_all(const char *p, u32 n)
{
    while (n != 0u) {
        long w = sys_write(1u, p, n);
        if (w <= 0) return;
        p += (u32)w;
        n -= (u32)w;
    }
}

static void print_u32(u32 v)
{
    char b[10];
    u32 p = 10u;
    do {
        u32 q = v / 10u;
        b[--p] = (char)('0' + (v - q * 10u));
        v = q;
    } while (v != 0u);
    write_all(b + p, 10u - p);
}

__attribute__((noreturn)) static void sys_exit(int code)
{
    register long r0 __asm__("r0") = code;
    register long r7 __asm__("r7") = 1;
    __asm__ volatile("svc #0" : : "r"(r0), "r"(r7)
                     : "memory", "cc");
    __builtin_unreachable();
}

__attribute__((noreturn, used, visibility("default")))
void _start(void)
{
    s32 r48 = iterate(0, 48u);
    s32 r96 = iterate(0, 96u);
    s32 d = r96 - TARGET_VALUE;
    s32 min = 0;
    s32 max = 0;
    s32 fixed = 0;
    u32 count = 0u;
    u32 first = 0u;
    u32 i;
    s32 x;

    if (d < 0) d = -d;

    for (x = TARGET_VALUE - 256; x <= TARGET_VALUE + 256; ++x) {
        if (step(x) == x) {
            if (count == 0u) min = x;
            max = x;
            ++count;
        }
    }

    x = 0;
    for (i = 1u; i <= 256u; ++i) {
        x = step(x);
        if (step(x) == x) {
            first = i;
            fixed = x;
            break;
        }
    }

    write_all("r48=", 4u); print_u32((u32)r48);
    write_all("\nr96=", 5u); print_u32((u32)r96);
    write_all("\nerror=", 7u); print_u32((u32)d);
    write_all("\nfixed_count=", 13u); print_u32(count);
    write_all("\nfixed_min=", 11u); print_u32((u32)min);
    write_all("\nfixed_max=", 11u); print_u32((u32)max);
    write_all("\nfirst_fixed=", 13u); print_u32(first);
    write_all("\nfixed_value=", 13u); print_u32((u32)fixed);
    write_all("\n", 1u);

    if (r48 == 1516200 && r96 == 1517719 && d == 44 &&
        count == 7u && min == 1517719 && max == 1517725 &&
        first == 90u && fixed == 1517719 && d <= TOLERANCE) {
        sys_exit(0);
    }
    sys_exit(1);
}
EOF

command -v "$CC" >/dev/null 2>&1 || {
    echo "missing compiler: $CC" >&2
    exit 127
}
command -v "$QEMU_ARM" >/dev/null 2>&1 || {
    echo "missing emulator: $QEMU_ARM" >&2
    exit 127
}

"$CC" --target="$TARGET" -O3 -marm -nostdlib -ffreestanding \
    -fno-builtin -fno-stack-protector -fno-unwind-tables \
    -fno-asynchronous-unwind-tables -ffunction-sections \
    -fdata-sections -Wl,-e,_start -Wl,--gc-sections \
    -Wl,--build-id=none -Wl,-static q16_qemu.c -o q16_qemu_arm

"$QEMU_ARM" ./q16_qemu_arm | tee qemu-output.txt
status=$?

echo "qemu_status=$status"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum q16_qemu_arm qemu-output.txt
fi
exit "$status"
