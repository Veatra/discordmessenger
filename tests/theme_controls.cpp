// Offline Win32 control smoke test. Does not load accounts or make requests.
// Links actual Theme.cpp. Classifier/drawing helpers are stubs; the icon transform uses a known black test icon.
#include "Theme.hpp"
#include <commctrl.h>
#include <cstdio>
#include <vector>
#include <cassert>
#include <cwchar>
#include <cstring>
#include <cstdlib>

// Palette/control fixture does not load any account, token, or network code.
bool IsIconMostlyBlack(HICON) { return true; }
void DrawIconInvert(HDC dc,HICON icon,int x,int y,int w,int h,bool) { DrawIconEx(dc,x,y,icon,w,h,0,NULL,DI_NORMAL); }
HWND listBox, tab;
HMENU fixtureMenu, fixturePopup;
bool dynamicMenuCalled=false;
UINT lastCommand=0, lastSystemCommand=0;
bool checkClose=false, checkKeyboard=false;
void PumpCommands() { MSG msg;while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);} }
void ClickCaption(HWND window,int hit,int x,int y) {
 SendMessage(window,WM_NCLBUTTONDOWN,hit,MAKELPARAM(x,y));assert(GetCapture()==window);
 POINT client={x,y};ScreenToClient(window,&client);
 SendMessage(window,WM_LBUTTONUP,0,MAKELPARAM(client.x,client.y));PumpCommands();
}
void CheckMenus(HWND window) {
 DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 for(int i=0;i<100;++i) {
  SendMessage(window,WM_INITMENUPOPUP,(WPARAM)fixturePopup,0);
  assert(dynamicMenuCalled);
  MENUITEMINFOW data={};data.cbSize=sizeof(data);data.fMask=MIIM_FTYPE|MIIM_DATA|MIIM_STATE;
  assert(GetMenuItemInfoW(fixturePopup,0,TRUE,&data));
  assert(bool(data.fType&MFT_OWNERDRAW)==IsDarkTheme());
  assert(!(data.fState&MFS_DISABLED));
  if(IsDarkTheme()) {
   MEASUREITEMSTRUCT measure={};measure.CtlType=ODT_MENU;measure.itemData=data.dwItemData;
   assert(SendMessage(window,WM_MEASUREITEM,0,(LPARAM)&measure));assert(measure.itemWidth>50 && measure.itemHeight>=27);
   assert(LOWORD(SendMessage(window,WM_MENUCHAR,L'o',(LPARAM)fixturePopup))==0);
  }
  SendMessage(window,WM_EXITMENULOOP,0,0);
  assert(GetMenuItemInfoW(fixturePopup,0,TRUE,&data));assert(!(data.fType&MFT_OWNERDRAW) && data.dwItemData==0);
 }
 assert(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=before+2);
}
BOOL CALLBACK CheckPopupBorder(HWND window,LPARAM found) {
 wchar_t name[32]={};GetClassNameW(window,name,32);
 if(wcscmp(name,L"#32768") || !IsWindowVisible(window))return TRUE;
 HDC dc=GetWindowDC(window);assert(GetPixel(dc,0,4)==ThemePalette::Border);ReleaseDC(window,dc);*(bool*)found=true;return TRUE;
}
void CheckIcon() {
 HDC dc=GetDC(NULL);BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=16;info.bmiHeader.biHeight=16;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void* pixels=NULL;HBITMAP color=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);assert(color);
 for(int i=0;i<256;++i)((uint32_t*)pixels)[i]=0xff000000;
 BYTE zeros[64]={};HBITMAP mask=CreateBitmap(16,16,1,1,zeros);assert(mask);
 ICONINFO iconInfo={};iconInfo.fIcon=TRUE;iconInfo.hbmColor=color;iconInfo.hbmMask=mask;
 HICON icon=CreateIconIndirect(&iconInfo);assert(icon);
 HIMAGELIST list=ImageList_Create(16,16,ILC_COLOR32|ILC_MASK,1,1);assert(list);
 int index=ThemeImageListAddIcon(list,icon);assert(index>=0);
 HDC target=CreateCompatibleDC(dc);HBITMAP result=CreateCompatibleBitmap(dc,16,16);auto old=SelectObject(target,result);
 RECT rect={0,0,16,16};FillRect(target,&rect,ThemeBrush(COLOR_WINDOW));ImageList_Draw(list,index,target,0,0,ILD_NORMAL);
 assert(GetPixel(target,8,8)==(IsDarkTheme() ? RGB(255,255,255) : RGB(0,0,0)));
 SelectObject(target,old);DeleteObject(result);DeleteDC(target);ImageList_Destroy(list);DestroyIcon(icon);DeleteObject(mask);DeleteObject(color);ReleaseDC(NULL,dc);
}
INT_PTR CALLBACK DialogProc(HWND,UINT message,WPARAM,LPARAM) { return message==WM_INITDIALOG; }
void CheckDialog(HINSTANCE instance) {
 alignas(DWORD) WORD buffer[(sizeof(DLGTEMPLATE)+6+sizeof(WORD)-1)/sizeof(WORD)]={};
 auto* layout=(DLGTEMPLATE*)buffer;
 layout->style=WS_POPUP|WS_CAPTION|DS_MODALFRAME;layout->cx=180;layout->cy=80;
 HWND dialog=CreateDialogIndirectParamW(instance,layout,NULL,DialogProc,0);assert(dialog);
 HDC dc=GetDC(dialog);
 HBRUSH brush=(HBRUSH)SendMessage(dialog,WM_CTLCOLORDLG,(WPARAM)dc,(LPARAM)dialog);
 LOGBRUSH info={};assert(GetObject(brush,sizeof(info),&info));
 assert(info.lbColor==ThemeColor(COLOR_3DFACE));
 ReleaseDC(dialog,dc);DestroyWindow(dialog);
}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l) {
 if(m==WM_COMMAND){lastCommand=LOWORD(w);return 0;}
 if(m==WM_SYSCOMMAND){lastSystemCommand=UINT(w)&0xfff0;if(checkClose && lastSystemCommand==SC_CLOSE)return 0;}
 if(m==WM_INITMENUPOPUP){EnableMenuItem((HMENU)w,100,MF_BYCOMMAND|MF_ENABLED);dynamicMenuCalled=true;return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 if(m==WM_TIMER){
  if(w==4){KillTimer(h,4);INPUT keys[4]={};for(auto& key:keys)key.type=INPUT_KEYBOARD;keys[0].ki.wVk=keys[3].ki.wVk=VK_MENU;keys[1].ki.wVk=keys[2].ki.wVk='F';keys[2].ki.dwFlags=keys[3].ki.dwFlags=KEYEVENTF_KEYUP;assert(SendInput(4,keys,sizeof(INPUT))==4);SetTimer(h,3,200,NULL);return 0;}
  if(w==3){KillTimer(h,3);INPUT keys[2]={};keys[0].type=keys[1].type=INPUT_KEYBOARD;keys[0].ki.wVk=keys[1].ki.wVk='O';keys[1].ki.dwFlags=KEYEVENTF_KEYUP;assert(SendInput(2,keys,sizeof(INPUT))==2);return 0;}
  if(w==2) { KillTimer(h,2);SendMessage(h,WM_CANCELMODE,0,0);return 0; }
  if(checkKeyboard)assert(lastCommand==100);
  else if(IsDarkTheme()){bool found=false;EnumThreadWindows(GetCurrentThreadId(),CheckPopupBorder,(LPARAM)&found);assert(found);}
  RedrawWindow(h,NULL,NULL,RDW_FRAME|RDW_INVALIDATE|RDW_UPDATENOW);
  RECT wr;GetWindowRect(h,&wr);
  HDC dc=GetDC(NULL), mem=CreateCompatibleDC(dc); RECT r;GetWindowRect(h,&r);r.right-=r.left;r.bottom-=r.top;
  HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bitmap);
  BitBlt(mem,0,0,r.right,r.bottom,dc,wr.left,wr.top,SRCCOPY);
  BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  std::vector<unsigned char> pixels(r.right*r.bottom*4);SelectObject(mem,old);assert(GetDIBits(dc,bitmap,0,r.bottom,pixels.data(),&info,DIB_RGB_COLORS)==r.bottom);
  BITMAPFILEHEADER file={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+pixels.size();
  FILE* f=fopen("theme-controls.bmp","wb");fwrite(&file,sizeof(file),1,f);fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,f);fwrite(pixels.data(),pixels.size(),1,f);fclose(f);
  DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(NULL,dc);DestroyWindow(h);return 0;
 }
 return DefWindowProc(h,m,w,l);
}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE,LPSTR args,int){
 _set_error_mode(_OUT_TO_STDERR);
 INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_WIN95_CLASSES};assert(InitCommonControlsEx(&ic));
 InitializeTheme(args[0]!='l');
 CheckDialog(inst);CheckIcon();
 WNDCLASSW wc={};wc.hInstance=inst;wc.lpszClassName=L"ThemeFixture";wc.lpfnWndProc=Proc;wc.hbrBackground=ThemeBrush(COLOR_3DFACE);wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassW(&wc);
 fixtureMenu=CreateMenu();fixturePopup=CreatePopupMenu();
 AppendMenuW(fixturePopup,MF_STRING|MF_GRAYED,100,L"&Open channel\tCtrl+O");AppendMenuW(fixturePopup,MF_SEPARATOR,0,NULL);
 AppendMenuW(fixturePopup,MF_STRING|MF_CHECKED,101,L"&Dark appearance");AppendMenuW(fixturePopup,MF_STRING|MF_GRAYED,102,L"Unavailable action");
 auto sub=CreatePopupMenu();AppendMenuW(sub,MF_STRING,103,L"Online");AppendMenuW(fixturePopup,MF_POPUP,(UINT_PTR)sub,L"&Status");
 AppendMenuW(fixtureMenu,MF_POPUP,(UINT_PTR)fixturePopup,L"&File");
 auto help=CreatePopupMenu();AppendMenuW(help,MF_STRING,104,L"&About Discord Messenger");AppendMenuW(fixtureMenu,MF_POPUP,(UINT_PTR)help,L"&Help");
 HWND h=CreateWindowW(wc.lpszClassName,L"Discord Messenger — Windows 8.1",WS_OVERLAPPEDWINDOW,40,40,820,640,NULL,fixtureMenu,inst,NULL);
 CheckMenus(h);
 auto make=[&](const wchar_t* c,const wchar_t* t,DWORD style,int x,int y,int w,int ht){HWND child=CreateWindowW(c,t,WS_CHILD|WS_VISIBLE|style,x,y,w,ht,h,NULL,inst,NULL);if(!child){FILE* err=fopen("fixture-errors.txt","a");fwprintf(err,L"Create failed: %ls, error %lu\n",c,GetLastError());fclose(err);ExitProcess(2);} SendMessage(child,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return child;};
 make(L"Static",L"Discord Messenger — dark appearance",0,20,18,740,26);
 make(L"Button",L"Connection and appearance",BS_GROUPBOX,16,52,400,200);
 make(L"Static",L"Message input",0,30,79,130,20);
 make(L"Edit",L"Readable text, links and emoji",WS_BORDER|ES_AUTOHSCROLL,30,103,360,30);
 auto check=make(L"Button",L"Dark appearance",BS_AUTOCHECKBOX|WS_TABSTOP,30,145,320,24);SendMessage(check,BM_SETCHECK,BST_CHECKED,0);assert(SendMessage(check,BM_GETCHECK,0,0)==BST_CHECKED);SendMessage(check,BM_CLICK,0,0);assert(SendMessage(check,BM_GETCHECK,0,0)==BST_UNCHECKED);SendMessage(check,BM_SETCHECK,BST_CHECKED,0);
 auto radio=make(L"Button",L"Cozy message layout",BS_AUTORADIOBUTTON|WS_TABSTOP,30,178,320,24);SendMessage(radio,BM_SETCHECK,BST_CHECKED,0);
 make(L"Button",L"Save settings",BS_DEFPUSHBUTTON|WS_TABSTOP,30,211,150,28);
 HWND disabled=make(L"Button",L"Disabled action",BS_PUSHBUTTON,200,211,185,28);EnableWindow(disabled,FALSE);
 HWND combo=make(L"ComboBox",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,25,278,360,150);SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Gradient message background");SendMessage(combo,CB_SETCURSEL,0,0);
 tab=make(WC_TABCONTROLW,L"",0,440,60,340,45);TCITEMW item={};item.mask=TCIF_TEXT;item.pszText=(LPWSTR)L"Appearance";TabCtrl_InsertItem(tab,0,&item);item.pszText=(LPWSTR)L"Connection";TabCtrl_InsertItem(tab,1,&item);
 listBox=make(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|WS_BORDER,440,130,340,230);
 LVCOLUMNW col={};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=(LPWSTR)L"Channel / member";col.cx=430;ListView_InsertColumn(listBox,0,&col);
 ListView_EnableGroupView(listBox,TRUE);
 LVGROUP group={};group.cbSize=sizeof(group);group.mask=LVGF_GROUPID|LVGF_HEADER;group.iGroupId=0;group.pszHeader=(LPWSTR)L"Online members";ListView_InsertGroup(listBox,-1,&group);
 LVITEMW row={};row.iGroupId=0;row.mask=LVIF_TEXT|LVIF_GROUPID;row.pszText=(LPWSTR)L"general — readable member names";ListView_InsertItem(listBox,&row);row.iItem=1;row.pszText=(LPWSTR)L"Selected channel";ListView_InsertItem(listBox,&row);ListView_SetItemState(listBox,1,LVIS_SELECTED,LVIS_SELECTED);
 for(int i=2;i<40;++i){wchar_t text[64];swprintf(text,64,L"Member %d — scrollable list",i);row.iItem=i;row.pszText=text;ListView_InsertItem(listBox,&row);}
 if(IsDarkTheme())assert(!(GetWindowLongPtr(listBox,GWL_STYLE)&WS_BORDER));
 HWND tree=make(WC_TREEVIEWW,L"",TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|WS_BORDER,25,335,360,150);TVINSERTSTRUCTW node={};node.hParent=TVI_ROOT;node.item.mask=TVIF_TEXT;node.item.pszText=(LPWSTR)L"Server channels";node.item.cChildren=1;node.item.mask|=TVIF_CHILDREN;auto root=TreeView_InsertItem(tree,&node);
 for(int i=0;i<40;++i){wchar_t text[64];swprintf(text,64,L"# channel-%d",i);node.hParent=root;node.item.mask=TVIF_TEXT;node.item.pszText=text;TreeView_InsertItem(tree,&node);}TreeView_Expand(tree,root,TVE_EXPAND);
 HWND progress=make(PROGRESS_CLASSW,L"",0,440,390,340,20);SendMessage(progress,PBM_SETPOS,60,0);
 make(STATUSCLASSNAMEW,L"Connected • typing indicator",0,0,510,800,24);
 ShowWindow(h,SW_SHOW);SetFocus(listBox);UpdateWindow(h);
 RECT beforeScroll={},afterScroll={};assert(ListView_GetItemRect(listBox,39,&beforeScroll,LVIR_BOUNDS));
 SendMessage(listBox,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);
 assert(ListView_GetItemRect(listBox,39,&afterScroll,LVIR_BOUNDS));assert(afterScroll.top<beforeScroll.top);
 ListView_EnsureVisible(listBox,0,FALSE);
 SendMessage(tree,WM_VSCROLL,SB_BOTTOM,0);assert(TreeView_GetFirstVisible(tree)!=root);
 SendMessage(tree,WM_VSCROLL,SB_TOP,0);assert(TreeView_GetFirstVisible(tree)==root);
 if(IsDarkTheme()) {
  RECT r;GetWindowRect(h,&r);POINT origin={0,0};ClientToScreen(h,&origin);MENUBARINFO bar={};bar.cbSize=sizeof(bar);GetMenuBarInfo(h,OBJID_MENU,0,&bar);
  int y=(r.top+bar.rcBar.top)/2;
  assert(SendMessage(h,WM_NCHITTEST,0,MAKELPARAM(r.right-22,y))==HTCLOSE);
  assert(SendMessage(h,WM_NCHITTEST,0,MAKELPARAM(r.right-66,y))==HTMAXBUTTON);
  assert(SendMessage(h,WM_NCHITTEST,0,MAKELPARAM(r.right-110,y))==HTMINBUTTON);
  // Exercise capture cancellation without posting a close/minimize command.
  SendMessage(h,WM_NCLBUTTONDOWN,HTCLOSE,MAKELPARAM(r.right-22,y));assert(GetCapture()==h);ReleaseCapture();
  EnableMenuItem(GetSystemMenu(h,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_GRAYED);
  SendMessage(h,WM_NCLBUTTONDOWN,HTCLOSE,MAKELPARAM(r.right-22,y));assert(GetCapture()!=h);
  EnableMenuItem(GetSystemMenu(h,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_ENABLED);
  checkClose=true;ClickCaption(h,HTCLOSE,r.right-22,y);assert(lastSystemCommand==SC_CLOSE);checkClose=false;
  ClickCaption(h,HTMINBUTTON,r.right-110,y);assert(lastSystemCommand==SC_MINIMIZE && IsIconic(h));ShowWindow(h,SW_RESTORE);PumpCommands();
  ClickCaption(h,HTMAXBUTTON,r.right-66,y);assert(lastSystemCommand==SC_MAXIMIZE && IsZoomed(h));SendMessage(h,WM_SYSCOMMAND,SC_RESTORE,0);PumpCommands();
  RECT restored;GetWindowRect(h,&restored);assert(restored.right-restored.left==r.right-r.left && restored.bottom-restored.top==r.bottom-r.top);
 }
 // Native Alt+F / mnemonic selection still sends the real command ID.
 SetForegroundWindow(h);checkKeyboard=!strstr(args,"popup");
 if(checkKeyboard){SetTimer(h,4,100,NULL);SetTimer(h,2,1000,NULL);}
 SetTimer(h,1,strstr(args,"popup") ? 500 : 1500,NULL);
 if(strstr(args,"popup")){SetTimer(h,2,1000,NULL);TrackPopupMenu(fixturePopup,TPM_LEFTALIGN,65,95,0,h,NULL);}
 MSG msg;while(GetMessage(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessage(&msg);}return 0;
}
