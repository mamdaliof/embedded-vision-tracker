#pragma once
#include <opencv2/opencv.hpp>
#include <cstdint>

/**
 * CPU-bound fallback mask generation.
 * Iterates through the YUY2 pointer and extracts the UV planes directly.
 */
inline cv::Mat process_yuv_mask(const cv::Mat& yuy2_img) {
    int width = yuy2_img.cols;
    int height = yuy2_img.rows;
    cv::Mat mask(height, width, CV_8UC1, cv::Scalar(0));
    
    const uint8_t* raw = yuy2_img.ptr<uint8_t>();
    uint8_t* mptr = mask.ptr<uint8_t>();
    
    // YUY2 is 4 bytes for 2 pixels: [Y0, U0, Y1, V0]
    for (int i = 0; i < (width * height) / 2; ++i) {
        uint8_t u = raw[1];
        uint8_t v = raw[3];
        
        // Approximate Green Signature matching the LUT
        uint8_t res = (u > 70 && u < 110 && v > 70 && v < 110) ? 255 : 0;
        
        mptr[0] = res; // Assign to Pixel 1
        mptr[1] = res; // Assign to Pixel 2
        
        mptr += 2;
        raw += 4;
    }
    
    return mask;
}