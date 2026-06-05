#pragma once
#include <opencv2/opencv.hpp>
#include <cstdint>
#include <algorithm>

/**
 * Generates a 64x64x64 Quantized 3D LUT.
 * Precomputes the exact OpenCV HSV thresholding offline.
 */
inline void generate_hsv_lut(uint8_t lut[64][64][64]) {
    for (int y = 0; y < 64; y++) {
        for (int u = 0; u < 64; u++) {
            for (int v = 0; v < 64; v++) {
                // 1. Reconstruct approximate 8-bit center values
                int y8 = (y << 2) | 2;
                int u8 = (u << 2) | 2;
                int v8 = (v << 2) | 2;

                // 2. Standard SDTV YUV to BGR manual conversion
                int c = y8 - 16;
                int d = u8 - 128;
                int e = v8 - 128;
                int r = (298 * c + 409 * e + 128) >> 8;
                int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                int b = (298 * c + 516 * d + 128) >> 8;
                
                r = std::clamp(r, 0, 255);
                g = std::clamp(g, 0, 255);
                b = std::clamp(b, 0, 255);

                // 3. Use OpenCV to do the HSV conversion on the single pixel
                cv::Mat bgr_pix(1, 1, CV_8UC3, cv::Scalar(b, g, r));
                cv::Mat hsv_pix;
                cv::cvtColor(bgr_pix, hsv_pix, cv::COLOR_BGR2HSV);

                cv::Vec3b hsv = hsv_pix.at<cv::Vec3b>(0, 0);
                int H = hsv[0];
                int S = hsv[1];
                int V = hsv[2];

                // 4. YOUR EXACT ORIGINAL THRESHOLDS
                if (H >= 36 && H <= 89 && 
                    S >= 50 && S <= 255 && 
                    V >= 70 && V <= 255) {
                    lut[y][u][v] = 255;
                } else {
                    lut[y][u][v] = 0;
                }
            }
        }
    }
}