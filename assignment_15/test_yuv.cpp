#include "yuv_process.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cassert>

int main() {
    int w = 4, h = 4;
    // YUYV: 2 bytes per pixel. 4x4 image = 16 pixels = 32 bytes.
    // OpenCV CV_8UC2 represents YUYV where each pixel has 2 channels.
    cv::Mat yuyv(h, w, CV_8UC2);
    
    // Fill with non-green: Y=128, U=128, V=128 (Grey)
    yuyv.setTo(cv::Scalar(128, 128)); 
    
    // Set one pixel to green (Y=128, U=60, V=60)
    // YUYV structure: [Y0, U0, Y1, V0], [Y2, U1, Y3, V1] ...
    // Each 2x1 block of pixels share U and V.
    // In CV_8UC2, data is [Y0, U0], [Y1, V0], [Y2, U1], [Y3, V1]
    uint8_t* data = yuyv.ptr<uint8_t>(0);
    data[0] = 128; // Y0
    data[1] = 60;  // U0
    data[2] = 128; // Y1
    data[3] = 60;  // V0

    cv::Mat mask = process_yuv_mask(yuyv);
    
    int non_zero = cv::countNonZero(mask);
    std::cout << "Non-zero pixels in mask: " << non_zero << std::endl;
    
    if (non_zero == 0) {
        std::cerr << "Test failed: Mask is empty" << std::endl;
        return 1;
    }
    
    std::cout << "Test passed!" << std::endl;
    return 0;
}
