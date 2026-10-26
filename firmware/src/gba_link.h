#pragma once

#include <stddef.h>
#include <stdbool.h>

void gba_link_init();

bool gba_link_upload(const char* rom, size_t size);

void gba_link_send();
