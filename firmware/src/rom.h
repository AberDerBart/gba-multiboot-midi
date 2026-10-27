#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

extern const uint8_t *rom;
uint32_t rom_get_size();

bool rom_upload_stdio();
