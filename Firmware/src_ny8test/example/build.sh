#!/bin/sh
# Build example/blink.c with Nyquest's LLVM toolchain (part of NYC_NY8 >= 2.30) and
# convert it into ../ny8_image.h for src_ny8test.
#
# NYC_LLVM points to the "llvm" directory of the NYC_NY8 installation.
# On Linux/macOS the Windows binaries run under Wine (set RUN=wine).
set -e
NYC_LLVM=${NYC_LLVM:-"C:/Nyquest/NYC_NY8/llvm"}
RUN=${RUN:-}
SRC=${1:-blink.c}
OUT=$(basename "$SRC" .c)

$RUN "$NYC_LLVM/bin/clang.exe" --target=ny8 -mcpu=ny8a051j -Os -c "$SRC" -o "$OUT.o"
$RUN "$NYC_LLVM/bin/ld.lld.exe" -o "$OUT.elf" --gc-sections -L"$NYC_LLVM/lib" --discard-locals \
    --start-group -l:startup_ny8a051j.o -l:intvect_ny8a051j.o -lny814 "$OUT.o" --end-group \
    -T ny8a050e1.ld --relax --Map="$OUT.map"
$RUN "$NYC_LLVM/bin/llvm-objdump.exe" -d --triple=ny8 --mcpu=ny8a051j "$OUT.elf" > "$OUT.lst" 2>/dev/null || true
python3 ../tools/ny8_elf2h.py "$OUT.elf" ../ny8_image.h
