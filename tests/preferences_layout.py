#!/usr/bin/env python3
"""Compile/run the actual preferences layout with offline settings/account stubs.

Supply a matching MinGW compiler/windres and --runner wine (or a wrapper).
Exercises page navigation, small-window scrolling, focus visibility and resizing.
"""
import argparse
from pathlib import Path
import subprocess
import shutil

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--compiler',default='x86_64-w64-mingw32-g++')
parser.add_argument('--windres',default='x86_64-w64-mingw32-windres')
parser.add_argument('--runner',required=True)
parser.add_argument('--output',default='/tmp/dm-preferences-fixture')
a=parser.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'src/windows/OptionsDialog.cpp').read_text()
main=(ROOT/'src/windows/Main.cpp').read_text()
def function(signature,source=None):
 source=s if source is None else source
 start=source.index(signature);brace=source.index('{',start);level=1;i=brace+1
 while level:
  if source[i]=='{':level+=1
  elif source[i]=='}':level-=1
  i+=1
 return source[start:i]+'\n'
header=s[s.index('enum ePage'):s.index('DLGTEMPLATEEX* LockDialogResource')]
cpp=r'''
#include "Theme.hpp"
#include "resource.h"
#include <windowsx.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <tchar.h>
#include <map>
#include "DoubleBufferingHelper.hpp"
#include "ThemeChrome.hpp"
HINSTANCE g_hInstance;
__FONTS__
const int OPTIONS_RESULT_OK=1;
#define DMDI(x) (x)
int ScaleByDPI(int value){return value;}
bool IsIconMostlyBlack(HICON){return false;}
void DrawIconInvert(HDC dc,HICON icon,int x,int y,int w,int h,bool){DrawIconEx(dc,x,y,icon,w,h,0,NULL,DI_NORMAL);}
const TCHAR* TmGetTString(int id){
 switch(id){case IDS_ACCOUNT_PRIVACY:return L"Account & Privacy";case IDS_APPEARANCE:return L"Appearance";case IDS_NOTIFICATIONS:return L"Notifications";case IDS_CHAT:return L"Chat";case IDS_WINDOW:return L"Window";default:return L"Connection";}
}
INT_PTR OptionsHandleCommand(HWND,HWND,int,UINT,WPARAM,LPARAM){return 0;}
void OptionsInitPage(HWND page,int number){
 if(number!=1)return;
 const wchar_t* fonts[]={L"Noto Sans",L"Lato",L"Atkinson Hyperlegible"};
 for(auto name:fonts)SendDlgItemMessage(page,IDC_MESSAGE_FONT,CB_ADDSTRING,0,(LPARAM)name);
 SendDlgItemMessage(page,IDC_MESSAGE_FONT,CB_SETCURSEL,0,0);
 SendDlgItemMessage(page,IDC_MESSAGE_SIZE,CB_ADDSTRING,0,(LPARAM)L"14 px");SendDlgItemMessage(page,IDC_MESSAGE_SIZE,CB_SETCURSEL,0,0);
 SendDlgItemMessage(page,IDC_MESSAGE_STYLE,CB_ADDSTRING,0,(LPARAM)L"Flat color");SendDlgItemMessage(page,IDC_MESSAGE_STYLE,CB_SETCURSEL,0,0);
 SetDlgItemText(page,IDC_CHANNEL_HOVER,L"842A52");CheckDlgButton(page,IDC_DARK_MODE,BST_CHECKED);CheckDlgButton(page,IDC_ROUND_AVATARS,BST_CHECKED);
}
'''+header
import re
font_init=function('void InitializeFonts()',main)
font_names=sorted(set(re.findall(r'\bg_[A-Za-z0-9_]+',font_init))-{'g_hInstance'})
cpp=cpp.replace('__FONTS__','HFONT '+','.join(font_names)+';')
cpp+=r'''
struct TestSettings {std::string face="Noto Sans";int size=14;const std::string& MessageFont(){return face;}int MessageFontSize(){return size;}unsigned ChannelHover(){return 0x522a84;}} settings;
TestSettings* GetLocalSettings(){return &settings;}
int ScaleByUser(int value){return value;}
int GetSystemDPI(){return 96;}
LPTSTR ConvertCppStringToTString(const std::string& text){int length=MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,NULL,0);auto output=(wchar_t*)malloc(size_t(length)*sizeof(wchar_t));MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,output,length);return output;}
'''
cpp+=function('static int CALLBACK MessageFontFound',main)+font_init+function('HFONT ChatAuthorFont(',main)
for name in ['DLGTEMPLATEEX* LockDialogResource','void WINAPI OnChildDialogInit','static void ScrollSettingsPage','INT_PTR CALLBACK OptionsChildDialogProc','void OnSelChanged','static void LayoutPreferences','HRESULT OnPreferenceDialogInit','static INT_PTR CALLBACK DialogProc']:
 cpp+=function(name)
