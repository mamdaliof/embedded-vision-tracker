#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "soc_system.h"

int main(int argc, char** argv) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Couldn't open /dev/mem\n");
        return -1;
    }

    uint8_t* esl_demo_map = (uint8_t*)mmap(NULL, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_BASE);
    if (esl_demo_map == MAP_FAILED) {
        perror("Couldn't map bridge.");
        close(fd);
        return -1;
    }

    // Cast to 32-bit pointer. Incrementing this pointer advances by 4 bytes (1 word)
    volatile uint32_t* peripheral_base = (uint32_t *)esl_demo_map;

    uint32_t led_pattern = 0x01; // Start with first LED on

    printf("Starting Control Loop...\n");
    while(1) {
        // 1. Read Sensors
        int32_t yaw_val   = peripheral_base[0]; // Offset 0
        int32_t pitch_val = peripheral_base[1]; // Offset 1
        
        // 2. Write Actuators (LEDs)
        peripheral_base[2] = led_pattern;       // Offset 2
        
        printf("Yaw: %d \t Pitch: %d \t LEDs: 0x%02X\n", yaw_val, pitch_val, led_pattern);
        
        // Shift LED pattern left, reset if it exceeds 8 bits
        led_pattern = led_pattern << 1;
        if (led_pattern > 0x80) {
            led_pattern = 0x01;
        }

        usleep(100000); 
    }

    close(fd);
    return 0;
}