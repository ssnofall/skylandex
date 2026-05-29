#include "character_db.h"
#include <furi.h>
#include <stddef.h>
#include <string.h>

static const char* TAG = "CharacterDB";

// skylander character lookup database
static const SkylanderInfo SKYLANDER_DB[] = {
    {0x0000, "Whirlwind",       "Air"},
    {0x0001, "Sonic Boom",      "Air"},
    {0x0002, "Warnado",         "Air"},
    {0x0003, "Lightning Rod",   "Air"},
    {0x0004, "Bash",            "Earth"},
    {0x0005, "Terrafin",        "Earth"},
    {0x0006, "Dino-Rang",       "Earth"},
    {0x0007, "Prism Break",     "Earth"},
    {0x0008, "Sunburn",         "Fire"},
    {0x0009, "Eruptor",         "Fire"},
    {0x000A, "Ignitor",         "Fire"},
    {0x000B, "Flameslinger",    "Fire"},
    {0x000C, "Zap",             "Water"},
    {0x000D, "Wham-Shell",      "Water"},
    {0x000E, "Gill Grunt",      "Water"},
    {0x000F, "Slam Bam",        "Water"},
    {0x0010, "Spyro",           "Magic"},
    {0x0011, "Voodood",         "Magic"},
    {0x0012, "Double Trouble",  "Magic"},
    {0x0013, "Trigger Happy",   "Tech"},
    {0x0014, "Drobot",          "Tech"},
    {0x0015, "Drill Sergeant",  "Tech"},
    {0x0016, "Boomer",          "Tech"},
    {0x0017, "Wrecking Ball",   "Tech"},
    {0x0018, "Camo",            "Life"},
    {0x0019, "Zook",            "Life"},
    {0x001A, "Stealth Elf",     "Life"},
    {0x001B, "Stump Smash",     "Life"},
    {0x001C, "Dark Spyro",      "Magic"},
    {0x001D, "Hex",             "Undead"},
    {0x001E, "Chop Chop",       "Undead"},
    {0x001F, "Ghost Roaster",   "Undead"},
    {0x0020, "Cynder",          "Undead"},
};

// total number of entries in the the Skylander database
#define SKYLANDER_DB_COUNT (sizeof(SKYLANDER_DB) / sizeof(SKYLANDER_DB[0]))

// internal helper to find a Skylander by character ID
static const SkylanderInfo* character_db_find(uint16_t character_id) {
    for(size_t i = 0; i < SKYLANDER_DB_COUNT; i++) {
        if(SKYLANDER_DB[i].id == character_id) {
            return &SKYLANDER_DB[i];
        }
    }
    // character ID was not found in the database
    return NULL;
}

// public lookup function with debug logging
const SkylanderInfo* character_db_lookup(uint16_t character_id) {
    const SkylanderInfo* info = character_db_find(character_id);
    if(info != NULL) {
        FURI_LOG_D(
            TAG,
            "lookup id=0x%04X OK name=%s element=%s",
            character_id,
            info->name,
            info->element);
    } else {
        FURI_LOG_D(TAG, "lookup id=0x%04X FAIL (not in database)", character_id);
    }
    return info;
}

// convert element strings into internal numeric IDs
uint8_t character_db_get_element(uint16_t character_id) {
    const SkylanderInfo* info = character_db_find(character_id);

    // unknown character
    if(info == NULL) {
        return 0;
    }
    // map element names to internal element IDs
    if(strcmp(info->element, "Magic") == 0) return 1;
    if(strcmp(info->element, "Water") == 0) return 2;
    if(strcmp(info->element, "Earth") == 0) return 3;
    if(strcmp(info->element, "Fire") == 0) return 4;
    if(strcmp(info->element, "Air") == 0) return 5;
    if(strcmp(info->element, "Undead") == 0) return 6;
    if(strcmp(info->element, "Life") == 0) return 7;
    if(strcmp(info->element, "Tech") == 0) return 8;

    // unknown/unmapped element
    return 0;
}
