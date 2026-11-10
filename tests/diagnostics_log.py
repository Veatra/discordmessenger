#!/usr/bin/env python3
"""Exercise the real file logger with native filesystem/Win32 API doubles."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#pragma once
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
using DWORD=unsigned long;
using LONGLONG=long long;
using HANDLE=FILE*;
#define INVALID_HANDLE_VALUE nullptr
#define SRWLOCK_INIT {0}
#define FILE_APPEND_DATA 1
#define FILE_READ_ATTRIBUTES 2
#define FILE_SHARE_READ 0
#define OPEN_ALWAYS 0
#define FILE_ATTRIBUTE_NORMAL 0
#define MOVEFILE_REPLACE_EXISTING 0
#define _countof(x) (sizeof(x)/sizeof(x[0]))
struct SRWLOCK { int locked; };
struct LARGE_INTEGER { long long QuadPart; };
struct SYSTEMTIME { unsigned short wYear,wMonth,wDay,wHour,wMinute,wSecond,wMilliseconds; };
inline void AcquireSRWLockExclusive(SRWLOCK* l){assert(!l->locked);l->locked=1;}
inline void ReleaseSRWLockExclusive(SRWLOCK* l){assert(l->locked);l->locked=0;}
inline std::string narrow(const wchar_t* s){std::wstring w(s);return std::string(w.begin(),w.end());}
inline DWORD GetModuleFileNameW(void*,wchar_t* b,size_t cap){char cwd[4096];assert(getcwd(cwd,sizeof(cwd)));std::string p=std::string(cwd)+"/test.exe";assert(p.size()<cap);for(size_t i=0;i<=p.size();++i)b[i]=p[i];return p.size();}
inline HANDLE CreateFileW(const wchar_t* p,int,int,void*,int,int,void*){return fopen(narrow(p).c_str(),"ab+");}
inline void CloseHandle(HANDLE h){fclose(h);}
inline bool GetFileSizeEx(HANDLE h,LARGE_INTEGER* n){fflush(h);struct stat s;if(fstat(fileno(h),&s))return false;n->QuadPart=s.st_size;return true;}
inline bool MoveFileExW(const wchar_t* a,const wchar_t* b,int){return rename(narrow(a).c_str(),narrow(b).c_str())==0;}
inline void OutputDebugStringA(const char*){}
inline void GetLocalTime(SYSTEMTIME* t){*t={2026,11,10,12,0,0,1};}
inline DWORD GetCurrentThreadId(){return 42;}
inline bool WriteFile(HANDLE h,const char* s,DWORD n,DWORD* written,void*){*written=fwrite(s,1,n,h);fflush(h);return *written==n;}
'''
MAIN = r'''
#include "utils/Diagnostics.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
std::string read(const char* p){std::ifstream f(p);return std::string(std::istreambuf_iterator<char>(f),{});}
int main(){
 DiagnosticLog("disabled"); assert(read("DiscordMessenger-diagnostics.txt").empty());
 SetDiagnosticsEnabled(true); DiagnosticLog("event %d\nnext",7);
 auto text=read("DiscordMessenger-diagnostics.txt");
 assert(text.find("[thread 42] event 7 next")!=std::string::npos);
 SetDiagnosticsEnabled(false); DiagnosticLog("must not appear");
 assert(read("DiscordMessenger-diagnostics.txt")==text);
 SetDiagnosticsEnabled(true);
 std::string large(2000,'x');
 for(int i=0;i<600;++i)DiagnosticLog("%s",large.c_str());
 SetDiagnosticsEnabled(false);
 assert(!read("DiscordMessenger-diagnostics.txt.previous.txt").empty());
 assert(read("DiscordMessenger-diagnostics.txt").size()<1024*1024+2304);
}
'''
with tempfile.TemporaryDirectory(prefix="dm-log-test-") as tmp:
    path=Path(tmp)
    (path/'windows.h').write_text(STUB)
    (path/'test.cpp').write_text(MAIN)
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++11','-Wall','-Wextra',
                    '-I'+tmp,'-I'+str(ROOT/'src/core'),str(path/'test.cpp'),
                    str(ROOT/'src/core/utils/Diagnostics.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],cwd=tmp,check=True)
print('PASS: logging enable/disable, timestamps/thread IDs, single-line events, size rotation')
