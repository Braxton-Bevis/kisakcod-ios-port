#!/usr/bin/env bash
# preflight-lint.sh <src-file>... — advisory iOS-portability scan, runnable on
# any seat (no Apple toolchain needed). Run BEFORE adding a TU to the census;
# it catches the failure classes CI has already taught us:
#   run 29277434085: LittleShort (WIN32-block endian macros)
#   run 29280246035: _time64 (MSVC CRT spelling absent from the shim)
# Warn-only by design (exit 1 just signals findings): a hit means "resolve or
# consciously justify before pushing", not "provably broken".
set -uo pipefail
[ $# -ge 1 ] || { echo "usage: $0 <src-file>..." >&2; exit 2; }

SHIM=src/ios/msvc_crt_compat.h
findings=0

for f in "$@"; do
  [ -f "$f" ] || { echo "SKIP (missing): $f"; continue; }

  # 1. MSVC underscore-CRT calls not covered by the shim.
  while IFS= read -r ident; do
    if ! grep -q "\b${ident}\b" "$SHIM" 2>/dev/null; then
      line=$(grep -n "\b${ident}\s*(" "$f" | head -1)
      echo "MSVC-CRT not in shim: ${ident}  [$f:${line%%:*}]"
      findings=1
    fi
  done < <(grep -oE '\b_[a-z][a-z0-9]*(64)?\s*\(' "$f" \
            | sed 's/\s*($//; s/($//' | sed 's/(.*//' | sort -u \
            | grep -vE '^_(_)?(asm|declspec|cdecl|stdcall|fastcall)$')

  # 2. Inline assembly.
  grep -nE '(^|[^\w])__asm([^\w]|$)' "$f" | head -3 | sed "s|^|__asm site: $f:|" \
    && findings=1

  # 3. WIN32-block-only endian macros (defined for KISAK_IOS since 6c6fe43,
  #    but flag new Big*/Swap uses that may need thought).
  grep -nE '\b(BigShort|BigLong|ShortSwap|LongSwap)\s*\(' "$f" | head -3 \
    | sed "s|^|endian swap use: $f:|" && findings=1

  # 4. Direct Win32 API/header reach not routed through the port layers.
  grep -nE '#include\s*[<"](windows|winsock|direct|io)\.h' "$f" | head -3 \
    | sed "s|^|raw win32 include: $f:|" && findings=1
  grep -nE '\b(GetTickCount|QueryPerformance(Counter|Frequency)|CreateThread|WSA[A-Z]\w+|GetModuleHandle|LoadLibrary)\s*\(' "$f" \
    | head -5 | sed "s|^|win32 API call: $f:|" && findings=1

  # 5. setjmp storage (arm64 jmp_buf ≈ 192B vs MSVC x86 64B — sized buffers
  #    corrupt silently; see DEPENDENCY_MAP §3).
  grep -nE '\bjmp_buf\b' "$f" | head -3 | sed "s|^|jmp_buf site: $f:|" \
    && findings=1
done

if [ "$findings" -eq 0 ]; then
  echo "preflight: no known-class findings"
  exit 0
fi
exit 1
