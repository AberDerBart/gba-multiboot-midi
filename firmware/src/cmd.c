#include "cmd.h"
#include "rom.h"
#include "serial.h"

#include <pico/stdio.h>
#include <stdio.h>
#include <string.h>

#define MAX_CMD_LEN 128

void handle_message();

void cmd_task() {
  static size_t index = 0;
  while (true) {
    int read = stdio_getchar_timeout_us(0);
    char c = read & 0xff;
    if (read != c) {
      // some error occured, stop reading
      break;
    }

    if (index >= MAX_CMD_LEN) {
      index = 0;
      printf("cmd buffer overflow\n");
      continue;
    }
    switch (c) {
      case '\r':
        // skip carriage return
        continue;
      case '\n':
        serial_buffer[index] = 0;
        index = 0;
        handle_message();
        continue;
      default:
        serial_buffer[index] = c;
        index++;
    }
  }
}

void handle_message() {
  if (!strncmp((char*) serial_buffer, "", MAX_CMD_LEN)) {
    return;
  } else if (!strncmp((char*) serial_buffer, "upload", MAX_CMD_LEN)) {
    rom_upload_stdio();
  } else {
    printf("unknown command: %s\n", serial_buffer);
  }
}
