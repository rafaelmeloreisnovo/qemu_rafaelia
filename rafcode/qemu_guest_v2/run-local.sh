#!/bin/sh
# Host test-only script; not a freestanding runtime dependency.
set -eu
cd "$(dirname "$0")"
: "${CC:=cc}"
for opt in 0 2 s; do
  obj="${TMPDIR:-/tmp}/raf-qemu-guest-$opt-$$.o"
  exe="${TMPDIR:-/tmp}/raf-qemu-guest-$opt-$$"
  trap 'rm -f "$obj" "$exe"' EXIT HUP INT TERM
  "$CC" -std=c11 -O"$opt" -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-unwind-tables -fno-asynchronous-unwind-tables -nostdinc \
    -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes -pedantic \
    -c raf_guest.c -o "$obj"
  if nm -u "$obj" | grep .; then echo 'unexpected undefined symbols' >&2; exit 1; fi
  "$CC" -std=c11 -O"$opt" -ffreestanding -fno-builtin -fno-stack-protector \
    -nostdinc -Wall -Wextra -Werror -Wconversion -Wshadow \
    -Wstrict-prototypes -pedantic raf_guest.c test_raf_guest.c -o "$exe"
  "$exe"
  rm -f "$obj" "$exe"
  trap - EXIT HUP INT TERM
  echo "PASS $CC O$opt 32768 bounded guest-address tests"
done
