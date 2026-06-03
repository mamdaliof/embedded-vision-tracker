#include <opencv2/opencv.hpp>

inline cv::Mat process_yuv_mask(const cv::Mat& yuyv) {
    std::vector<cv::Mat> planes;
    cv::split(yuyv, planes); // planes[0]: Y, planes[1]: U/V interleaved
    
    cv::Mat uv_interleaved = planes[1];
    // uv_interleaved contains [U0, V0, U1, V1, ...]
    // Reshape it to 2 channels to split U and V
    cv::Mat uv_split[2];
    cv::split(uv_interleaved.reshape(2, yuyv.rows), uv_split);
    
    cv::Mat u_plane = uv_split[0]; // U values
    cv::Mat v_plane = uv_split[1]; // V values
    
    cv::Mat mask_u, mask_v, mask_uv;
    // Green color usually has low U and low V in YUV
    // Thresholds: U < 100, V < 100
    cv::inRange(u_plane, 0, 100, mask_u);
    cv::inRange(v_plane, 0, 100, mask_v);
    cv::bitwise_and(mask_u, mask_v, mask_uv);
    
    // Each pixel in mask_uv corresponds to TWO pixels in original image (sharing U/V)
    // We need to upscale it back to original width
    cv::Mat mask;
    cv::repeat(mask_uv, 1, 2, mask); // Repeat columns
    
    return mask;
}
