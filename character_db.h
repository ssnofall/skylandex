#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint16_t id;
    uint16_t variant_id;
    uint8_t element_id;
    char name[32];
    char element[16];
} SkylanderInfo;

bool character_db_init();
void character_db_free();

const SkylanderInfo* character_db_lookup(uint16_t character_id);
const SkylanderInfo* character_db_lookup_by_variant(uint16_t character_id, uint16_t variant_id);

uint8_t character_db_get_element(uint16_t character_id);
size_t character_db_get_count();
