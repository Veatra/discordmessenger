#include "Theme.hpp"
#include "ThemeChrome.hpp"
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <cwctype>
#include <memory>
#include <vector>
#include <string>

namespace {
const UINT_PTR ChromeId = 0x444D4348;
HMENU openingMenu = NULL; // Same UI thread as the creation hook/menu loop.
struct Fonts {
    HFONT menu = NULL, caption = NULL;
    int dpi = 96;
    Fonts() {
        HDC dc = GetDC(NULL); dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(NULL, dc);
        NONCLIENTMETRICSW metrics = {}; metrics.cbSize = sizeof(metrics);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
            menu = CreateFontIndirectW(&metrics.lfMenuFont);
            caption = CreateFontIndirectW(&metrics.lfCaptionFont);
        }
    }
    ~Fonts() { if (menu) DeleteObject(menu); if (caption) DeleteObject(caption); }
};
Fonts& Font() { static Fonts fonts; return fonts; }
int Scale(int value) { return MulDiv(value, Font().dpi, 96); }
void Fill(HDC dc, const RECT& r, COLORREF color) {
    SetDCBrushColor(dc, color); FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
}
void Line(HDC dc, int x, int y, int xx, int yy, COLORREF color) {
    SetDCPenColor(dc, color); auto old = SelectObject(dc, GetStockObject(DC_PEN));
    MoveToEx(dc, x, y, NULL); LineTo(dc, xx, yy); SelectObject(dc, old);
}
struct MenuItem {
    HMENU menu;
    UINT position, originalType;
    ULONG_PTR originalData;
    std::wstring text;
    bool bar;
};
struct Chrome {
    bool caption = false;
    bool popup = false;
    HMENU popupMenu = NULL;
    bool active = true;
    int hover = HTNOWHERE, pressed = HTNOWHERE;
    std::vector<std::unique_ptr<MenuItem>> items;
};
MenuItem* FindItem(Chrome* state, ULONG_PTR data) {
    for (auto& item : state->items) if ((ULONG_PTR)item.get() == data) return item.get();
    return NULL;
}
void RestoreMenus(Chrome* state, bool all) {
    for (auto it = state->items.begin(); it != state->items.end();) {
        auto& item = **it;
        if (!all && item.bar) { ++it; continue; }
        MENUITEMINFOW current = {}; current.cbSize = sizeof(current); current.fMask = MIIM_DATA;
        if (IsMenu(item.menu) && GetMenuItemInfoW(item.menu, item.position, TRUE, &current) && current.dwItemData == (ULONG_PTR)&item) {
            current.fMask = MIIM_FTYPE | MIIM_DATA;
            current.fType = item.originalType; current.dwItemData = item.originalData;
            SetMenuItemInfoW(item.menu, item.position, TRUE, &current);
        }
        it = state->items.erase(it);
    }
}
void SkinMenu(Chrome* state, HMENU menu, bool bar) {
    if (!menu) return;
    MENUINFO background = {}; background.cbSize = sizeof(background); background.fMask = MIM_BACKGROUND;
    background.hbrBack = ThemeBrush(COLOR_3DFACE); SetMenuInfo(menu, &background);
    for (int index = 0; index < GetMenuItemCount(menu); ++index) {
        MENUITEMINFOW data = {}; data.cbSize = sizeof(data); data.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING | MIIM_BITMAP;
        if (!GetMenuItemInfoW(menu, index, TRUE, &data)) continue;
        // Preserve existing app owner-draw/image menu contracts (emoji, avatars, etc.).
        if ((data.fType & (MFT_OWNERDRAW | MFT_BITMAP)) || data.hbmpItem) continue;
        std::vector<wchar_t> text(size_t(data.cch) + 1);
        data.dwTypeData = text.data(); data.cch = UINT(text.size());
        GetMenuItemInfoW(menu, index, TRUE, &data);
        std::unique_ptr<MenuItem> item(new MenuItem{menu, UINT(index), data.fType, data.dwItemData, text.data(), bar});
        data.fMask = MIIM_FTYPE | MIIM_DATA;
        data.fType |= MFT_OWNERDRAW; data.dwItemData = (ULONG_PTR)item.get();
        if (SetMenuItemInfoW(menu, index, TRUE, &data)) state->items.push_back(std::move(item));
    }
}
bool MenuMeasure(Chrome* state, MEASUREITEMSTRUCT* measure) {
    if (!measure || measure->CtlType != ODT_MENU) return false;
    auto item = FindItem(state, measure->itemData); if (!item) return false;
    HDC dc = GetDC(NULL); auto font = SelectObject(dc, Font().menu ? Font().menu : GetStockObject(DEFAULT_GUI_FONT));
    SIZE size = {}; GetTextExtentPoint32W(dc, item->text.c_str(), int(item->text.size()), &size);
    SelectObject(dc, font); ReleaseDC(NULL, dc);
    measure->itemHeight = item->originalType & MFT_SEPARATOR ? Scale(9) : (std::max)(Scale(27), int(size.cy) + Scale(8));
    measure->itemWidth = size.cx + Scale(item->bar ? 18 : 64);
    return true;
}
bool MenuDraw(Chrome* state, DRAWITEMSTRUCT* draw) {
    if (!draw || draw->CtlType != ODT_MENU) return false;
    auto item = FindItem(state, draw->itemData); if (!item) return false;
    HDC dc = draw->hDC; int saved = SaveDC(dc); RECT r = draw->rcItem;
    bool selected = (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
    Fill(dc, r, selected ? ThemePalette::Hover : ThemePalette::Panel);
    if (item->originalType & MFT_SEPARATOR) {
        Line(dc, r.left + Scale(12), (r.top+r.bottom)/2, r.right - Scale(12), (r.top+r.bottom)/2, ThemePalette::Border);
    } else {
        SelectObject(dc, Font().menu ? Font().menu : GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, draw->itemState & ODS_DISABLED ? ThemePalette::Muted : ThemePalette::Text);
        RECT text = r; text.left += Scale(item->bar ? 9 : 30); text.right -= Scale(item->bar ? 9 : 24);
        UINT flags = DT_SINGLELINE | DT_VCENTER;
        if (draw->itemState & ODS_NOACCEL) flags |= DT_HIDEPREFIX;
        auto tab = item->text.find(L'\t');
        DrawTextW(dc, item->text.c_str(), int(tab == std::wstring::npos ? item->text.size() : tab), &text, flags);
        if (tab != std::wstring::npos) DrawTextW(dc, item->text.c_str()+tab+1, -1, &text, flags | DT_RIGHT);
        int middle = (r.top+r.bottom)/2;
        if (draw->itemState & ODS_CHECKED) {
            if (item->originalType & MFT_RADIOCHECK) {
                RECT dot = {r.left+Scale(12), middle-Scale(2), r.left+Scale(17), middle+Scale(3)}; Fill(dc, dot, ThemePalette::Accent);
            } else {
                Line(dc, r.left+Scale(10), middle, r.left+Scale(14), middle+Scale(4), ThemePalette::Accent);
                Line(dc, r.left+Scale(14), middle+Scale(4), r.left+Scale(21), middle-Scale(4), ThemePalette::Accent);
            }
        }
        if (!item->bar && GetSubMenu(item->menu, item->position)) {
            Line(dc, r.right-Scale(13), middle-Scale(4), r.right-Scale(9), middle, ThemePalette::Muted);
            Line(dc, r.right-Scale(9), middle, r.right-Scale(13), middle+Scale(4), ThemePalette::Muted);
        }
    }
    RestoreDC(dc, saved); return true;
}
LRESULT MenuChar(Chrome* state, HMENU menu, wchar_t key) {
    std::vector<UINT> matches;
    int selected = -1;
    for (auto& item : state->items) {
        if (item->menu != menu) continue;
        UINT flags = GetMenuState(menu, item->position, MF_BYPOSITION);
        if (flags & MF_HILITE) selected = int(item->position);
        if (flags & (MF_DISABLED | MF_GRAYED | MF_SEPARATOR)) continue;
        for (size_t i=0; i+1<item->text.size(); ++i) if (item->text[i] == L'&') {
            if (item->text[i+1] == L'&') { ++i; continue; }
            if (towupper(item->text[i+1]) == towupper(key)) matches.push_back(item->position);
            break;
        }
    }
    if (matches.empty()) return MAKELRESULT(0, MNC_IGNORE);
    auto match = std::find_if(matches.begin(), matches.end(), [selected](UINT p) { return int(p)>selected; });
    return MAKELRESULT(match==matches.end() ? matches.front() : *match, matches.size()==1 ? MNC_EXECUTE : MNC_SELECT);
}

struct Frame {
    RECT window, client, title, icon, close, maximize, minimize;
};
Frame Geometry(HWND window) {
    Frame f = {}; GetWindowRect(window, &f.window);
    RECT client; GetClientRect(window, &client);
    POINT origin = {0, 0}; ClientToScreen(window, &origin);
    f.client = {origin.x-f.window.left, origin.y-f.window.top, origin.x-f.window.left+client.right, origin.y-f.window.top+client.bottom};
    f.title = {0, 0, f.window.right-f.window.left, f.client.top};
    MENUBARINFO bar = {}; bar.cbSize = sizeof(bar);
    if (GetMenu(window) && GetMenuBarInfo(window, OBJID_MENU, 0, &bar)) f.title.bottom = bar.rcBar.top-f.window.top;
    // Use the native frame metrics/layout so menu keyboard tracking, accessibility,
    // work-area maximization and resizing keep their OS contracts on Windows 8.1.
    int edge = IsZoomed(window) ? f.client.left : 1;
    f.title.left = edge; f.title.right -= edge;
    f.title.top = IsZoomed(window) ? GetSystemMetrics(SM_CYFRAME)+GetSystemMetrics(SM_CXPADDEDBORDER) : 1;
    int width = Scale(44), height = f.title.bottom-f.title.top;
    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
    if (style & WS_SYSMENU) f.close = {f.title.right-width, f.title.top, f.title.right, f.title.bottom};
    bool controls = f.close.right && (style & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
    if (controls) {
        f.maximize = f.close; OffsetRect(&f.maximize, -width, 0);
        f.minimize = f.maximize; OffsetRect(&f.minimize, -width, 0);
    }
    int icon = (std::min)(Scale(16), height-Scale(4));
    if (style & WS_SYSMENU) f.icon = {f.title.left+Scale(10), f.title.top+(height-icon)/2, f.title.left+Scale(10)+icon, f.title.top+(height+icon)/2};
    return f;
}
bool CaptionButtonEnabled(HWND window, int hit) {
    LONG_PTR style=GetWindowLongPtr(window,GWL_STYLE);
    if (!IsWindowEnabled(window)) return false;
    if (hit==HTCLOSE) {
        if (!(style&WS_SYSMENU) || (GetClassLongPtr(window,GCL_STYLE)&CS_NOCLOSE)) return false;
        UINT flags=GetMenuState(GetSystemMenu(window,FALSE),SC_CLOSE,MF_BYCOMMAND);
        return flags!=UINT(-1) && !(flags&(MF_DISABLED|MF_GRAYED));
    }
    return (style & (hit==HTMINBUTTON ? WS_MINIMIZEBOX : WS_MAXIMIZEBOX))!=0;
}
void PaintFrame(HWND window, Chrome* state) {
    if (IsIconic(window)) return;
    Frame f = Geometry(window); HDC dc = GetWindowDC(window); if (!dc) return;
    int saved = SaveDC(dc);
    ExcludeClipRect(dc, f.client.left, f.client.top, f.client.right, f.client.bottom);
    RECT all = {0, 0, f.window.right-f.window.left, f.window.bottom-f.window.top};
    // Leave the native owner-drawn menu row alone; it handles hover/mnemonics.
    RECT left = {0, 0, f.client.left, all.bottom}, right = {f.client.right, 0, all.right, all.bottom};
    RECT bottom = {0, f.client.bottom, all.right, all.bottom};
    Fill(dc, left, ThemePalette::Canvas); Fill(dc, right, ThemePalette::Canvas); Fill(dc, bottom, ThemePalette::Canvas);
    Fill(dc, f.title, ThemePalette::Input);
    // Native menu bars can leave the unused part of their row in COLOR_MENU.
    // Paint only that spare space; Windows still draws/tracks all menu items.
    if (GetMenu(window)) {
        RECT spare = {f.client.left, f.title.bottom, f.client.right, f.client.top};
        RECT item;
        for (int i=0; i<GetMenuItemCount(GetMenu(window)); ++i)
            if (GetMenuItemRect(window, GetMenu(window), i, &item))
                spare.left=(std::max)(spare.left, item.right-f.window.left);
        Fill(dc, spare, ThemePalette::Panel);
    }
    FrameRect(dc, &all, ThemeBrush(COLOR_3DFACE));
    if (f.icon.right>f.icon.left) {
        HICON icon = (HICON)SendMessage(window, WM_GETICON, ICON_SMALL, 0);
        if (!icon) icon = (HICON)GetClassLongPtr(window, GCLP_HICONSM);
        if (!icon) icon = (HICON)GetClassLongPtr(window, GCLP_HICON);
        if (icon) DrawIconEx(dc, f.icon.left, f.icon.top, icon, f.icon.right-f.icon.left, f.icon.bottom-f.icon.top, 0, NULL, DI_NORMAL);
    }
    RECT text = f.title; text.left = f.icon.right+Scale(9); text.right = (f.minimize.right ? f.minimize.left : f.close.right ? f.close.left : f.title.right)-Scale(12);
    wchar_t title[1024] = {}; GetWindowTextW(window, title, _countof(title));
    SelectObject(dc, Font().caption ? Font().caption : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, state->active ? ThemePalette::Text : ThemePalette::Muted);
    DrawTextW(dc, title, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    const RECT buttons[] = {f.minimize, f.maximize, f.close};
    const int hits[] = {HTMINBUTTON, HTMAXBUTTON, HTCLOSE};
    for (int i=0; i<3; ++i) {
        const RECT& r = buttons[i]; if (!r.right) continue;
        bool enabled = CaptionButtonEnabled(window,hits[i]);
        if (enabled && state->hover==hits[i]) Fill(dc, r, i==2 ? RGB(180,49,60) : ThemePalette::Hover);
        int x = (r.left+r.right)/2, y = (r.top+r.bottom)/2, s = Scale(5);
        COLORREF color = enabled ? ThemePalette::Text : ThemePalette::Border;
        if (i==0) Line(dc, x-s, y+1, x+s+1, y+1, color);
        if (i==1) {
            if (IsZoomed(window)) {
                RECT back = {x-s+Scale(2), y-s, x+s+1, y+s-Scale(2)};
                FrameRect(dc, &back, ThemeBrush(COLOR_WINDOWTEXT));
                Fill(dc, RECT{x-s, y-s+Scale(2), x+s-Scale(2), y+s+1}, ThemePalette::Input);
            }
            Line(dc,x-s,y-s+Scale(2),x+s-Scale(2),y-s+Scale(2),color);
            Line(dc,x-s,y-s+Scale(2),x-s,y+s+1,color);
            Line(dc,x-s,y+s,x+s-Scale(2),y+s,color);
            Line(dc,x+s-Scale(2),y-s+Scale(2),x+s-Scale(2),y+s+1,color);
        }
        if (i==2) { Line(dc,x-s,y-s,x+s+1,y+s+1,color); Line(dc,x-s,y+s,x+s+1,y-s-1,color); }
    }
    RestoreDC(dc, saved); ReleaseDC(window, dc);
}
int FrameHit(HWND window, LPARAM l) {
    Frame f = Geometry(window); POINT point = {GET_X_LPARAM(l)-f.window.left, GET_Y_LPARAM(l)-f.window.top};
    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
    if (PtInRect(&f.close, point)) return HTCLOSE;
    if ((style & WS_MAXIMIZEBOX) && PtInRect(&f.maximize, point)) return HTMAXBUTTON;
    if ((style & WS_MINIMIZEBOX) && PtInRect(&f.minimize, point)) return HTMINBUTTON;
    if (PtInRect(&f.icon, point)) return HTSYSMENU;
    // Keep the top few pixels available for resizing.
    if (PtInRect(&f.title, point) && (IsZoomed(window) || point.y>=Scale(5))) return HTCAPTION;
    return HTNOWHERE;
}
void PaintPopup(HWND window, Chrome* state) {
    if (!openingMenu || !IsMenu(state->popupMenu)) return;
    HDC dc=GetWindowDC(window); if (!dc) return;
    RECT bounds; GetWindowRect(window,&bounds);
    RECT frame={0,0,bounds.right-bounds.left,bounds.bottom-bounds.top};
    FrameRect(dc,&frame,ThemeBrush(COLOR_3DSHADOW)); InflateRect(&frame,-1,-1);
    FrameRect(dc,&frame,ThemeBrush(COLOR_3DFACE));
    // Windows adds its own classic submenu glyph after WM_DRAWITEM. Replace
    // that small gutter after native painting, preserving native popup input.
    if (IsMenu(state->popupMenu)) for (int i=0;i<GetMenuItemCount(state->popupMenu);++i) {
        if (!GetSubMenu(state->popupMenu,i)) continue;
        RECT row;
        if (!GetMenuItemRect(NULL,state->popupMenu,i,&row) || row.left<bounds.left || row.right>bounds.right || row.top<bounds.top || row.bottom>bounds.bottom) continue;
        OffsetRect(&row,-bounds.left,-bounds.top);
        UINT flags=GetMenuState(state->popupMenu,i,MF_BYPOSITION);
        row.left=row.right-Scale(20);
        Fill(dc,row,flags&MF_HILITE ? ThemePalette::Hover : ThemePalette::Panel);
        int x=row.right-Scale(9), y=(row.top+row.bottom)/2;
        COLORREF color=flags&(MF_GRAYED|MF_DISABLED) ? ThemePalette::Muted : ThemePalette::Text;
        Line(dc,x-Scale(3),y-Scale(4),x+1,y,color);
        Line(dc,x,y,x-Scale(4),y+Scale(4),color);
    }
    ReleaseDC(window,dc);
}
void PaintScrollbars(HWND window) {
    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
    if (!(style & (WS_VSCROLL | WS_HSCROLL)) || IsIconic(window)) return;
    HDC dc = GetWindowDC(window); if (!dc) return;
    RECT bounds; GetWindowRect(window, &bounds);
    RECT gutters[2] = {}; bool visible[2] = {};
    for (int axis=0; axis<2; ++axis) {
        if (!(style & (axis ? WS_HSCROLL : WS_VSCROLL))) continue;
        SCROLLBARINFO bar = {}; bar.cbSize = sizeof(bar);
        if (!GetScrollBarInfo(window, axis ? OBJID_HSCROLL : OBJID_VSCROLL, &bar) ||
            (bar.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN))) continue;
        RECT gutter = bar.rcScrollBar; OffsetRect(&gutter, -bounds.left, -bounds.top);
        gutters[axis]=gutter; visible[axis]=true;
        Fill(dc, gutter, ThemePalette::Canvas);
        if (bar.rgstate[0] & STATE_SYSTEM_UNAVAILABLE) continue;
        RECT thumb = gutter;
        if (axis) {
            thumb.left += bar.xyThumbTop; thumb.right = gutter.left+bar.xyThumbBottom;
            int middle = (gutter.top+gutter.bottom)/2;
            thumb.top = middle-Scale(2); thumb.bottom = middle+Scale(2);
        } else {
            thumb.top += bar.xyThumbTop; thumb.bottom = gutter.top+bar.xyThumbBottom;
            int middle = (gutter.left+gutter.right)/2;
            thumb.left = middle-Scale(2); thumb.right = middle+Scale(2);
        }
        if (thumb.right>thumb.left && thumb.bottom>thumb.top) Fill(dc, thumb, ThemePalette::ScrollThumb);
    }
    if (visible[0] && visible[1])
        Fill(dc, RECT{gutters[0].left,gutters[1].top,gutters[0].right,gutters[1].bottom}, ThemePalette::Canvas);
    ReleaseDC(window, dc);
}
BOOL CALLBACK AttachMenuPopup(HWND window, LPARAM) {
    wchar_t name[32]={};GetClassNameW(window,name,_countof(name));
    if (!wcscmp(name,L"#32768")) ThemeChromeWindow(window);
    return TRUE;
}
LRESULT CALLBACK ChromeProc(HWND window, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    Chrome* state = (Chrome*)ref;
    switch (message) {
    case WM_SHOWWINDOW:
        if (state->popup && w) state->popupMenu=openingMenu;
        break;
    case WM_NCDESTROY: {
        RestoreMenus(state, true);
        LRESULT result = DefSubclassProc(window, message, w, l);
        RemoveWindowSubclass(window, ChromeProc, id); delete state; return result;
    }
    case WM_INITMENUPOPUP: {
        LRESULT result = DefSubclassProc(window, message, w, l); // App updates enabled/check state first.
        openingMenu=(HMENU)w;
        SkinMenu(state, (HMENU)w, false);
        // USER32 can create its menu window before notifying the owner. Cover
        // existing windows here; the creation hook covers windows created later.
        EnumThreadWindows(GetCurrentThreadId(),AttachMenuPopup,0);
        return result;
    }
    case WM_EXITMENULOOP: RestoreMenus(state, false); openingMenu=NULL; break;
    case WM_MEASUREITEM: if (MenuMeasure(state, (MEASUREITEMSTRUCT*)l)) return TRUE; break;
    case WM_DRAWITEM: if (MenuDraw(state, (DRAWITEMSTRUCT*)l)) return TRUE; break;
    case WM_MENUCHAR:
        if (!state->items.empty()) {
            LRESULT match=MenuChar(state,(HMENU)l,LOWORD(w));
            if (HIWORD(match)!=MNC_IGNORE) return match;
        }
        break;
    case WM_NCHITTEST:
        if (state->caption) {
            int hit = FrameHit(window, l);
            LRESULT native = DefSubclassProc(window,message,w,l);
            // Retain the native resize grip, except under the caption buttons.
            if (hit!=HTCLOSE && hit!=HTMINBUTTON && hit!=HTMAXBUTTON &&
                native>=HTLEFT && native<=HTBOTTOMRIGHT) return native;
            if (hit!=HTNOWHERE) return hit;
            return native;
        }
        break;
    case WM_NCMOUSEMOVE:
        if (state->caption) {
            int hit = FrameHit(window, l);
            if (state->hover!=hit) { state->hover=hit; PaintFrame(window,state); }
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE | TME_NONCLIENT, window, 0}; TrackMouseEvent(&track);
        }
        break;
    case WM_NCMOUSELEAVE:
        if (state->caption && state->hover!=HTNOWHERE) { state->hover=HTNOWHERE; PaintFrame(window,state); }
        break;
    case WM_NCLBUTTONDOWN: case WM_NCLBUTTONDBLCLK:
        if (state->caption && (w==HTCLOSE || w==HTMINBUTTON || w==HTMAXBUTTON)) {
            if (CaptionButtonEnabled(window,int(w))) { state->pressed=int(w); SetCapture(window); }
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (state->caption && state->pressed!=HTNOWHERE) {
            POINT point={GET_X_LPARAM(l),GET_Y_LPARAM(l)}; ClientToScreen(window,&point);
            int hit=FrameHit(window,MAKELPARAM(point.x,point.y));
            if (hit!=state->hover) { state->hover=hit; PaintFrame(window,state); }
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (state->caption && state->pressed!=HTNOWHERE) {
            int pressed=state->pressed; state->pressed=HTNOWHERE;
            POINT point={GET_X_LPARAM(l),GET_Y_LPARAM(l)}; ClientToScreen(window,&point);
            bool inside=FrameHit(window,MAKELPARAM(point.x,point.y))==pressed && CaptionButtonEnabled(window,pressed);
            ReleaseCapture();
            if (inside) PostMessage(window,WM_SYSCOMMAND,pressed==HTCLOSE ? SC_CLOSE : pressed==HTMINBUTTON ? SC_MINIMIZE : IsZoomed(window) ? SC_RESTORE : SC_MAXIMIZE,0);
            return 0;
        }
        break;
    case WM_CAPTURECHANGED: case WM_CANCELMODE:
        state->pressed=HTNOWHERE; state->hover=HTNOWHERE;
        if (state->caption) PaintFrame(window,state);
        break;
    }
    if (message==WM_NCACTIVATE) state->active=w!=0;
    LRESULT result = DefSubclassProc(window, message, w, l);
    if (message==WM_NCPAINT || message==WM_NCACTIVATE || message==WM_SETTEXT || message==WM_SIZE || message==WM_STYLECHANGED || message==WM_ENABLE) {
        if (state->caption) PaintFrame(window,state);
        PaintScrollbars(window);
    } else if (message==WM_VSCROLL || message==WM_HSCROLL || message==WM_MOUSEWHEEL || message==WM_MOUSEHWHEEL || message==WM_PAINT || message==WM_KEYDOWN || message==WM_LBUTTONUP) {
        PaintScrollbars(window);
    }
    if (state->popup && (message==WM_PAINT || message==WM_NCPAINT || message==WM_PRINT || message==WM_PRINTCLIENT || message==WM_MOUSEMOVE || message==WM_KEYDOWN)) PaintPopup(window,state);
    return result;
}
}

void ThemeChromeWindow(HWND window) {
    DWORD_PTR existing;
    if (!IsDarkTheme() || GetWindowSubclass(window, ChromeProc, ChromeId, &existing)) return;
    std::unique_ptr<Chrome> state(new Chrome);
    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
    wchar_t name[32]={};GetClassNameW(window,name,_countof(name));
    state->popup=!wcscmp(name,L"#32768"); state->popupMenu=openingMenu;
    state->caption = !(style & WS_CHILD) && (style & WS_CAPTION)==WS_CAPTION;
    if (!SetWindowSubclass(window, ChromeProc, ChromeId, (DWORD_PTR)state.get())) return;
    if (state->caption) {
        // Documented Vista+ API, independent of Windows 10 immersive-dark attributes.
        DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
        DwmSetWindowAttribute(window, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
        SkinMenu(state.get(), GetMenu(window), true);
        if (GetMenu(window)) DrawMenuBar(window);
        SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    if (state->popup) RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_FRAME);
    state.release();
}

bool ThemeMenuPopupActive() { return openingMenu!=NULL; }
