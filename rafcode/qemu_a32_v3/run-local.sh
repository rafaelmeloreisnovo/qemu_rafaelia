#!/bin/sh
# Test-only host utilities; runtime core never links libc/CRT.
set -eu
cd "$(dirname "$0")"
: "${CC:=cc}"
objs="raf_cpu.c raf_mmu.c raf_irq.c raf_decode.c raf_block.c raf_exec.c raf_run.c ../qemu_guest_v2/raf_guest.c"
for opt in 0 2 s; do
  obj="${TMPDIR:-/tmp}/raf-a32-$opt-$$.o"
  exe="${TMPDIR:-/tmp}/raf-a32-test-$opt-$$"
  trap 'rm -f "$obj" "$exe"' EXIT HUP INT TERM
  for f in $objs; do
    "$CC" -std=c11 -O"$opt" -ffreestanding -fno-builtin -fno-stack-protector -nostdinc \
      -Wall -Wextra -Werror -Wconversion -Wshadow -pedantic -c "$f" -o "$obj"
    if nm -u "$obj" | grep -E '(__|malloc|memcpy|memmove|memset|syscall)' ; then
      echo "forbidden runtime symbol in $f" >&2; exit 1
    fi
  done
  "$CC" -std=c11 -O"$opt" -ffreestanding -fno-builtin -fno-stack-protector -nostdinc \
    -Wall -Wextra -Werror -Wconversion -Wshadow -pedantic \
    $objs test_engine.c -o "$exe"
  "$exe"
  rm -f "$obj" "$exe"
  trap - EXIT HUP INT TERM
  echo "PASS A32 subset CC=$CC O$opt"
done
