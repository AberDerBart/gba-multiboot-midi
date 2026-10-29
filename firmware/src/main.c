#include <stdio.h>
#include "cmd.h"
#include "pico/stdio.h"

#include "midi.h"
#include "midi_ble.h"
#include "gba_link.h"

int main(void) {

  stdio_init_all();

  midi_init();
  midi_ble_init();
  gba_link_init();

  printf("initialized\n");

  if (gba_link_upload()){
    printf("upload success\n");
  }else{
    printf("upload falied\n");
  }

  while (1) {
    cmd_task();
  }

  return 0;
}

