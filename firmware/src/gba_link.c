#include "gba_link.h"
#include "pico/time.h"
#include <stdint.h>
#include "hardware/gpio.h"
#define SO_PIN 2
#define SI_PIN 3
#define SC_PIN 4

void gba_link_init() {
  gpio_init(SC_PIN);
  gpio_set_dir(SC_PIN, true);

  gpio_init(SO_PIN);
  gpio_set_dir(SO_PIN, false);

  gpio_init(SI_PIN);
  gpio_set_dir(SI_PIN, true);
}

uint16_t transfer16(uint16_t);
uint32_t transfer32(uint32_t);

bool gba_link_upload(char *rom, size_t size) {
  uint16_t r;

  // Send connection request, wait for slave to enter Normal mode
  uint8_t count = 0;
  while (true) {
    r = transfer16(0x6202);

    // Check whether slave has entered correct mode, recognition okay
    if ((r & 0xfff0) == 0x7200) {
      unsigned x = r & 0xf;
      if (x != 2)
        return false;
      break;
    }

    // Blink blue while waiting
    // gpio_put(LED_BLUE, count & 8 ? HIGH : LOW);
    count += 1;

    // Wait a bit before trying again
    sleep_ms(1000 / 16);
  }

  // Blue during transmission
  // gpio_put(LED_BLUE, LOW);

  // Exchange master/slave info
  r = transfer16(0x6102);
  if (r != 0x7202)
    return false;

  // Send header
  for (size_t i = 0; i < 0xc0; i += 2) {

    // Send 2 bytes at a time
    uint16_t xxxx = (uint16_t)rom[i] | (uint16_t)rom[i + 1] << 8;
    r = transfer16(xxxx);

    // Slave replies with index and id
    if (((r >> 8) & 0xff) != (0xc0 - i) / 2)
      return false;
    if ((r & 0xff) != 2)
      return false;
  }

  // Transfer is complete
  r = transfer16(0x6200);
  if (r != 2)
    return false;

  // Exchange master/slave info (again)
  r = transfer16(0x6202);
  if (r != 0x7202)
    return false;

  // Choose palette
  uint8_t pp = 0xd1;

  // Wait until slave is ready
  while (true) {
    r = transfer16(0x6300 | pp);
    if ((r & 0xff00) == 0x7300)
      break;
  }

  // Random client data
  uint8_t cc = r & 0xff;

  // Compute handshake data
  uint8_t hh = (0x11 + cc + 0xff + 0xff) & 0xff;

  // Send handshake
  r = transfer16(0x6400 | hh);
  if ((r & 0xff00) != 0x7300)
    return false;

  // Wait a bit
  sleep_ms(1000 / 16);

  // Send length information
  uint16_t llll = (size - 0xc0) / 4 - 0x34;
  r = transfer16(llll);
  if ((r & 0xff00) != 0x7300)
    return false;
  uint8_t rr = r & 0xff;

  // Send encrypted payload
  uint32_t c = 0x0000c387;
  uint32_t x = 0x0000c37b;
  uint32_t k = 0x43202f2f;
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
    r = transfer32(yyyyyyyy) >> 16;

    // Client replies with lower bits of destination address
    if (r != (i & 0xffff))
      return false;
  }

  // Final checksum update
  c ^= f;
  for (unsigned bit = 0; bit < 32; ++bit) {
    bool carry = c & 1;
    c >>= 1;
    if (carry)
      c ^= x;
  }

  // Client just acknowledged the last bits
  r = transfer16(0x0065);
  if (r != (size & 0xffff))
    return false;

  // Wait until all slaves are ready for CRC transfer
  while (true) {
    r = transfer16(0x0065);
    if (r == 0x0075)
      break;
    if (r != 0x0074)
      return false;
  }

  // Signal that CRC will follow
  r = transfer16(0x0066);
  if (r != 0x0075)
    return false;

  // Exchange CRC
  r = transfer16((uint16_t)(c & 0xffff));
  if (r != c)
    return false;

  // Success!
  return true;
}

uint16_t transfer16(uint16_t msg) {
  uint32_t reply = transfer32((uint32_t)msg & 0xffff);
  return (uint16_t)(reply >> 16);
}

uint32_t transfer32(uint32_t msg) {
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
