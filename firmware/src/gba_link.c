#include "gba_link.h"
#include "pico/time.h"
#include <hardware/sync.h>
#include <stdint.h>
#include <stdio.h>
#include "hardware/gpio.h"
#include "rom.h"

#define SO_PIN 2
#define SI_PIN 3
#define SC_PIN 4

#define BAUD_RATE 115200
#define SD_PIN 5

void gba_link_init() {
  gpio_init(SC_PIN);
  gpio_set_dir(SC_PIN, true);

  gpio_init(SO_PIN);
  gpio_set_dir(SO_PIN, false);
  gpio_pull_up(SO_PIN);

  gpio_init(SI_PIN);
  gpio_set_dir(SI_PIN, true);
  gpio_put(SI_PIN, true);

  gpio_init(SD_PIN);
  gpio_set_dir(SD_PIN, false);
  gpio_pull_up(SD_PIN);
}

typedef struct reply16_multi_t {
  uint16_t msgs[3];
  size_t player_count;
} reply16_multi_t;

reply16_multi_t transfer16_multi(uint16_t);
uint16_t transfer16_normal(uint16_t);
uint32_t transfer32_normal(uint32_t);

bool gba_link_upload() {
  uint32_t size = rom_get_size();
  if (size < 0xc0) {
    printf("rom not available");
    return false;
  }

  // align rom size
  size = (size + 0x0f) & ~0x0f;

  uint16_t r;

  // Send connection request, wait for slave to enter Normal mode
  printf("GBA link: sending connection request\n");

  while (true) {
    r = transfer16_normal(0x6202);
    printf(".");

    // Check whether slave has entered correct mode, recognition okay
    if ((r & 0xfff0) == 0x7200) {
      unsigned x = r & 0xf;
      if (x != 2)
        return false;
      break;
    }
    printf("%04x\n", r);

    // Wait a bit before trying again
    sleep_ms(5);
  }

  // Blue during transmission
  // gpio_put(LED_BLUE, LOW);

  // Exchange master/slave info
  r = transfer16_normal(0x6102);
  if (r != 0x7202)
    return false;

  // Send header
  printf("GBA link: sending header\n");
  for (size_t i = 0; i < 0xc0; i += 2) {

    // Send 2 bytes at a time
    uint16_t xxxx = (uint16_t)rom[i] | (uint16_t)rom[i + 1] << 8;
    r = transfer16_normal(xxxx);

    // Slave replies with index and id
    if (((r >> 8) & 0xff) != (0xc0 - i) / 2) {
      printf("counter does not match, want %d, got %d\n",(0xc0 -i), (r >> 8) & 0xff);
      return false;
    }
    if ((r & 0xff) != 2) {
      printf("client id does not match, got %d\n", r & 0xff);
      return false;
    }
  }


  printf("header transfer complete\n");
  // Transfer is complete
  r = transfer16_normal(0x6200);
  if (r != 2)
    return false;

  // Exchange master/slave info (again)
  printf("exchange parent client info again\n");
  r = transfer16_normal(0x6202);
  if (r != 0x7202)
    return false;

  // Choose palette
  uint8_t pp = 0xd1;

  printf("send palette data\n");
  // Wait until slave is ready
  while (true) {
    r = transfer16_normal(0x6300 | pp);
    if ((r & 0xff00) == 0x7300)
      break;
  }


  // Random client data
  uint8_t cc = r & 0xff;

  // Compute handshake data
  uint8_t hh = (0x11 + cc + 0xff + 0xff) & 0xff;

  printf("send handshake\n");
  // Send handshake
  r = transfer16_normal(0x6400 | hh);
  if ((r & 0xff00) != 0x7300)
    return false;

  // Wait a bit
  sleep_ms(1000 / 16);

  printf("GBA link: sending payload\n");

  // Send length information
  uint16_t llll = (size - 0xc0) / 4 - 0x34;
  r = transfer16_normal(llll);
  if ((r & 0xff00) != 0x7300)
    return false;
  uint8_t rr = r & 0xff;

  // Send encrypted payload
  uint32_t c = 0x0000fff8;
  uint32_t x = 0x0000a517;
  uint32_t k = 0x6465646f;
  uint32_t m = 0xffff0000 | cc << 8 | pp;
  uint32_t f = 0xffff0000 | rr << 8 | hh;
  for (size_t i = 0xc0; i < size; i += 4) {

    // Send 4 bytes at a time
    uint32_t data = (uint32_t)rom[i] | (uint32_t)rom[i + 1] << 8 |
                    (uint32_t)rom[i + 2] << 16 | (uint32_t)rom[i + 3] << 24;

    // Update checksum
    c ^= data;
    for (unsigned bit = 0; bit < 32; ++bit) {
      bool carry = c & 1;
      c >>= 1;
      if (carry)
        c ^= x;
    }

    // Encrypt and send data
    m = 0x6f646573 * m + 1;
    uint32_t complement = -(uint32_t)0x2000000 - (uint32_t)i;
    uint32_t yyyyyyyy = data ^ complement ^ m ^ k;
    
    r = transfer16_normal(yyyyyyyy & 0xFFFF);
    // Client replies with lower bits of destination address
    if (r != i) {
      printf("address mismatch want %d got %d\n", i, r);
      return false;
    }
    r = transfer16_normal(yyyyyyyy >> 16);
    // Client replies with lower bits of destination address
    if (r != i + 2) {
      printf("2nd word address mismatch want %d got %d\n", i + 2, r);
      return false;
    }
  }
  printf("final checksum update\n");

  // Final checksum update
  c ^= f;
  for (unsigned bit = 0; bit < 32; ++bit) {
    bool carry = c & 1;
    c >>= 1;
    if (carry)
      c ^= x;
  }

  // Client just acknowledged the last bits
  r = transfer16_normal(0x0065);
  if (r != (size & 0xffff)) {
    printf("checksum reply failed, want %d, got %d\n", size & 0xffff, r);
    return false;
  }

  printf("wait for CRC transfer\n");
  // Wait until all slaves are ready for CRC transfer
  while (true) {
    r = transfer16_normal(0x0065);
    if (r == 0x0075)
      break;
    if (r != 0x0074)
      return false;
  }

  printf("signal CRC\n");

  // Signal that CRC will follow
  r = transfer16_normal(0x0066);
  if (r != 0x0075)
    return false;

  printf("exchange RCR\n");
  // Exchange CRC
  r = transfer16_normal((uint16_t)(c & 0xffff));
  if (r != c) {
    printf("CRC failed, want %04x, got %04x\n", c, r);
    return false;
  }

  printf("GBA link: ROM sent\n");

  // Success!
  return true;
}

