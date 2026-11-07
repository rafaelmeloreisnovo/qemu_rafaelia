#!/bin/sh
# Test-only tooling; not linked into the freestanding core.
set -eu
cd "$(dirname "$0")"
: "${CC:=cc}"
for opt in 0 2 s; do
  "$CC" -std=c11 -O"$opt" -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -nostdinc -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes -pedantic -c raf_fs.c -o /tmp/raf-fs-core-"$opt".o
  if nm -u /tmp/raf-fs-core-"$opt".o | grep .; then echo 'unexpected undefined symbols' >&2; exit 1; fi
  "$CC" -std=c11 -O"$opt" -ffreestanding -fno-builtin -fno-stack-protector -nostdinc -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes -pedantic raf_fs.c test_raf_fs.c -o /tmp/raf-fs-test-"$opt"
  /tmp/raf-fs-test-"$opt"
  echo "PASS CC=$CC O$opt 32768 move cases + endian/bounds"
done
