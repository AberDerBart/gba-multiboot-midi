#include <stdio.h>

#include "midi.h"
#include "midi_ble.h"
#include "pico/stdlib.h"


int main(void) {

  midi_init();
  midi_ble_init();

  printf("init\n");

  while (1) {
  }

  return 0;
}

