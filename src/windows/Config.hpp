#pragma once

// This fork targets Windows 8.1 and later. Do not import Windows 10-only APIs.
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0603
#error Discord Messenger requires a Windows 8.1 (0x0603) or newer build target.
#endif

#define NEW_WINDOWS
