"""Test the actual clipboard bitmap readback with deterministic GDI doubles."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'src/windows/WinUtils.cpp').read_text()
start = source.index('bool GetDataFromBitmap(')
end = source.index('\nstd::string CutOutURLPath', start)
program = r'''
#include <cassert>
#include <cstdint>
#include <climits>
#include <cstddef>
#include <new>
using HDC=int; using HBITMAP=int; using BYTE=unsigned char;
struct BITMAP { int bmWidth, bmHeight; };
struct Header { size_t biSize; int biWidth,biHeight,biPlanes,biBitCount,biCompression; };
struct BITMAPINFO { Header bmiHeader; };
#define BI_RGB 0
#define DIB_RGB_COLORS 0
static int imageWidth=2, imageHeight=2;
static bool objectOK=true, readOK=true;
int GetObject(int, size_t, BITMAP* b) { *b={imageWidth,imageHeight}; return objectOK; }
int GetDIBits(int,int,int,int height,BYTE* out,BITMAPINFO* info,int) {
    assert(info->bmiHeader.biHeight == -height);
    assert(info->bmiHeader.biBitCount == 32); // no palette buffer required
    if (!readOK) return height-1;
    // GDI's top-down output: red first row, blue second row (BGRA pixels).
    for(int x=0;x<imageWidth;++x) {
        out[x*4]=0; out[x*4+1]=0; out[x*4+2]=255; out[x*4+3]=0;
        out[(imageWidth+x)*4]=255; out[(imageWidth+x)*4+1]=0;
        out[(imageWidth+x)*4+2]=0; out[(imageWidth+x)*4+3]=0;
    }
    return height;
}
'''
program += source[start:end]
program += r'''
int main() {
    BYTE* pixels=nullptr; int width=0,height=0,bpp=0;
    assert(GetDataFromBitmap(1,1,pixels,width,height,bpp));
    assert(width==2 && height==2 && bpp==32);
    assert(pixels[2]==255 && pixels[8]==255 && pixels[10]==0);
    delete[] pixels;
    readOK=false;
    assert(!GetDataFromBitmap(1,1,pixels,width,height,bpp) && pixels==nullptr);
    readOK=true; objectOK=false;
    assert(!GetDataFromBitmap(1,1,pixels,width,height,bpp) && pixels==nullptr);
    objectOK=true; imageWidth=INT_MAX;
    assert(!GetDataFromBitmap(1,1,pixels,width,height,bpp));
    imageWidth=0;
    assert(!GetDataFromBitmap(1,1,pixels,width,height,bpp));
    imageWidth=2; imageHeight=0;
    assert(!GetDataFromBitmap(1,1,pixels,width,height,bpp));
}
'''
with tempfile.TemporaryDirectory(prefix='dm-bitmap-test-') as tmp:
    p = Path(tmp)
    (p/'test.cpp').write_text(program)
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++11','-Wall','-Wextra',
                    str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: top-down bitmap orientation, 32-bit pixels, failed reads and invalid dimensions')
