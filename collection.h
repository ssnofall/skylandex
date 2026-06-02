#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_FIGURES 32
#define COLLECTION_FILE_VERSION 4

typedef struct {
    uint16_t character_id;
    uint16_t variant_id;
    char name[32];
    char element[16];
    char uid_hex[16];
    char nfc_path[64];
    char date_scanned[12];
    bool dump_complete;
    uint16_t level;
    uint32_t xp;
    uint32_t gold;
    char nickname[20];
    uint16_t hero_points;
    uint16_t hat_id;
    uint16_t upgrade_path;
    uint8_t platform_flags;
    uint32_t heroic_challenges;
} CollectionEntry;

typedef struct {
    CollectionEntry entries[MAX_FIGURES];
    uint16_t count;
} Collection;

Collection* collection_alloc();
void collection_free(Collection* collection);

bool collection_add(
    Collection* collection,
    uint16_t character_id,
    uint16_t variant_id,
    const char* name,
    const char* element,
    const char* uid_hex,
    const char* nfc_path);

bool collection_contains_uid(Collection* collection, const char* uid_hex);
const CollectionEntry* collection_get_entry(Collection* collection, uint16_t index);
bool collection_remove(Collection* collection, uint16_t index);
bool collection_save(Collection* collection);
bool collection_load(Collection* collection);
void collection_format_date_now(char* out, size_t out_size);
