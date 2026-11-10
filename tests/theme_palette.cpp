#include "ThemePalette.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace ThemePalette;
    const uint32_t surfaces[] = {Canvas, Panel, Input, Selection, Hover};
    for (auto surface : surfaces) {
        assert(Contrast(Text, surface) >= 4.5);
        assert(Contrast(Muted, surface) >= 4.5);
        assert(Contrast(Readable(Accent, surface), surface) >= 4.5);
        assert(Contrast(Readable(Mention, surface), surface) >= 4.5);
    }
    // Role colors ranging from black to saturated/dim RGB must remain readable.
    for (unsigned r=0;r<=255;r+=51) for (unsigned g=0;g<=255;g+=51) for (unsigned b=0;b<=255;b+=51) {
        auto color=Rgb(r,g,b);
        assert(Contrast(Readable(color,Canvas),Canvas)>=4.5);
        assert(Contrast(Readable(color,Selection),Selection)>=4.5);
    }
    assert(Readable(Text,Canvas)==Text); // preserve already readable colors
    puts("PASS: dark text/background pairs, links, selections, and 216 role colors");
}
