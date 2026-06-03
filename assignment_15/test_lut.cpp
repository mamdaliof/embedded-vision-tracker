#include <iostream>
#include <cassert>
#include "lut_gen.hpp"

void test_green_detection() {
    // Typical "Green" in YUV: U=107, V=43 (approx for mid-green)
    // Typical "Red" in YUV: U=90, V=240
    
    unsigned char lut[256][256];
    generate_hsv_lut(lut);

    std::cout << "Testing Green (U=107, V=43)... ";
    assert(lut[107][43] == 255);
    std::cout << "PASS" << std::endl;

    std::cout << "Testing Red (U=90, V=240)... ";
    assert(lut[90][240] == 0);
    std::cout << "PASS" << std::endl;
}

int main() {
    test_green_detection();
    return 0;
}
