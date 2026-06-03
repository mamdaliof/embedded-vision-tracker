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
#include "controllers/PositionControllerPan/pan_submod.h"
#include "controllers/PositionControllerTilt/tilt_submod.h"

// 100Hz Loop (matched to xx_step_size = 0.01)
#define LOOP_PERIOD_NS 10000000 
#define COUNTS_PITCH 13100
#define COUNTS_YAW 10750

// Helper function to find a limit by moving until the encoder stops changing
// Helper function to find a limit by moving until the encoder stops changing
int32_t find_limit(volatile uint32_t* base, int motor_idx, int encoder_idx, int direction, int pwm_duty) {
    // 1. Apply movement
    base[motor_idx] = (1U << 31) | (direction << 8) | pwm_duty;

    // 2. Head start: Wait 250ms to let the motor overcome static friction 
    // and actually start moving BEFORE we begin checking for a stall.
    usleep(250000); 

    int32_t last_enc = (int32_t)base[encoder_idx];
    int stall_counter = 0;

    // 3. Monitor for stall
    while (stall_counter < 15) { 
        usleep(20000); // 20ms wait per loop
        int32_t current_enc = (int32_t)base[encoder_idx];
        
        // If it moved less than 5 counts in 20ms, consider it stalled
        if (abs(current_enc - last_enc) < 5) {
            stall_counter++;
        } else {
            stall_counter = 0; // It is moving freely, reset counter!
        }
        last_enc = current_enc;
    }
    
    // 4. Stop the motor and wait a moment for it to physically settle
    base[motor_idx] = (1U << 31) | (0 << 8) | 0;
    usleep(250000); 
    
    return last_enc;
}

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

    // Calibration: Find limits for Pan and Tilt
    printf("Calibrating Pan (Yaw)...\n");
        printf("Calibrating Pan (Yaw)...\n");
    int32_t pan_limit = find_limit(base, 2, 0, 0, 25); // Duty 90
    int32_t pan_center = pan_limit + (COUNTS_YAW / 2);

    printf("Calibrating Tilt (Pitch)...\n");
    int32_t tilt_limit = find_limit(base, 3, 1, 0, 25); // Duty 90
    int32_t tilt_center = tilt_limit + (COUNTS_PITCH / 2);

    // Now your true zeros are the center of the mechanical ranges!
    int32_t zero_pan = pan_center; 
    int32_t zero_tilt = tilt_center;

    // Capture initial "Zero Point" (assuming home position)
    // int32_t zero_pan = (int32_t)base[0];
    // int32_t zero_tilt = (int32_t)base[1];

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

        // Safety limits (half of total range)
        const int32_t PAN_MAX_LIMIT = COUNTS_YAW / 2;
        const int32_t TILT_MAX_LIMIT = COUNTS_PITCH / 2;

        // Clamp Targets to prevent trying to drive past limits
        if (target_pan > PAN_MAX_LIMIT) target_pan = PAN_MAX_LIMIT;
        if (target_pan < -PAN_MAX_LIMIT) target_pan = -PAN_MAX_LIMIT;
        if (target_tilt > TILT_MAX_LIMIT) target_tilt = TILT_MAX_LIMIT;
        if (target_tilt < -TILT_MAX_LIMIT) target_tilt = -TILT_MAX_LIMIT;

        double rad_per_count_pitch = 1.2*M_PI/COUNTS_PITCH;
        double rad_per_count_yaw = M_PI/COUNTS_YAW;

        // 2. Update Inputs (Raw Counts)
        pan_u[0] = (double)target_pan*rad_per_count_yaw;
        pan_u[1] = (double)current_pan*rad_per_count_yaw;

        tilt_u[0] = pan_y[0]; // Correction from Pan PID
        tilt_u[1] = (double)target_tilt*rad_per_count_pitch;
        tilt_u[2] = (double)current_tilt*rad_per_count_pitch;

        // 3. Calculate Step
        pan_XXCalculateSubmodel(pan_u, pan_y, pan_xx_time);
        tilt_XXCalculateSubmodel(tilt_u, tilt_y, tilt_xx_time);

        // 4. Output Mapping
        // Controller output y is normalized -1.0 to 1.0
        double out_p = 0.8*pan_y[1]; // Motor output for Pan
        double out_t = 0.8*tilt_y[0]; // Motor output for Tilt

        // Convert to Duty (0-255) and Direction
        uint8_t duty_p = (uint8_t)(fmin(fabs(out_p), 1.0) * 255.0)/2.0;
        uint8_t dir_p = (out_p >= 0) ? 1 : 0;

        uint8_t duty_t = (uint8_t)(fmin(fabs(out_t), 1.0) * 255.0)/2.0;
        uint8_t dir_t = (out_t >= 0) ? 1 : 0;

        // Write to Hardware
        base[2] = (1U << 31) | (dir_p << 8) | duty_p;
        base[3] = (1U << 31) | (dir_t << 8) | duty_t;

        if (pan_xx_steps % 100 == 0) {
            printf("\rP: %d (Out: %d) | T: %d (Out: %d) | outP: %f | outT: %f\n", (int)current_pan, (int)duty_p, (int)current_tilt, (int)duty_t, out_p, out_t);
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
