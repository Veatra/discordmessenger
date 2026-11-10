#pragma once
#include <cstdint>
#include <cmath>

// COLORREF-compatible values, kept independent of Win32 for contrast tests.
namespace ThemePalette {
constexpr uint32_t Rgb(unsigned r, unsigned g, unsigned b) { return r | (g << 8) | (b << 16); }
constexpr uint32_t Canvas = Rgb(30, 34, 41);
constexpr uint32_t Panel = Rgb(38, 43, 52);
constexpr uint32_t Input = Rgb(23, 27, 33);
constexpr uint32_t Text = Rgb(232, 236, 243);
constexpr uint32_t Muted = Rgb(190, 200, 215);
constexpr uint32_t Border = Rgb(81, 92, 109);
constexpr uint32_t Hover = Rgb(51, 60, 73);
constexpr uint32_t Selection = Rgb(53, 79, 119);
constexpr uint32_t Accent = Rgb(147, 189, 255);
constexpr uint32_t Mention = Rgb(186, 177, 255);
constexpr uint32_t Warning = Rgb(255, 163, 145);

inline double Luminance(uint32_t color) {
    double values[3];
    for (int i = 0; i < 3; ++i) {
        double c = ((color >> (8 * i)) & 255) / 255.0;
        values[i] = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    }
    return values[0] * 0.2126 + values[1] * 0.7152 + values[2] * 0.0722;
}
inline double Contrast(uint32_t a, uint32_t b) {
    double x = Luminance(a), y = Luminance(b);
    return x > y ? (x + 0.05) / (y + 0.05) : (y + 0.05) / (x + 0.05);
}
inline uint32_t Readable(uint32_t color, uint32_t background) {
    if (Contrast(color, background) >= 4.5) return color;
    const unsigned target = Luminance(background) < 0.5 ? 255 : 0;
    for (unsigned step = 1; step <= 20; ++step) {
        uint32_t adjusted = 0;
        for (int channel = 0; channel < 3; ++channel) {
            unsigned original = (color >> (channel * 8)) & 255;
            adjusted |= ((original * (20 - step) + target * step) / 20) << (channel * 8);
        }
        if (Contrast(adjusted, background) >= 4.5) return adjusted;
    }
    return Rgb(target, target, target);
}
}