member=(ROOT/'src/windows/MemberList.cpp').read_text()
cpp+=r'''
using Snowflake=unsigned long long;
struct TestRole {int m_colorOriginal=0;};
struct TestGuild {std::map<Snowflake,TestRole> m_roles;} guild;
using Guild=TestGuild;
struct Channel {enum {CATEGORY, TEXT};int m_channelType=TEXT;};
std::map<Snowflake,Channel> channels;
struct TestDiscord {TestGuild* GetGuild(Snowflake){return &guild;}Channel* GetChannel(Snowflake id){return &channels[id];}void OnSelectChannel(Snowflake){}} discord;
TestDiscord* GetDiscordInstance(){return &discord;}
class MemberList {public:HWND m_listHwnd=NULL;int m_hotItem=-1;Snowflake m_guild=1;std::vector<Snowflake> m_groups;void PaintGroupHeaders(HDC);static LRESULT CALLBACK ListWndProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);};
'''
channel=(ROOT/'src/windows/ChannelView.cpp').read_text()
cpp+=r'''
class ChannelView {public:
 struct Item {wchar_t str[256];Snowflake m_snowflake;HTREEITEM m_hItem;};
 HWND m_treeHwnd=NULL;HIMAGELIST m_treeImages=NULL;std::vector<Item> m_channels;Snowflake m_currentChannel=0;int m_nCategoryCollapseIcon=0,m_nCategoryExpandIcon=0;
 void SetItemIcon(HTREEITEM,int){}bool OnNotifyTree(LRESULT&,WPARAM,LPARAM);
};
ChannelView* activeTree=NULL;
'''
cpp+=function('bool ChannelView::OnNotifyTree(',channel)
cpp+=function('void MemberList::PaintGroupHeaders(',member)+function('LRESULT CALLBACK MemberList::ListWndProc(',member)
cpp+=r'''
int rosterDraws=0;
LRESULT CALLBACK Roster(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_NOTIFY && activeTree){auto event=(NMHDR*)l;if(event->hwndFrom==activeTree->m_treeHwnd){LRESULT result=0;if(activeTree->OnNotifyTree(result,w,l))return result;}}
 if(message==WM_MEASUREITEM){((MEASUREITEMSTRUCT*)l)->itemHeight=54;return TRUE;}
 if(message==WM_DRAWITEM){auto item=(DRAWITEMSTRUCT*)l;FillRect(item->hDC,&item->rcItem,ThemeBrush(COLOR_WINDOW));SetTextColor(item->hDC,ThemeColor(COLOR_WINDOWTEXT));SetBkMode(item->hDC,TRANSPARENT);RECT r=item->rcItem;r.left+=10;DrawTextW(item->hDC,L"Readable member",-1,&r,DT_SINGLELINE|DT_VCENTER);++rosterDraws;return TRUE;}
 return DefWindowProc(window,message,w,l);
}
void CheckRosterPaint(){
 WNDCLASSW type={};type.hInstance=g_hInstance;type.lpfnWndProc=Roster;type.lpszClassName=L"RosterFixture";type.hbrBackground=ThemeBrush(COLOR_WINDOW);RegisterClassW(&type);
 HWND window=CreateWindowW(type.lpszClassName,L"Role header fixture",WS_OVERLAPPEDWINDOW,10,10,280,440,NULL,NULL,g_hInstance,NULL);
 MemberList list;list.m_groups={7};guild.m_roles[7].m_colorOriginal=0x52c9c4;
 list.m_listHwnd=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_NOCOLUMNHEADER|LVS_OWNERDRAWFIXED,0,0,260,390,window,(HMENU)1,g_hInstance,NULL);
 LVCOLUMNW column={};column.mask=LVCF_WIDTH;column.cx=240;ListView_InsertColumn(list.m_listHwnd,0,&column);
 ListView_EnableGroupView(list.m_listHwnd,TRUE);LVGROUP group={};group.cbSize=sizeof(group);group.mask=LVGF_HEADER|LVGF_GROUPID;group.iGroupId=0;group.pszHeader=(LPWSTR)L"Moderators (24)";ListView_InsertGroup(list.m_listHwnd,-1,&group);
 for(int i=0;i<24;++i){LVITEMW row={};row.mask=LVIF_TEXT|LVIF_GROUPID;row.iItem=i;row.iGroupId=0;row.pszText=(LPWSTR)L"Member";ListView_InsertItem(list.m_listHwnd,&row);}
 SetWindowSubclass(list.m_listHwnd,MemberList::ListWndProc,0x444d4d42,(DWORD_PTR)&list);
 ShowWindow(window,SW_SHOW);rosterDraws=0;RedrawWindow(list.m_listHwnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ERASE);assert(rosterDraws>0);

 ChannelView tree;activeTree=&tree;SetWindowPos(window,NULL,10,10,540,440,SWP_NOZORDER);
 tree.m_treeHwnd=CreateWindowW(WC_TREEVIEWW,L"",WS_CHILD|WS_VISIBLE|TVS_FULLROWSELECT|TVS_SHOWSELALWAYS,270,0,240,380,window,(HMENU)2,g_hInstance,NULL);
 SetWindowFont(tree.m_treeHwnd,g_ChannelFont,TRUE);TreeView_SetItemHeight(tree.m_treeHwnd,22);TreeView_SetBkColor(tree.m_treeHwnd,RGB(18,18,20));
 tree.m_treeImages=ImageList_Create(16,16,ILC_COLOR32|ILC_MASK,1,1);ImageList_AddIcon(tree.m_treeImages,LoadIcon(NULL,IDI_APPLICATION));TreeView_SetImageList(tree.m_treeHwnd,tree.m_treeImages,TVSIL_NORMAL);
 HTREEITEM parent=TVI_ROOT;
 for(int i=0;i<4;++i){bool category=i%2==0;ChannelView::Item data={};swprintf(data.str,256,category?L"Category %d":L"Channel %d",i);data.m_snowflake=100+i;channels[data.m_snowflake].m_channelType=category?Channel::CATEGORY:Channel::TEXT;
 TVINSERTSTRUCTW node={};node.hParent=category?TVI_ROOT:parent;node.hInsertAfter=TVI_LAST;node.item.mask=TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE;node.item.pszText=data.str;node.item.lParam=i;node.item.iImage=node.item.iSelectedImage=category?I_IMAGENONE:0;
 data.m_hItem=TreeView_InsertItem(tree.m_treeHwnd,&node);tree.m_channels.push_back(data);if(category)parent=data.m_hItem;
 if(i==2){TVITEMEX item={};item.hItem=data.m_hItem;item.mask=TVIF_INTEGRAL;item.iIntegral=2;SendMessage(tree.m_treeHwnd,TVM_SETITEM,0,(LPARAM)&item);}
 if(!category)TreeView_Expand(tree.m_treeHwnd,parent,TVE_EXPAND);
 }
 TreeView_SelectItem(tree.m_treeHwnd,tree.m_channels[1].m_hItem);RedrawWindow(tree.m_treeHwnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
 if(IsDarkTheme()){
 HDC dc=GetDC(tree.m_treeHwnd);RECT selected;TreeView_GetItemRect(tree.m_treeHwnd,tree.m_channels[1].m_hItem,&selected,FALSE);
 assert(GetPixel(dc,200,(selected.top+selected.bottom)/2)==RGB(53,54,60));
 RECT category;TreeView_GetItemRect(tree.m_treeHwnd,tree.m_channels[2].m_hItem,&category,FALSE);
 bool text=false;for(int y=category.top;y<category.top+44;++y)for(int x=6;x<180;++x){auto color=GetPixel(dc,x,y);if(GetRValue(color)>80 && GetGValue(color)>80 && GetBValue(color)>80){assert(y>=category.top+22);text=true;}}
 assert(text);ReleaseDC(tree.m_treeHwnd,dc);
 }
 activeTree=NULL;TreeView_SetImageList(tree.m_treeHwnd,NULL,TVSIL_NORMAL);ImageList_Destroy(tree.m_treeImages);
 // Verify the shared header painter even on Wine versions without group rectangles.
 HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,220,30);auto old=SelectObject(dc,bitmap);
 RECT r={0,0,220,30};COLORREF color=RGB(82,201,196);ThemeDrawSectionHeader(dc,r,L"Moderators (24)",color,g_CategoryFont);assert(GetPixel(dc,5,15)==ThemeReadableColor(color,ThemeColor(COLOR_WINDOW)));
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);DestroyWindow(window);
}
'''
cpp+=r'''
void Capture(HWND window){
 RECT r;GetWindowRect(window,&r);int w=r.right-r.left,h=r.bottom-r.top;HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);
 HBITMAP bitmap=CreateCompatibleBitmap(screen,w,h);auto old=SelectObject(dc,bitmap);BitBlt(dc,0,0,w,h,screen,r.left,r.top,SRCCOPY);SelectObject(dc,old);
 BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 std::vector<BYTE> pixels(size_t(w)*h*4);assert(GetDIBits(screen,bitmap,0,h,pixels.data(),&info,DIB_RGB_COLORS));
 BITMAPFILEHEADER file={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+pixels.size();
 FILE* stream=fopen("preferences.bmp","wb");assert(stream);fwrite(&file,sizeof(file),1,stream);fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,stream);fwrite(pixels.data(),pixels.size(),1,stream);fclose(stream);
 DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);
}
INT_PTR CALLBACK Fixture(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==WM_TIMER){
  auto data=(DialogHeader*)GetWindowLongPtr(window,GWLP_USERDATA);static int step=0;
  if(step<C_PAGES){SendMessage(data->hwndNav,LB_SETCURSEL,step,0);SendMessage(window,WM_COMMAND,MAKEWPARAM(IDC_OPTIONS_NAV,LBN_SELCHANGE),(LPARAM)data->hwndNav);assert(data->pageNum==step);assert(IsWindow(data->hwndDisplay));++step;return TRUE;}
  if(step==C_PAGES){SendMessage(data->hwndNav,LB_SETCURSEL,PG_APPEARANCE,0);OnSelChanged(window);RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);Capture(window);SetWindowPos(window,NULL,0,0,500,380,SWP_NOMOVE|SWP_NOZORDER);++step;return TRUE;}
  if(step==C_PAGES+1){
   SCROLLINFO si={};si.cbSize=sizeof(si);si.fMask=SIF_ALL;GetScrollInfo(data->hwndDisplay,SB_VERT,&si);assert(si.nMax>int(si.nPage));
   ScrollSettingsPage(data->hwndDisplay,SB_VERT,si.nMax);GetScrollInfo(data->hwndDisplay,SB_VERT,&si);assert(si.nPos>0);
   ScrollSettingsPage(data->hwndDisplay,SB_VERT,0);HWND field=GetDlgItem(data->hwndDisplay,IDC_ROUND_AVATARS);SetFocus(field);
   RECT focused,viewport;GetWindowRect(field,&focused);MapWindowPoints(NULL,data->hwndDisplay,(POINT*)&focused,2);GetClientRect(data->hwndDisplay,&viewport);assert(focused.top>=0 && focused.bottom<=viewport.bottom);
   SetWindowPos(window,NULL,0,0,900,800,SWP_NOMOVE|SWP_NOZORDER);++step;return TRUE;
  }
  if(step==C_PAGES+2){SCROLLINFO si={};si.cbSize=sizeof(si);si.fMask=SIF_POS;GetScrollInfo(data->hwndDisplay,SB_VERT,&si);assert(si.nPos==0);SendMessage(window,WM_COMMAND,IDC_PREFS_DONE,0);return TRUE;}
 }
 INT_PTR result=DialogProc(window,message,w,l);
 if(message==WM_INITDIALOG)SetTimer(window,1,100,NULL);
 return result;
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR args,int){
 _set_error_mode(_OUT_TO_STDERR);g_hInstance=instance;INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);
 for(int id:{IDR_NOTO_FONT,IDR_ATKINSON_FONT,IDR_LATO_FONT}){auto r=FindResource(instance,MAKEINTRESOURCE(id),RT_RCDATA);assert(r);DWORD count=0;assert(AddFontMemResourceEx(LockResource(LoadResource(instance,r)),SizeofResource(instance,r),NULL,&count));}
 for(int size:{12,24}){settings.size=size;InitializeFonts();LOGFONT lf={};
  assert(GetObject(g_ChatTextFont,sizeof(lf),&lf) && lf.lfHeight==-size);
  assert(GetObject(g_ChatAuthorFont,sizeof(lf),&lf) && lf.lfHeight==-16 && !_wcsicmp(lf.lfFaceName,L"Noto Sans"));
  assert(GetObject(g_GuildCaptionFont,sizeof(lf),&lf) && lf.lfHeight==-16 && !_wcsicmp(lf.lfFaceName,L"Noto Sans"));
  assert(GetObject(g_ChannelFont,sizeof(lf),&lf) && lf.lfHeight==-13);
  assert(GetObject(ChatAuthorFont(L"\U0001d4d4na",false),sizeof(lf),&lf) && lf.lfHeight==-18);
 }
 settings.face="Lato";settings.size=20;InitializeFonts();LOGFONT font={};
 assert(GetObject(g_ChatTextFont,sizeof(font),&font) && !_wcsicmp(font.lfFaceName,L"Lato") && font.lfHeight==-20);
 assert(GetObject(g_ChatAuthorFont,sizeof(font),&font) && !_wcsicmp(font.lfFaceName,L"Noto Sans") && font.lfHeight==-16);
 assert(GetObject(g_GuildCaptionFont,sizeof(font),&font) && !_wcsicmp(font.lfFaceName,L"Noto Sans"));
 settings.face="Noto Sans";settings.size=14;InitializeFonts();
 InitializeTheme(args[0]!='l');CheckRosterPaint();assert(DialogBoxParam(instance,MAKEINTRESOURCE(IDD_DIALOG_OPTIONS),NULL,Fixture,0)==OPTIONS_RESULT_OK);
 DeleteObject(g_MessageTextFont);DeleteObject(g_CategoryFont);DeleteObject(g_GuildCaptionFont);return 0;
}
'''
(out/'preferences.cpp').write_text(cpp)
subprocess.run([a.windres,'-Ihacks','-i','src/resource.rc','-o',str(out/'resource.o')],cwd=ROOT,check=True)
subprocess.run([a.compiler,'-std=c++11','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0603','-DWINVER=0x0603','-DNOMINMAX','-Isrc','-Isrc/windows','-Isrc/core','-Ideps','-Ideps/mwas/include',str(out/'preferences.cpp'),'src/windows/Theme.cpp','src/windows/ThemeChrome.cpp','src/windows/DoubleBufferingHelper.cpp',str(out/'resource.o'),'-o',str(out/'preferences.exe'),'-mwindows','-lcomctl32','-luxtheme','-ldwmapi','-lmsimg32','-lgdi32','-static-libgcc','-static-libstdc++'],cwd=ROOT,check=True)
runtime=subprocess.check_output([a.compiler,'-print-file-name=libwinpthread-1.dll'],text=True).strip()
if Path(runtime).is_file():shutil.copy2(runtime,out/'libwinpthread-1.dll')
for mode in ['dark','light']:
 subprocess.run([a.runner,str(out/'preferences.exe'),mode],cwd=out,check=True,timeout=40)
 (out/'preferences.bmp').rename(out/f'preferences-{mode}.bmp')
print('PASS: actual preferences navigation, all page resources, small-window scrolling and resize restoration')
