#pragma once
#include <windows.h>

// App-owned windows only. Native menus, scroll state and window operations stay intact.
void ThemeChromeWindow(HWND window);

// Used by the creation hook to leave OS-dialog popup menus alone.
bool ThemeMenuPopupActive();

inline HMENU ThemeAppMenu(HWND window) { HMENU menu=GetMenu(window);return menu?menu:(HMENU)GetPropW(window,L"DMAppMenu"); }
