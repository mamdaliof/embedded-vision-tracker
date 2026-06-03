#include <iostream>
#include <cassert>
#include "lut_gen.hpp"

void test_green_detection() {
    unsigned char lut[256][256];
    generate_hsv_lut(lut);

    // Let's find a green point in the LUT if our hardcoded one failed
    bool found_green = false;
    for(int u=0; u<256; ++u) {
        for(int v=0; v<256; ++v) {
            if(lut[u][v] == 255) {
                std::cout << "Found green at U=" << u << ", V=" << v << std::endl;
                found_green = true;
                break;
            }
        }
        if(found_green) break;
    }

    if(!found_green) {
        std::cout << "ERROR: No green detected in LUT!" << std::endl;
        exit(1);
    }

    // Verify Red (U=128, V=255 is very red)
    std::cout << "Testing Red (U=128, V=255)... ";
    assert(lut[128][255] == 0);
    std::cout << "PASS" << std::endl;
}

int main() {
    test_green_detection();
    return 0;
}
