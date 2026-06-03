#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc_system.h"

int main(int argc, char** argv) {
    // parse arguments: yaw_duty (0-255), yaw_dir (0-1), pitch_duty (0-255), pitch_dir (0-1)
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <yaw_duty 0-255> <yaw_dir 0-1> <pitch_duty 0-255> <pitch_dir 0-1>\n", argv[0]);
        return -1;
    }

    uint8_t yaw_duty      = (uint8_t) atoi(argv[1]);
    uint8_t yaw_direction = (uint8_t) atoi(argv[2]);
    uint8_t pitch_duty      = (uint8_t) atoi(argv[3]);
    uint8_t pitch_direction = (uint8_t) atoi(argv[4]);

    // open /dev/mem to access physical memory
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Couldn't open /dev/mem\n");
        return -1;
    }

    // map the Avalon bus into userspace
    volatile uint32_t* base = (uint32_t*) mmap(
        NULL,                                    // let OS choose virtual address
        HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN,      // size of the region to map
        PROT_READ | PROT_WRITE,                  // read and write access
        MAP_SHARED,                              // share with hardware
        fd,                                      // /dev/mem file descriptor
        HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_BASE       // physical address of IP
    );

    if (base == MAP_FAILED) {
        perror("Couldn't map bridge.");
        close(fd);
        return -1;
    }

    // read encoder values continuously and update PWM
    printf("Starting Dual-Axis PWM + Encoder loop. Press Ctrl+C to exit.\n");
    printf("Yaw: Duty %d, Dir %d | Pitch: Duty %d, Dir %d\n", yaw_duty, yaw_direction, pitch_duty, pitch_direction);
    int counter = 0;
    while (1) {
        // write PWM control words
        // bit[31]  = cnt_enable
        // bit[8]   = direction
        // bit[7:0] = duty_cycle
        base[2] = (1 << 31) | (yaw_direction << 8) | yaw_duty;   // Yaw at 0x02
        base[3] = (1 << 31) | (pitch_direction << 8) | pitch_duty; // Pitch at 0x03
        // read encoder counts from address 0x00 and 0x01
        int32_t yaw   = (int32_t) base[0]; // Reads slave_address 0x00
        int32_t pitch = (int32_t) base[1]; // Reads slave_address 0x01
        if (counter % 1000 == 0) { // Print every 100 iterations (every 100ms)
            printf("\rYaw: %d \t Pitch: %d", yaw, pitch);
            fflush(stdout);
            counter = 0;
        }
        counter++;

        
        usleep(1000); // 1ms
    }

    munmap((void*) base, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN);
    close(fd);
    return 0;
}