#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include "Diagnostics.hpp"

namespace {
struct LogState {
	SRWLOCK lock = SRWLOCK_INIT;
	bool enabled = false;
	HANDLE file = INVALID_HANDLE_VALUE;
	std::wstring path;
	~LogState() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }
};
LogState& State() { static LogState state; return state; }
const LONGLONG MAX_LOG_BYTES = 1024 * 1024;

bool OpenLog(LogState& s)
{
	if (s.path.empty()) {
		wchar_t path[32768];
		DWORD length = GetModuleFileNameW(NULL, path, _countof(path));
		if (!length || length >= _countof(path)) return false;
		s.path.assign(path, length);
		auto slash = s.path.find_last_of(L"\\/");
		if (slash == std::wstring::npos) return false;
		s.path.resize(slash + 1);
		s.path += L"DiscordMessenger-diagnostics.txt";
	}
	s.file = CreateFileW(s.path.c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	return s.file != INVALID_HANDLE_VALUE;
}
}

void SetDiagnosticsEnabled(bool enabled)
{
	auto& s = State();
	AcquireSRWLockExclusive(&s.lock);
	s.enabled = enabled;
	if (!enabled && s.file != INVALID_HANDLE_VALUE) {
		CloseHandle(s.file);
		s.file = INVALID_HANDLE_VALUE;
	}
	ReleaseSRWLockExclusive(&s.lock);
}

void DiagnosticLog(const char* format, ...)
{
	auto& s = State();
	AcquireSRWLockExclusive(&s.lock);
	if (!s.enabled) { ReleaseSRWLockExclusive(&s.lock); return; }
	if (s.file == INVALID_HANDLE_VALUE && !OpenLog(s)) {
		OutputDebugStringA("Discord Messenger: cannot write diagnostics beside the executable.\n");
		ReleaseSRWLockExclusive(&s.lock);
		return;
	}
	LARGE_INTEGER size;
	if (GetFileSizeEx(s.file, &size) && size.QuadPart >= MAX_LOG_BYTES) {
		CloseHandle(s.file);
		s.file = INVALID_HANDLE_VALUE;
		// Keep just one previous file. Do not grow the active file if rotation fails.
		if (!MoveFileExW(s.path.c_str(), (s.path + L".previous.txt").c_str(), MOVEFILE_REPLACE_EXISTING) || !OpenLog(s)) {
			ReleaseSRWLockExclusive(&s.lock);
			return;
		}
	}
	char message[2048] = {};
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	// One event per line even if a caller includes a newline.
	for (char* p = message; *p; ++p) if (*p == '\r' || *p == '\n') *p = ' ';
	SYSTEMTIME t;
	GetLocalTime(&t);
	char line[2304];
	int length = snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u.%03u [thread %lu] %s\r\n",
		t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
		(unsigned long)GetCurrentThreadId(), message);
	if (length > 0 && length < int(sizeof(line))) {
		DWORD written;
		WriteFile(s.file, line, DWORD(length), &written, NULL);
	}
	ReleaseSRWLockExclusive(&s.lock);
}
