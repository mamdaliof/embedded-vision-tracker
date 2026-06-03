#ifndef LUT_GEN_HPP
#define LUT_GEN_HPP

#include <cmath>
#include <algorithm>

/**
 * Converts YUV to HSV and checks if it matches "green".
 * Y is fixed at 128 for the LUT.
 */
inline bool is_green_hsv(int u, int v) {
    // Convert YUV to RGB (standard BT.601)
    // Y=128, U, V are in [0, 255]
    float y_f = 128.0f;
    float u_f = (float)u - 128.0f;
    float v_f = (float)v - 128.0f;

    float r = y_f + 1.402f * v_f;
    float g = y_f - 0.344136f * u_f - 0.714136f * v_f;
    float b = y_f + 1.772f * u_f;

    r = std::max(0.0f, std::min(255.0f, r)) / 255.0f;
    g = std::max(0.0f, std::min(255.0f, g)) / 255.0f;
    b = std::max(0.0f, std::min(255.0f, b)) / 255.0f;

    // RGB to HSV
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    float delta = max_c - min_c;

    float h = 0;
    if (delta > 0) {
        if (max_c == r) h = 60.0f * fmod(((g - b) / delta), 6.0f);
        else if (max_c == g) h = 60.0f * (((b - r) / delta) + 2.0f);
        else if (max_c == b) h = 60.0f * (((r - g) / delta) + 4.0f);
    }
    if (h < 0) h += 360.0f;

    float s = (max_c == 0) ? 0 : (delta / max_c) * 255.0f;
    // float v_val = max_c * 255.0f; // Not needed for green check

    // Green range: Hue [36, 89], Saturation [50, 255]
    return (h >= 36.0f && h <= 89.0f && s >= 50.0f);
}

inline void generate_hsv_lut(unsigned char lut[256][256]) {
    for (int u = 0; u < 256; ++u) {
        for (int v = 0; v < 256; ++v) {
            lut[u][v] = is_green_hsv(u, v) ? 255 : 0;
        }
    }
}

#endif
