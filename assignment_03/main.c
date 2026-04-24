// main.c
// Contains a simple GPIO controller that alternates every second between HIGH and LOW on the pin specified by line_num (BCM numbering)
// Heavily inspired by https://github.com/starnight/libgpiod-example/blob/master/libgpiod-led/main.c

#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
int main(int argc, char **argv) {
  char *chipname = "gpiochip0";
  unsigned int line_num = 8;
  unsigned int val;
  struct gpiod_chip *chip;
  struct gpiod_line *line;
  int i, ret;
  chip = gpiod_chip_open_by_name(chipname);
  if (!chip) {
    perror("Open chip failed\n");
    goto end;
  }
  line = gpiod_chip_get_line(chip, line_num);
  if (!line) {
    perror("Get line failed\n");
    goto close_chip;
  }
  ret = gpiod_line_request_output(line, "user", 0);
  if (ret < 0) {
    perror("Request line as output failed\n");
    goto release_line;
  }
  val = 0;
  while (true) {
      ret = gpiod_line_set_value(line, val);
      if (ret < 0) {
        perror("Set line output failed\n");
        goto release_line;
      }
      printf("Output %u on line #%u\n", val, line_num);
      sleep(1);
      val = !val;
    }
release_line:
  gpiod_line_release(line);
close_chip:
  gpiod_chip_close(chip);
end:
  return 0;
}
