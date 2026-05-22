#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>  // FIXED: Added missing header for gettimeofday
#include <unistd.h>

#define LOOPS 10000
#define SPEED 100000 
#define BYTES 2      

double time_time(void) {
  struct timeval tv;
  double t;

  gettimeofday(&tv, 0); // FIXED: Header resolved

  t = (double)tv.tv_sec + ((double)tv.tv_usec / 1E6);

  return t;
}

int spiOpen(unsigned spiChan, unsigned spiBaud, unsigned spiFlags) {
  int fd;
  char spiMode;
  char spiBits = 8;
  char dev[32];

  spiMode = spiFlags & 3;
  spiBits = 8;

  sprintf(dev, "/dev/spidev0.%d", spiChan);

  if ((fd = open(dev, O_RDWR)) < 0) {
    return -1;
  }

  if (ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0) {
    close(fd);
    return -2;
  }

  if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits) < 0) {
    close(fd);
    return -3;
  }

  if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiBaud) < 0) {
    close(fd);
    return -4;
  }

  return fd;
}

int spiClose(int fd) { return close(fd); }

int spiRead(int fd, unsigned speed, char *buf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  // FIXED: Cast pointers to (unsigned long) to match 64-bit architecture
  spi.tx_buf = (unsigned long)NULL;
  spi.rx_buf = (unsigned long)buf;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

int spiWrite(int fd, unsigned speed, char *buf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  // FIXED: Cast pointers to (unsigned long) to match 64-bit architecture
  spi.tx_buf = (unsigned long)buf;
  spi.rx_buf = (unsigned long)NULL;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

int spiXfer(int fd, unsigned speed, char *txBuf, char *rxBuf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf = (unsigned long)txBuf;
  spi.rx_buf = (unsigned long)rxBuf;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

#define MAX_SPI_BUFSIZ 8192

char RXBuf[MAX_SPI_BUFSIZ];
char TXBuf[MAX_SPI_BUFSIZ];

int bytes = BYTES;
int speed = SPEED;
int loops = LOOPS;

int main(int argc, char *argv[]) {
    int fd;
    int loops = LOOPS;
    int speed = SPEED;

    // Open SPI
    fd = spiOpen(1, speed, 0);
    if (fd < 0) {
        perror("SPI open failed");
        return 1;
    }

    uint8_t duty = 0;
    uint8_t direction = 1; // 1 = CW
    
    // Calculate how many loops per duty cycle step (0-255)
    int step_size = loops / 256;
    if (step_size == 0) step_size = 1;

    printf("Starting transmission: %d loops, %d bytes\n", loops, BYTES);

    for (int i = 0; i < loops; i++) {
        // Ramp duty cycle from 0 to 255
        duty = (i / step_size) % 256;

        // Build Payload
        TXBuf[0] = duty;
        TXBuf[1] = direction; 

        // Transfer
        if (spiXfer(fd, speed, TXBuf, RXBuf, BYTES) < 0) {
            perror("SPI Xfer failed");
            break;
        }

        // Reconstruct Encoder Value (16-bit)
        int16_t encoder_val = (int16_t)((RXBuf[0] << 8) | (RXBuf[1] & 0xFF));

        if (i % 500 == 0) {
            printf("Loop: %d | Duty: %d | Enc: %u\n", i, duty, encoder_val);
        }
    }

    close(fd);
    return 0;
}