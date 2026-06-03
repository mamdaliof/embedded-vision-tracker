// Raspberry Pi + icoBoard implementation of the dual-axis JIWY controller.
// Mirrors main_control.c (DE10-Nano version) but replaces /dev/mem + Avalon bus
// with SPI transactions to the icoBoard FPGA.
//
// SPI frame layout (both directions, 4 bytes per transaction):
//   TX (RPi -> icoBoard):  [duty_pan][dir_pan][duty_tilt][dir_tilt]
//   RX (icoBoard -> RPi):  [enc_pan_hi][enc_pan_lo][enc_tilt_hi][enc_tilt_lo]
//
// Compile:
//   gcc rpi_main_control.c \
//       controllers/PositionControllerPan/pan_*.c \
//       controllers/PositionControllerTilt/tilt_*.c \
//       -lm -o jiwy_controller_rpi
//
// Run:
//   ./jiwy_controller_rpi <target_pan_counts> <target_tilt_counts>

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "../controllers/PositionControllerPan/pan_submod.h"
#include "../controllers/PositionControllerTilt/tilt_submod.h"

// SPI config — matches icoBoard SPI clock polarity/phase (mode 0)
#define SPI_DEVICE    "/dev/spidev0.1"
#define SPI_SPEED_HZ  500000   // 500 kHz; icoBoard can handle up to ~1 MHz comfortably
#define SPI_FRAME_LEN 4        // bytes per full duplex transaction

// Timing — 100 Hz to match the 20-sim model step size of 0.01 s
#define LOOP_PERIOD_NS 10000000L

// Encoder scaling: counts -> radians
// Adjust COUNTS_PER_REV to match our encoder's resolution.
#define COUNTS_PER_REV 4096.0
#define COUNTS_TO_RAD  (2.0 * M_PI / COUNTS_PER_REV)

// SPI helpers
static int spi_open(void) {
    int fd = open(SPI_DEVICE, O_RDWR);
    if (fd < 0) { perror("open SPI"); return -1; }

    uint8_t mode  = SPI_MODE_0;
    uint8_t bits  = 8;
    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(fd, SPI_IOC_WR_MODE,          &mode)  < 0) { perror("SPI mode");  close(fd); return -2; }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits)  < 0) { perror("SPI bits");  close(fd); return -3; }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ,  &speed) < 0) { perror("SPI speed"); close(fd); return -4; }

    return fd;
}

