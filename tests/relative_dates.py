#!/usr/bin/env python3
"""Exercise the production relative-date formatter across calendar/DST boundaries."""
from pathlib import Path
import subprocess
import tempfile
source=Path('src/core/utils/Util.cpp').read_text()
body=source[source.index('std::string FormatTimeLong('):source.index('std::string FormatTimeShort(')].replace('::time(NULL)','clockNow')
harness=r'''
#include <ctime>
#include <string>
#include <cassert>
#include <cstdlib>
#include <cstdio>
time_t clockNow;
struct Frontend {
 std::string GetTodayAtText(){return "%H:%M";}
 std::string GetYesterdayAtText(){return "Yesterday at %H:%M";}
 std::string GetFormatTimeLongText(){return "%Y-%m-%d %H:%M";}
} frontend;
Frontend* GetFrontend(){return &frontend;}
time_t Stamp(int y,int m,int d,int h=12,int min=0){tm t={};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=min;t.tm_isdst=-1;return mktime(&t);}
'''+body+r'''
int main(){
 setenv("TZ","Europe/Berlin",1);tzset();
 clockNow=Stamp(2026,11,11);
 assert(FormatTimeLong(Stamp(2026,11,11,0,38),true)=="00:38");
 assert(FormatTimeLong(Stamp(2026,11,10,23,23),true)=="Yesterday at 23:23");
 assert(FormatTimeLong(Stamp(2026,11,9,23,23),true)=="2026-11-09 23:23");
 assert(FormatTimeLong(Stamp(2026,11,11,0,38),false)=="2026-11-11 00:38");
 clockNow=Stamp(2026,1,1);assert(FormatTimeLong(Stamp(2025,12,31,23,59),true)=="Yesterday at 23:59");
 clockNow=Stamp(2026,3,30,0,5);assert(FormatTimeLong(Stamp(2026,3,29,0,1),true)=="Yesterday at 00:01");
 clockNow=Stamp(2026,10,26,0,5);assert(FormatTimeLong(Stamp(2026,10,25,0,1),true)=="Yesterday at 00:01");
 puts("PASS: production date formatter, today/yesterday, year rollover and DST");
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'dates.cpp').write_text(harness)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra',str(p/'dates.cpp'),'-o',str(p/'dates')],check=True)
 subprocess.run([str(p/'dates')],check=True)