uint16_t transfer16_normal(uint16_t msg) {
  reply16_multi_t reply = transfer16_multi(msg);
  
  return reply.msgs[0];
  //uint32_t reply = transfer32_normal((uint32_t)msg & 0xffff);
  //return (uint16_t)(reply >> 16);
}

absolute_time_t bit_time_offset(absolute_time_t start, size_t bit) {
  return delayed_by_us(start, 1000000 * bit / BAUD_RATE);
}

void send16_multi(uint16_t msg) {
  gpio_set_dir(SD_PIN, true);
  absolute_time_t start = get_absolute_time();
  // send start bit
  gpio_put(SD_PIN, false);

  // send bits
  for (size_t i = 0; i < 16; i++) {
    bool bit = (msg >> i) & 1;
    busy_wait_until(bit_time_offset(start, i + 1));
    gpio_put(SD_PIN, bit);
  }
  busy_wait_until(bit_time_offset(start, 17));
  // send stop bit
  gpio_put(SD_PIN, true);

  busy_wait_until(bit_time_offset(start, 18));
}

bool recv16_multi(uint16_t* dst, uint8_t timeout_us) {
  uint16_t tmp = 0x0000;
  gpio_set_dir(SD_PIN, false);
  
  // wait for start bit
  absolute_time_t timeout = make_timeout_time_us(timeout_us);

  bool start_bit_stable = false;
  while (!start_bit_stable) {
    while (gpio_get(SD_PIN)) {
      if (to_us_since_boot(get_absolute_time()) > to_us_since_boot(timeout)){
        return false;
      }
    }
    // wait for half a bit to check if start bit is stable
    busy_wait_us(1000000 / BAUD_RATE / 2);
    start_bit_stable = !gpio_get(SD_PIN);
  }

  // this is referencing the half point of the bit
  // as we waited half a bits transfer time during start bit detection
  absolute_time_t t0 = get_absolute_time();
  
  // read bits
  for (size_t i = 0; i < 16; i++) {
    busy_wait_until(bit_time_offset(t0, i + 1));
    uint16_t bit = gpio_get(SD_PIN);
    tmp |= bit << i;
  }
  // wait for middle of stop bit
  // we can wait for the end of it in the next call
  busy_wait_until(bit_time_offset(t0, 17));
  // TODO: validate stop bit

  *dst = tmp;
  return true;
}


reply16_multi_t transfer16_multi(uint16_t msg) {
  reply16_multi_t result = {
    .msgs = { 0x0000, 0x0000, 0x0000 },
    .player_count = 0,
  };

  uint32_t irq = save_and_disable_interrupts();

  while (!gpio_get(SD_PIN)) {
    // Wait for ready signal (nobody is pulling SD low)
  }
  // signal start of transfer
  gpio_put(SC_PIN, false);
  // signal that the master is sending
  gpio_put(SI_PIN, true);
  //send data
  send16_multi(msg);
  // signal that the master is done sending
  gpio_put(SI_PIN, false);
  //receive data
  for (size_t i = 0; i < 3; i++){
    if (!recv16_multi(&(result.msgs[i]), 250)) {
      break;
    }
    result.player_count++;
  }
  // signal end of transfer
  gpio_put(SC_PIN, true);
  gpio_put(SI_PIN, true);

  restore_interrupts(irq);

  busy_wait_us(36);

  return result;
}

uint32_t transfer32_normal(uint32_t msg) {
  uint32_t reply = 0;

  // Send bits, from most- to least-significant
  for (int i = 31; i >= 0; --i) {

    // Clock falling-edge
    gpio_put(SC_PIN, false);

    // Write i-th bit
    gpio_put(SI_PIN, (msg & (1 << i)));

    // Wait a bit
    sleep_us(4);

    // Clock rising-edge, slave will latch the ouput
    gpio_put(SC_PIN, true);

    // Read i-th bit as well
    reply |= (gpio_get(SO_PIN) ? 1 : 0) << i;

    // Wait a bit
    sleep_us(4);
  }

  // Leave SI high to signal readiness
  gpio_put(SI_PIN, true);

  return reply;
}
