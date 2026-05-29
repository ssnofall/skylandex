#pragma once

#include <stdint.h>

typedef struct {
    uint16_t id;
    const char* name;
    const char* element;
} SkylanderInfo;

const SkylanderInfo* character_db_lookup(uint16_t character_id);
uint8_t character_db_get_element(uint16_t character_id);