#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc_system.h"

int main(int argc, char** argv) {
    // parse arguments: duty cycle (0-255) and direction (0=CCW, 1=CW)
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <duty_cycle 0-255> <direction 0=CCW 1=CW>\n", argv[0]);
        return -1;
    }

    uint8_t duty      = (uint8_t) atoi(argv[1]);
    uint8_t direction = (uint8_t) atoi(argv[2]);

    // open /dev/mem to access physical memory
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Couldn't open /dev/mem\n");
        return -1;
    }

    // map the Avalon bus into userspace
    volatile uint32_t* base = (uint32_t*) mmap(
        NULL,
        HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_BASE
    );

    if (base == MAP_FAILED) {
        perror("Couldn't map bridge.");
        close(fd);
        return -1;
    }

    // read encoder values continuously and update PWM
    printf("Starting PWM + Encoder loop. Press Ctrl+C to exit.\n");
    printf("Duty cycle: %d/255, Direction: %s\n", duty, direction ? "CW" : "CCW");

    while (1) {
        // write PWM control word to address 0x02
        // bit[31]  = cnt_enable
        // bit[8]   = direction
        // bit[7:0] = duty_cycle
        base[2] = (1 << 31) | (direction << 8) | duty;

        // read encoder counts from address 0x00 and 0x01
        int32_t yaw   = (int32_t) base[0]; // Reads slave_address 0x00
        int32_t pitch = (int32_t) base[1]; // Reads slave_address 0x01

        printf("Yaw: %d \t Pitch: %d\n", yaw, pitch);

        usleep(100000); // 100ms
    }

    munmap((void*) base, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN);
    close(fd);
    return 0;
}