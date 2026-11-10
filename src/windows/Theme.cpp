#include "Theme.hpp"
#include "WinUtils.hpp"
#include <commctrl.h>
#include <uxtheme.h>
#include <cwchar>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

namespace {
bool dark = false;
HINSTANCE instance;
HHOOK creationHook;
HBRUSH brushes[31] = {};
struct Resources {
    ~Resources() {
        if (creationHook) UnhookWindowsHookEx(creationHook);
        for (auto brush : brushes) if (brush) DeleteObject(brush);
    }
} resources;
const UINT_PTR SUBCLASS_ID = 0x444D5448;
enum Kind { Generic, Dialog, Button, Tab, Status, Header, Combo };
struct Control { Kind kind; bool hover = false; };

std::wstring Text(HWND window) {
    int length = GetWindowTextLengthW(window);
    std::vector<wchar_t> buffer(size_t(length) + 1);
    GetWindowTextW(window, buffer.data(), int(buffer.size()));
    return std::wstring(buffer.data());
}
void Fill(HDC dc, const RECT& rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
}
void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    auto old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL); LineTo(dc, x2, y2);
    SelectObject(dc, old); DeleteObject(pen);
}
void PaintButton(HWND window, HDC dc, Control* control) {
    RECT rect; GetClientRect(window, &rect);
    UINT style = UINT(GetWindowLongPtr(window, GWL_STYLE));
    UINT type = style & BS_TYPEMASK;
    bool enabled = IsWindowEnabled(window) != FALSE;
    bool focused = GetFocus() == window;
    UINT state = UINT(SendMessage(window, BM_GETSTATE, 0, 0));
    auto text = Text(window);
    Fill(dc, rect, ThemePalette::Panel);
    SetTextColor(dc, enabled ? ThemePalette::Text : ThemePalette::Muted);
    SetBkMode(dc, TRANSPARENT);
    if (type == BS_GROUPBOX) {
        RECT frame = rect; frame.top += 7;
        FrameRect(dc, &frame, ThemeBrush(COLOR_3DSHADOW));
        RECT caption = rect; caption.left += 9; caption.top = 0;
        RECT measure = caption;
        DrawTextW(dc, text.c_str(), -1, &measure, DT_CALCRECT | DT_SINGLELINE);
        caption.right = caption.left + measure.right - measure.left + 8;
        caption.bottom = measure.bottom;
        Fill(dc, caption, ThemePalette::Panel); caption.left += 4;
        DrawTextW(dc, text.c_str(), -1, &caption, DT_SINGLELINE);
        return;
    }
    const bool check = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
    const bool radio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
    if (check || radio) {
        RECT glyph = rect;
        int size = (std::min)(int(rect.bottom - rect.top - 2), GetSystemMetrics(SM_CXMENUCHECK));
        glyph.right = glyph.left + size;
        glyph.top = (rect.bottom - size) / 2; glyph.bottom = glyph.top + size;
        HPEN pen = CreatePen(PS_SOLID, 1, control->hover ? ThemePalette::Accent : ThemePalette::Border);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, ThemeBrush(COLOR_WINDOW));
        if (radio) Ellipse(dc, glyph.left, glyph.top, glyph.right, glyph.bottom);
        else Rectangle(dc, glyph.left, glyph.top, glyph.right, glyph.bottom);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);
        LRESULT value = SendMessage(window, BM_GETCHECK, 0, 0);
        if (value != BST_UNCHECKED) {
            InflateRect(&glyph, -3, -3);
            if (radio) {
                oldBrush = SelectObject(dc, ThemeBrush(COLOR_HOTLIGHT));
                oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                Ellipse(dc, glyph.left, glyph.top, glyph.right, glyph.bottom);
                SelectObject(dc, oldBrush); SelectObject(dc, oldPen);
            } else if (value == BST_INDETERMINATE) Fill(dc, glyph, ThemePalette::Accent);
            else {
                Line(dc, glyph.left, (glyph.top + glyph.bottom) / 2, (glyph.left + glyph.right) / 2, glyph.bottom, ThemePalette::Accent);
                Line(dc, (glyph.left + glyph.right) / 2, glyph.bottom, glyph.right, glyph.top, ThemePalette::Accent);
            }
        }
        rect.left += size + 6;
        DrawTextW(dc, text.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER);
    } else {
        Fill(dc, rect, state & BST_PUSHED ? ThemePalette::Selection : control->hover ? ThemePalette::Hover : ThemePalette::Panel);
        FrameRect(dc, &rect, ThemeBrush(focused || type == BS_DEFPUSHBUTTON ? COLOR_HOTLIGHT : COLOR_3DSHADOW));
        HICON icon = (HICON)SendMessage(window, BM_GETIMAGE, IMAGE_ICON, 0);
        if ((style & BS_ICON) && icon) {
            int size = GetSystemMetrics(SM_CXSMICON);
            DrawIconInvert(dc, icon, (rect.right-size)/2, (rect.bottom-size)/2, size, size, IsIconMostlyBlack(icon));
        } else {
            InflateRect(&rect, -4, -2);
            DrawTextW(dc, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }
    if (focused && !(SendMessage(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        InflateRect(&rect, -3, -3); DrawFocusRect(dc, &rect);
    }
}
void PaintTab(HWND window, HDC dc) {
    RECT rect; GetClientRect(window, &rect); Fill(dc, rect, ThemePalette::Panel);
    int count = TabCtrl_GetItemCount(window), selected = TabCtrl_GetCurSel(window);
    for (int i=0; i<count; ++i) {
        RECT item; TabCtrl_GetItemRect(window, i, &item);
        Fill(dc, item, i==selected ? ThemePalette::Hover : ThemePalette::Panel);
        SetTextColor(dc, i==selected ? ThemePalette::Text : ThemePalette::Muted);
        wchar_t caption[256] = {}; TCITEMW data = {}; data.mask = TCIF_TEXT;
        data.pszText = caption; data.cchTextMax = _countof(caption);
        SendMessageW(window, TCM_GETITEMW, i, (LPARAM)&data);
        DrawTextW(dc, caption, -1, &item, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (i==selected) {
            RECT underline = item; underline.top = underline.bottom - 2;
            Fill(dc, underline, ThemePalette::Accent);
            if (GetFocus()==window) { InflateRect(&item,-3,-3); DrawFocusRect(dc,&item); }
        }
    }
}
void PaintHeader(HWND window, HDC dc) {
    RECT rect; GetClientRect(window,&rect); Fill(dc,rect,ThemePalette::Panel);
    for (int i=0; i<Header_GetItemCount(window); ++i) {
        RECT item; Header_GetItemRect(window,i,&item);
        wchar_t text[256]={}; HDITEMW info={}; info.mask=HDI_TEXT; info.pszText=text; info.cchTextMax=_countof(text);
        SendMessageW(window,HDM_GETITEMW,i,(LPARAM)&info);
        Line(dc,item.right-1,item.top,item.right-1,item.bottom,ThemePalette::Border);
        item.left+=6; item.right-=4;
        DrawTextW(dc,text,-1,&item,DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }
}
void PaintStatus(HWND window, HDC dc) {
    RECT rect; GetClientRect(window,&rect); Fill(dc,rect,ThemePalette::Panel);
    int count = int(SendMessage(window,SB_GETPARTS,0,0));
    for (int i=0; i<count && i<64; ++i) {
        RECT part; SendMessage(window,SB_GETRECT,i,(LPARAM)&part);
        LRESULT length=SendMessage(window,SB_GETTEXTLENGTH,i,0);
        if (HIWORD(length) & SBT_OWNERDRAW) {
            DRAWITEMSTRUCT draw = {}; draw.CtlType=ODT_STATIC; draw.CtlID=GetDlgCtrlID(window);
            draw.itemID=i; draw.itemAction=ODA_DRAWENTIRE; draw.hwndItem=window; draw.hDC=dc; draw.rcItem=part;
            SendMessage(GetParent(window),WM_DRAWITEM,draw.CtlID,(LPARAM)&draw);
        } else {
            std::vector<wchar_t> text(size_t(LOWORD(length))+1);
            SendMessageW(window,SB_GETTEXTW,i,(LPARAM)text.data());
            part.left+=4; DrawTextW(dc,text.data(),-1,&part,DT_SINGLELINE|DT_VCENTER);
        }
    }
    Line(dc,0,0,rect.right,0,ThemePalette::Border);
}
void PaintCombo(HWND window,HDC dc) {
    RECT rect; GetClientRect(window,&rect); Fill(dc,rect,ThemePalette::Input);
    FrameRect(dc,&rect,ThemeBrush(COLOR_3DSHADOW));
    RECT caption=rect; caption.left+=6; caption.right-=24;
    auto text=Text(window); DrawTextW(dc,text.c_str(),-1,&caption,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    int x=rect.right-14, y=rect.bottom/2;
    Line(dc,x-4,y-2,x,y+2,ThemePalette::Muted); Line(dc,x,y+2,x+4,y-2,ThemePalette::Muted);
    if (GetFocus()==window) { InflateRect(&caption,-1,-3); DrawFocusRect(dc,&caption); }
}
LRESULT CALLBACK ThemeProc(HWND window, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    Control* control = (Control*)ref;
    switch (message) {
    case WM_NCDESTROY: {
        LRESULT result=DefSubclassProc(window,message,w,l);
        RemoveWindowSubclass(window,ThemeProc,id); delete control; return result;
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: case WM_CTLCOLORDLG: {
        wchar_t name[32]={}; GetClassNameW((HWND)l,name,_countof(name));
        bool input=message==WM_CTLCOLOREDIT || message==WM_CTLCOLORLISTBOX || _wcsicmp(name,L"Edit")==0;
        SetTextColor((HDC)w, IsWindowEnabled((HWND)l) ? ThemePalette::Text : ThemePalette::Muted);
        SetBkColor((HDC)w, input ? ThemePalette::Input : ThemePalette::Panel);
        return (LRESULT)ThemeBrush(input ? COLOR_APPWORKSPACE : COLOR_3DFACE);
    }
    case WM_NOTIFY: {
        NMHDR* info=(NMHDR*)l;
        if (info && info->code==NM_CUSTOMDRAW && SendMessage(info->hwndFrom,LVM_ISGROUPVIEWENABLED,0,0)) {
            wchar_t name[64]={}; GetClassNameW(info->hwndFrom,name,_countof(name));
            if (!_wcsicmp(name,WC_LISTVIEWW)) {
                NMLVCUSTOMDRAW* draw=(NMLVCUSTOMDRAW*)l;
                if (draw->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT && draw->dwItemType==LVCDI_GROUP) {
                    draw->clrText=ThemePalette::Text; draw->clrFace=ThemePalette::Canvas;
                    SetTextColor(draw->nmcd.hdc,ThemePalette::Text); return CDRF_NEWFONT;
                }
            }
        }
        break;
    }
    case WM_ERASEBKGND:
        if (control->kind!=Generic) { RECT r; GetClientRect(window,&r); Fill((HDC)w,r,ThemePalette::Panel); return 1; }
        break;
    case WM_MOUSEMOVE:
        if (control->kind==Button && !control->hover) {
            control->hover=true; TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0}; TrackMouseEvent(&track);
            InvalidateRect(window,NULL,FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        control->hover=false; InvalidateRect(window,NULL,FALSE); break;
    case WM_PRINTCLIENT: case WM_PAINT:
        if (control->kind==Button || control->kind==Tab || control->kind==Status || control->kind==Header || control->kind==Combo) {
            PAINTSTRUCT paint={}; HDC dc=message==WM_PAINT ? BeginPaint(window,&paint) : (HDC)w;
            int saved=SaveDC(dc);
            HFONT font=(HFONT)SendMessage(window,WM_GETFONT,0,0);
            SelectObject(dc,font ? font : GetStockObject(DEFAULT_GUI_FONT));
            SetBkMode(dc,TRANSPARENT);
            SetTextColor(dc,IsWindowEnabled(window) ? ThemePalette::Text : ThemePalette::Muted);
            switch(control->kind) {
            case Button: PaintButton(window,dc,control); break;
            case Tab: PaintTab(window,dc); break;
            case Status: PaintStatus(window,dc); break;
            case Header: PaintHeader(window,dc); break;
            case Combo: PaintCombo(window,dc); break;
            default: break;
            }
            RestoreDC(dc,saved);
            if (message==WM_PAINT) EndPaint(window,&paint);
            return 0;
        }
        break;
    }
    LRESULT result=DefSubclassProc(window,message,w,l);
    if (message==WM_ENABLE || message==WM_SETFOCUS || message==WM_KILLFOCUS || message==BM_SETCHECK || message==BM_SETSTATE ||
        message==WM_KEYUP || message==WM_LBUTTONUP || message==WM_LBUTTONDOWN || message==WM_SETTEXT || message==CB_SETCURSEL)
        InvalidateRect(window,NULL,FALSE);
    return result;
}
BOOL CALLBACK ThemeChild(HWND child, LPARAM) { ThemeWindow(child); return TRUE; }
LRESULT CALLBACK Created(int code, WPARAM w, LPARAM l) {
    if (code>=0) {
        CWPRETSTRUCT* event=(CWPRETSTRUCT*)l;
        if (event->message==WM_CREATE || event->message==WM_INITDIALOG) {
            HWND root=GetAncestor(event->hwnd,GA_ROOT);
            bool owned=(HINSTANCE)GetWindowLongPtr(root,GWLP_HINSTANCE)==instance;
            wchar_t name[32]={}; GetClassNameW(root,name,_countof(name));
            if (owned && !_wcsicmp(name,L"#32770")) {
                // The instance alone is not enough for native common dialogs.
                // Theme only dialogs whose procedure belongs to this executable.
                auto procedure=GetWindowLongPtr(root,DWLP_DLGPROC);
                HMODULE module=NULL;
                owned=procedure && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCWSTR)procedure,&module) && module==instance;
            }
            if (owned) {
                ThemeWindow(event->hwnd);
                if (event->message==WM_INITDIALOG)
                    EnumChildWindows(event->hwnd,ThemeChild,0);
            }
        }
    }
    return CallNextHookEx(creationHook,code,w,l);
}
}

void InitializeTheme(bool requested) {
    HIGHCONTRASTW contrast={sizeof(contrast),0,NULL};
    bool highContrast=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0) && (contrast.dwFlags&HCF_HIGHCONTRASTON);
    dark=requested && !highContrast;
    instance=GetModuleHandle(NULL);
    if (dark) {
        creationHook=SetWindowsHookExW(WH_CALLWNDPROCRET,Created,NULL,GetCurrentThreadId());
        if (!creationHook) dark=false; // Avoid a partially themed UI if hooking fails.
    }
}
bool IsDarkTheme() { return dark; }
COLORREF ThemeColor(int index) {
    if (!dark) return GetSysColor(index);
    switch(index) {
    case COLOR_WINDOW: return ThemePalette::Canvas;
    case COLOR_APPWORKSPACE: return ThemePalette::Input;
    case COLOR_WINDOWTEXT: case COLOR_BTNTEXT: case COLOR_MENUTEXT: case COLOR_CAPTIONTEXT: case COLOR_HIGHLIGHTTEXT: return ThemePalette::Text;
    case COLOR_GRAYTEXT: case COLOR_INACTIVECAPTIONTEXT: return ThemePalette::Muted;
    case COLOR_HIGHLIGHT: return ThemePalette::Selection;
    case COLOR_HOTLIGHT: return ThemePalette::Accent;
    case COLOR_3DSHADOW: case COLOR_3DDKSHADOW: case COLOR_WINDOWFRAME: return ThemePalette::Border;
    case COLOR_3DHIGHLIGHT: case COLOR_3DLIGHT: return ThemePalette::Hover;
    case COLOR_SCROLLBAR: return ThemePalette::Border;
    default: return ThemePalette::Panel;
    }
}
HBRUSH ThemeBrush(int index) {
    if (!dark || index<0 || index>=31) return GetSysColorBrush(index);
    if (!brushes[index]) brushes[index]=CreateSolidBrush(ThemeColor(index));
    return brushes[index];
}
COLORREF ThemeReadableColor(COLORREF color,COLORREF background) {
    if (!dark || color==CLR_NONE) return color;
    // Bound the cache and avoid repeated luminance/power calculations while painting.
    static std::unordered_map<uint64_t,COLORREF> cache;
    const uint64_t key=uint64_t(color) | (uint64_t(background)<<32);
    auto found=cache.find(key);
    if (found!=cache.end()) return found->second;
    COLORREF adjusted=ThemePalette::Readable(color,background);
    if (cache.size()<512) cache.emplace(key,adjusted);
    return adjusted;
}
BOOL ThemeDrawEdge(HDC dc, LPRECT rect, UINT edge, UINT flags) {
    if (!dark) return DrawEdge(dc,rect,edge,flags);
    if (flags & BF_MIDDLE) FillRect(dc,rect,ThemeBrush(COLOR_3DFACE));
    if (flags & BF_LEFT) Line(dc,rect->left,rect->top,rect->left,rect->bottom,ThemePalette::Border);
    if (flags & BF_RIGHT) Line(dc,rect->right-1,rect->top,rect->right-1,rect->bottom,ThemePalette::Border);
    if (flags & BF_TOP) Line(dc,rect->left,rect->top,rect->right,rect->top,ThemePalette::Border);
    if (flags & BF_BOTTOM) Line(dc,rect->left,rect->bottom-1,rect->right,rect->bottom-1,ThemePalette::Border);
    if (flags & BF_ADJUST) InflateRect(rect,-2,-2);
    return TRUE;
}
void ThemeWindow(HWND window) {
    if (!dark) return;
    DWORD_PTR existing;
    if (GetWindowSubclass(window,ThemeProc,SUBCLASS_ID,&existing)) return;
    wchar_t name[64]={}; GetClassNameW(window,name,_countof(name));
    Control* state=new Control;
    if (!_wcsicmp(name,L"#32770")) state->kind=Dialog;
    else if (!_wcsicmp(name,L"Button")) state->kind=Button;
    else if (!_wcsicmp(name,WC_TABCONTROLW)) state->kind=Tab;
    else if (!_wcsicmp(name,STATUSCLASSNAMEW)) state->kind=Status;
    else if (!_wcsicmp(name,WC_HEADERW)) state->kind=Header;
    else if (!_wcsicmp(name,L"ComboBox") && (GetWindowLongPtr(window,GWL_STYLE)&3)==CBS_DROPDOWNLIST) state->kind=Combo;
    else state->kind=Generic;
    // Existing owner-draw buttons retain their application's drawing contract.
    if (state->kind==Button && (GetWindowLongPtr(window,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW) state->kind=Generic;
    if (!SetWindowSubclass(window,ThemeProc,SUBCLASS_ID,(DWORD_PTR)state)) { delete state; return; }
    if (state->kind==Button && (GetWindowLongPtr(window,GWL_STYLE)&BS_TYPEMASK)==BS_GROUPBOX)
        SetWindowLongPtr(window,GWL_STYLE,GetWindowLongPtr(window,GWL_STYLE)|WS_CLIPSIBLINGS);
    SetWindowTheme(window,L"",L"");
    if (!_wcsicmp(name,WC_LISTVIEWW)) {
        ListView_SetBkColor(window,ThemePalette::Canvas);
        ListView_SetTextBkColor(window,ThemePalette::Canvas);
        ListView_SetTextColor(window,ThemePalette::Text);
    } else if (!_wcsicmp(name,WC_TREEVIEWW)) {
        TreeView_SetBkColor(window,ThemePalette::Canvas);
        TreeView_SetTextColor(window,ThemePalette::Text);
        TreeView_SetLineColor(window,ThemePalette::Border);
    } else if (!_wcsicmp(name,PROGRESS_CLASSW)) {
        SetWindowLongPtr(window,GWL_STYLE,GetWindowLongPtr(window,GWL_STYLE)|PBS_SMOOTH);
        SendMessage(window,PBM_SETBKCOLOR,0,ThemePalette::Panel);
        SendMessage(window,PBM_SETBARCOLOR,0,ThemePalette::Accent);
    } else if (!_wcsicmp(name,TOOLTIPS_CLASSW)) {
        SendMessage(window,TTM_SETTIPBKCOLOR,ThemePalette::Panel,0);
        SendMessage(window,TTM_SETTIPTEXTCOLOR,ThemePalette::Text,0);
    }
}

BOOL ThemeDrawIconEx(HDC dc, int x, int y, HICON icon, int width, int height, UINT step, HBRUSH background, UINT flags) {
    if (dark && icon && width>0 && height>0 && (flags&DI_IMAGE) && IsIconMostlyBlack(icon)) {
        DrawIconInvert(dc,icon,x,y,width,height,true);
        return TRUE;
    }
    return DrawIconEx(dc,x,y,icon,width,height,step,background,flags);
}

int ThemeImageListAddIcon(HIMAGELIST list, HICON icon) {
    HICON adjusted=NULL;
    ICONINFO info={};
    if (dark && icon && IsIconMostlyBlack(icon) && GetIconInfo(icon,&info)) {
        BITMAP bitmap={};
        if (info.hbmColor && GetObject(info.hbmColor,sizeof(bitmap),&bitmap) && bitmap.bmWidth>0 && bitmap.bmHeight>0) {
            BITMAPINFO format={};
            format.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            format.bmiHeader.biWidth=bitmap.bmWidth; format.bmiHeader.biHeight=bitmap.bmHeight;
            format.bmiHeader.biPlanes=1; format.bmiHeader.biBitCount=32; format.bmiHeader.biCompression=BI_RGB;
            std::vector<uint32_t> pixels(size_t(bitmap.bmWidth)*bitmap.bmHeight);
            HDC dc=GetDC(NULL);
            if (GetDIBits(dc,info.hbmColor,0,bitmap.bmHeight,pixels.data(),&format,DIB_RGB_COLORS)==bitmap.bmHeight) {
                bool alpha=false;
                for (auto pixel:pixels) if (pixel>>24) { alpha=true; break; }
                for (auto& pixel:pixels) {
                    unsigned opacity=alpha ? pixel>>24 : 255;
                    uint32_t inverted=pixel&0xff000000;
                    for (int channel=0;channel<3;++channel) {
                        unsigned value=(pixel>>(channel*8))&255;
                        inverted|=(opacity-(std::min)(opacity,value))<<(channel*8);
                    }
                    pixel=inverted;
                }
                if (SetDIBits(dc,info.hbmColor,0,bitmap.bmHeight,pixels.data(),&format,DIB_RGB_COLORS)==bitmap.bmHeight)
                    adjusted=CreateIconIndirect(&info);
            }
            ReleaseDC(NULL,dc);
        }
        if (info.hbmColor) DeleteObject(info.hbmColor);
        if (info.hbmMask) DeleteObject(info.hbmMask);
    }
    // Image lists copy the icon; only the temporary transformed icon is ours.
    int index=ImageList_ReplaceIcon(list,-1,adjusted ? adjusted : icon);
    if (adjusted) DestroyIcon(adjusted);
    return index;
}
