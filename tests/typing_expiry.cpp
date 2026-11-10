#include "state/TypingExpiry.hpp"
#include <cassert>
#include <limits>

int main()
{
	const uint64_t start = 1700000000000ULL;
	const uint64_t interval = 10000;
	assert(TypingTimeRemainingMs(start, start, interval) == interval);
	assert(TypingTimeRemainingMs(start, start + 9900, interval) == 100);
	assert(TypingTimeRemainingMs(start, start + interval, interval) == 0);
	assert(TypingTimeRemainingMs(start, start + interval + 1, interval) == 0);
	// A delayed UI timer, sleep/resume, and future timestamps must not wrap.
	assert(TypingTimeRemainingMs(start, start + 86400000, interval) == 0);
	assert(TypingTimeRemainingMs(start, start - 1, interval) == interval);
	const auto max = std::numeric_limits<uint64_t>::max();
	assert(TypingTimeRemainingMs(max - 5, max, interval) == interval - 5);
	assert(TypingTimeRemainingMs(start, start, 0) == 0);
}
