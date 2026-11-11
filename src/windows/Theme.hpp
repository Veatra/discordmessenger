#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "ThemePalette.hpp"

// Initialize once after loading settings, before creating application windows.
// Uses only documented Windows 8.1 APIs. Never changes OS-wide system colors.
void InitializeTheme(bool dark);
bool IsDarkTheme();
COLORREF ThemeColor(int systemIndex);
HBRUSH ThemeBrush(int systemIndex);
COLORREF ThemeReadableColor(COLORREF foreground, COLORREF background);
BOOL ThemeDrawEdge(HDC dc, LPRECT rect, UINT edge, UINT flags);
void ThemeWindow(HWND window);

BOOL ThemeDrawIconEx(HDC dc, int x, int y, HICON icon, int width, int height, UINT step, HBRUSH background, UINT flags);

int ThemeImageListAddIcon(HIMAGELIST list, HICON icon);

// Caller saves/restores the DC. Handles partial-paint buffer viewport offsets.
void ThemeClipAvatar(HDC dc, const RECT& rect, bool rounded);
