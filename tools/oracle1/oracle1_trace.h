// BMK4 Oracle 1 trace writer — tool-owned.
#pragma once

#include <cstddef>
#include <cstdint>

namespace bmk4or1
{
// Opens the trace file (binary, truncate) and writes the schema line.
// Returns false on I/O failure.
bool TraceOpen(const char *path, bool emitNames);

// Flushes and closes; safe to call twice.
void TraceClose();

bool TraceEmitNames();

// printf-style single-line event write; appends '\n' and flushes so that
// engine-refusal exits keep every event emitted before the failure.
void TraceLine(const char *fmt, ...);

// FNV-1a64 over `size` bytes (fixture-manifest convention when the caller
// includes the terminating NUL).
std::uint64_t Fnv1a64(const void *bytes, std::size_t size);

// Driver-side events (not engine hooks).
void EmitZoneOpen(const char *basename, unsigned long long fileBytes);
void EmitZoneLoaded(const char *basename);
} // namespace bmk4or1