// Full-duplex 4-byte transfer.
// tx[4]: bytes to send to icoBoard
// rx[4]: bytes received from icoBoard
static int spi_xfer(int fd, uint8_t *tx, uint8_t *rx) {
    struct spi_ioc_transfer t = {
        .tx_buf        = (unsigned long)tx,
        .rx_buf        = (unsigned long)rx,
        .len           = SPI_FRAME_LEN,
        .speed_hz      = SPI_SPEED_HZ,
        .bits_per_word = 8,
        .delay_usecs   = 0,
        .cs_change     = 0,
    };
    return ioctl(fd, SPI_IOC_MESSAGE(1), &t);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <target_pan_counts> <target_tilt_counts>\n", argv[0]);
        return -1;
    }

    int32_t target_pan  = atoi(argv[1]);
    int32_t target_tilt = atoi(argv[2]);

    // Open SPI
    int fd = spi_open();
    if (fd < 0) return -1;

    // Capture zero reference (send a neutral frame, read back current position)
    uint8_t tx[SPI_FRAME_LEN] = {0, 0, 0, 0};
    uint8_t rx[SPI_FRAME_LEN] = {0};

    if (spi_xfer(fd, tx, rx) < 0) { perror("SPI zero-reference xfer"); close(fd); return -1; }

    int16_t zero_pan  = (int16_t)((rx[0] << 8) | rx[1]);
    int16_t zero_tilt = (int16_t)((rx[2] << 8) | rx[3]);

    // Initialize controllers
    pan_XXDouble  pan_u[2],  pan_y[2];
    tilt_XXDouble tilt_u[3], tilt_y[1];

    pan_u[0]  = 0.0; pan_u[1]  = 0.0;
    tilt_u[0] = 0.0; tilt_u[1] = 0.0; tilt_u[2] = 0.0;

    pan_XXInitializeSubmodel(pan_u,  pan_y,  0.0);
    tilt_XXInitializeSubmodel(tilt_u, tilt_y, 0.0);

    printf("Controller initialized (SPI mode).\n");
    printf("Zero: P=%d, T=%d | Target: P=%d, T=%d\n",
           zero_pan, zero_tilt, (int)target_pan, (int)target_tilt);
    printf("Starting 100Hz control loop...\n");

    // Timing setup 
    struct timespec next_step;
    clock_gettime(CLOCK_MONOTONIC, &next_step);

    int32_t step = 0;
    while (1) {
        // Read encoders — relative to zero reference
        if (spi_xfer(fd, tx, rx) < 0) {
            perror("SPI read xfer");
            break;
        }
        int16_t raw_pan  = (int16_t)((rx[0] << 8) | rx[1]);
        int16_t raw_tilt = (int16_t)((rx[2] << 8) | rx[3]);

        int32_t current_pan  = (int32_t)raw_pan  - zero_pan;
        int32_t current_tilt = (int32_t)raw_tilt - zero_tilt;

        // Convert encoder counts to radians (SI units for 20-sim model)
        double pan_rad  = current_pan  * COUNTS_TO_RAD;
        double tilt_rad = current_tilt * COUNTS_TO_RAD;
        double sp_pan   = target_pan   * COUNTS_TO_RAD;
        double sp_tilt  = target_tilt  * COUNTS_TO_RAD;

        // Update controller inputs
        pan_u[0] = sp_pan;   // setpoint  (rad)
        pan_u[1] = pan_rad;  // feedback  (rad)

        tilt_u[0] = pan_y[0]; // pan correction coupling (matches DE10 version)
        tilt_u[1] = sp_tilt;  // setpoint  (rad)
        tilt_u[2] = tilt_rad; // feedback  (rad)

        // Step controllers and measure calculation time
        struct timespec t_calc_start, t_calc_end;
        clock_gettime(CLOCK_MONOTONIC, &t_calc_start);

        pan_XXCalculateSubmodel(pan_u,  pan_y,  pan_xx_time);
        tilt_XXCalculateSubmodel(tilt_u, tilt_y, tilt_xx_time);

        clock_gettime(CLOCK_MONOTONIC, &t_calc_end);

        // Convert controller output [-1, 1] to duty (0-255) + direction
        double out_p = pan_y[1];
        double out_t = tilt_y[0];

        uint8_t duty_p = (uint8_t)(fmin(fabs(out_p), 1.0) * 255.0);
        uint8_t dir_p  = (out_p >= 0.0) ? 1 : 0;

        uint8_t duty_t = (uint8_t)(fmin(fabs(out_t), 1.0) * 255.0);
        uint8_t dir_t  = (out_t >= 0.0) ? 1 : 0;

        // Write PWM to icoBoard via SPI
        // TX frame: [duty_pan, dir_pan, duty_tilt, dir_tilt]
        tx[0] = duty_p;
        tx[1] = dir_p;
        tx[2] = duty_t;
        tx[3] = dir_t;

        // Periodic status + DSE timing print
        if (step % 100 == 0) {
            long calc_ns = (t_calc_end.tv_sec  - t_calc_start.tv_sec)  * 1000000000L
                         + (t_calc_end.tv_nsec - t_calc_start.tv_nsec);
            printf("\rP: %d (out: %d) | T: %d (out: %d) | calc: %ld us",
                   (int)current_pan, (int)duty_p,
                   (int)current_tilt, (int)duty_t,
                   calc_ns / 1000);
            fflush(stdout);
        }
        step++;

        // Wait for next 10 ms tick
        next_step.tv_nsec += LOOP_PERIOD_NS;
        if (next_step.tv_nsec >= 1000000000L) {
            next_step.tv_nsec -= 1000000000L;
            next_step.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_step, NULL);
    }

    // -- Cleanup --
    pan_XXTerminateSubmodel(pan_u,  pan_y,  pan_xx_time);
    tilt_XXTerminateSubmodel(tilt_u, tilt_y, tilt_xx_time);
    close(fd);
    return 0;
}
