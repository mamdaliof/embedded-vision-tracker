#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "soc_system.h"

// Include 20-sim generated controllers (prefixed)
#include "../controllers/PositionControllerPan/pan_submod.h"
#include "../controllers/PositionControllerTilt/tilt_submod.h"

// 100Hz Loop (matched to xx_step_size = 0.01)
#define LOOP_PERIOD_NS 10000000 

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <target_pan_counts> <target_tilt_counts>\n", argv[0]);
        return -1;
    }

    int32_t target_pan = atoi(argv[1]);
    int32_t target_tilt = atoi(argv[2]);

    // Setup Memory Mapping
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Couldn't open /dev/mem\n");
        return -1;
    }

    volatile uint32_t* base = (uint32_t*) mmap(
        NULL, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        fd, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_BASE
    );

    if (base == MAP_FAILED) {
        perror("Couldn't map bridge.");
        close(fd);
        return -1;
    }

    // Capture initial "Zero Point" (assuming home position)
    int32_t zero_pan = (int32_t)base[0];
    int32_t zero_tilt = (int32_t)base[1];

    // Initialize Controllers
    pan_XXDouble pan_u[2], pan_y[2];
    tilt_XXDouble tilt_u[3], tilt_y[1];

    pan_u[0] = 0.0; pan_u[1] = 0.0;
    tilt_u[0] = 0.0; tilt_u[1] = 0.0; tilt_u[2] = 0.0;

    pan_XXInitializeSubmodel(pan_u, pan_y, 0.0);
    tilt_XXInitializeSubmodel(tilt_u, tilt_y, 0.0);

    printf("Controller Initialized (Encoder Mode).\n");
    printf("Zero: P=%d, T=%d | Target: P=%d, T=%d\n", zero_pan, zero_tilt, (int)target_pan, (int)target_tilt);
    printf("Starting 100Hz Control Loop...\n");

    struct timespec next_step;
    clock_gettime(CLOCK_MONOTONIC, &next_step);

    while (1) {
        // 1. Read Encoders (Current - Initial = Absolute Difference)
        int32_t current_pan = (int32_t)base[0] - zero_pan;
        int32_t current_tilt = (int32_t)base[1] - zero_tilt;

        // 2. Update Inputs (Raw Counts)
        pan_u[0] = (double)target_pan;
        pan_u[1] = (double)current_pan;

        tilt_u[0] = pan_y[0]; // Correction from Pan PID
        tilt_u[1] = (double)target_tilt;
        tilt_u[2] = (double)current_tilt;

        // 3. Calculate Step
        pan_XXCalculateSubmodel(pan_u, pan_y, pan_xx_time);
        tilt_XXCalculateSubmodel(tilt_u, tilt_y, tilt_xx_time);

        // 4. Output Mapping
        // Controller output y is normalized -1.0 to 1.0
        double out_p = pan_y[1]; // Motor output for Pan
        double out_t = tilt_y[0]; // Motor output for Tilt

        // Convert to Duty (0-255) and Direction
        uint8_t duty_p = (uint8_t)(fmin(fabs(out_p), 1.0) * 255.0);
        uint8_t dir_p = (out_p >= 0) ? 1 : 0;

        uint8_t duty_t = (uint8_t)(fmin(fabs(out_t), 1.0) * 255.0);
        uint8_t dir_t = (out_t >= 0) ? 1 : 0;

        // Write to Hardware
        base[2] = (1 << 31) | (dir_p << 8) | duty_p;
        base[3] = (1 << 31) | (dir_t << 8) | duty_t;

        if (pan_xx_steps % 100 == 0) {
            printf("\rP: %d (Out: %d) | T: %d (Out: %d)", (int)current_pan, (int)duty_p, (int)current_tilt, (int)duty_t);
            fflush(stdout);
        }

        // 5. Wait for next step (10ms)
        next_step.tv_nsec += LOOP_PERIOD_NS;
        if (next_step.tv_nsec >= 1000000000) {
            next_step.tv_nsec -= 1000000000;
            next_step.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_step, NULL);
    }

    // Cleanup
    pan_XXTerminateSubmodel(pan_u, pan_y, pan_xx_time);
    tilt_XXTerminateSubmodel(tilt_u, tilt_y, tilt_xx_time);
    munmap((void*) base, HPS_0_ARM_A9_0_ESL_BUS_DEMO_0_SPAN);
    close(fd);
    return 0;
}
