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
    volatile uint32_t* encoder_base = (uint32_t *)esl_demo_map;

    printf("Starting Encoder Read Loop. Press Ctrl+C to exit.\n");
    while(1) {
        int32_t yaw_val   = encoder_base[0]; // Reads slave_address 0x00
        int32_t pitch_val = encoder_base[1]; // Reads slave_address 0x01
        
        printf("Yaw Count: %d \t Pitch Count: %d\n", yaw_val, pitch_val);
        
        usleep(100000); // 100ms delay 
    }

    close(fd);
    return 0;
}