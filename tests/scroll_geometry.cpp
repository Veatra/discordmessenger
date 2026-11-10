#include "ScrollGeometry.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
int main() {
    using namespace ScrollGeometry;
    auto empty=Calculate(100,0,0,100,0,24); assert(empty.length==0);
    auto fit=Calculate(100,0,99,100,0,24); assert(fit.length==0);
    auto first=Calculate(100,0,999,100,0,24); assert(first.start==0 && first.length==24);
    auto last=Calculate(100,0,999,100,900,24); assert(last.start==76);
    assert(last.Position(76,0)==900); assert(last.Position(76+12,12)==900);
    assert(last.Position(-200,12)==0 && last.Position(2000,12)==900);
    assert(Calculate(10,0,999,100,900,24).travel==0);
    assert(Clamp(100,0,40,100)==0); assert(Clamp(-5,0,999,0)==0);
    assert(Maximum(0,999,0)==999); assert(Maximum(0,INT_MAX,UINT_MAX)==0);
    auto huge=Calculate(10000,INT_MIN,INT_MAX,0,INT_MAX,24);
    assert(huge.start==9976 && huge.Position(9976,0)==INT_MAX);
    for (int pos=100;pos<=1000;pos+=10) {
        auto thumb=Calculate(10000,100,1099,100,pos,24);
        assert(thumb.Position(thumb.start,0)==pos);
    }
    puts("PASS: scroll ranges, pages, thumb dragging, shrinking lists and 32-bit limits");
}
