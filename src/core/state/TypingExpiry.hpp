#pragma once
#include <cstdint>

// Clamp future server timestamps and avoid unsigned subtraction wrapping after
// expiry. The caller supplies time so deadline behavior can be tested directly.
inline uint64_t TypingTimeRemainingMs(uint64_t startMs, uint64_t nowMs, uint64_t intervalMs)
{
	if (nowMs < startMs)
		return intervalMs;
	const uint64_t elapsed = nowMs - startMs;
	return elapsed >= intervalMs ? 0 : intervalMs - elapsed;
}
