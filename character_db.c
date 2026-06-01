#include "character_db.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>
#include <stddef.h>

static const char* TAG = "CharacterDB";

#define DB_MAGIC    0x4B445953
#define DB_VERSION  1
#define DB_SD_PATH  "/ext/apps_data/skylandex/skylander_db.bin"
#define DB_ASSETS_PATH "/ext/apps_assets/skylandex/skylander_db.bin"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
} DBHeader;

typedef struct {
    uint16_t character_id;
    uint16_t variant_id;
    uint8_t element_id;
    uint8_t padding;
    char name[30];
    char element[16];
} DBEntry;

static SkylanderInfo* g_db = NULL;
static size_t g_db_count = 0;

static bool character_db_parse_file(File* fp, uint32_t count) {
    if(count == 0) {
        g_db = NULL;
        g_db_count = 0;
        return true;
    }

    SkylanderInfo* db = malloc(sizeof(SkylanderInfo) * count);
    if(db == NULL) return false;

    for(uint32_t i = 0; i < count; i++) {
        DBEntry entry;
        if(storage_file_read(fp, &entry, sizeof(DBEntry)) != sizeof(DBEntry)) {
            FURI_LOG_E(TAG, "Failed to read entry %lu/%lu", (unsigned long)i, (unsigned long)count);
            free(db);
            return false;
        }
        db[i].id = entry.character_id;
        db[i].variant_id = entry.variant_id;
        db[i].element_id = entry.element_id;
        strlcpy(db[i].name, entry.name, sizeof(db[i].name));
        strlcpy(db[i].element, entry.element, sizeof(db[i].element));
    }

    g_db = db;
    g_db_count = count;
    FURI_LOG_I(TAG, "Loaded %zu entries", g_db_count);
    return true;
}

static bool character_db_try_open_and_parse(Storage* storage, const char* path) {
    File* fp = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(fp, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        DBHeader hdr;
        if(storage_file_read(fp, &hdr, sizeof(hdr)) == sizeof(hdr) &&
           hdr.magic == DB_MAGIC && hdr.version == DB_VERSION)
        {
            ok = character_db_parse_file(fp, hdr.count);
        }
        storage_file_close(fp);
    }

    storage_file_free(fp);
    return ok;
}

static bool character_db_copy_from_assets(Storage* storage) {
    File* src = storage_file_alloc(storage);
    File* dst = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(src, DB_ASSETS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        DBHeader hdr;
        if(storage_file_read(src, &hdr, sizeof(hdr)) == sizeof(hdr) &&
           hdr.magic == DB_MAGIC && hdr.version == DB_VERSION)
        {
            storage_simply_mkdir(storage, "/ext/apps_data/skylandex");

            if(storage_file_open(dst, DB_SD_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                uint8_t buf[512];
                size_t total = 0;
                size_t to_copy = sizeof(hdr) + (size_t)hdr.count * sizeof(DBEntry);

                storage_file_write(dst, &hdr, sizeof(hdr));
                total += sizeof(hdr);

                while(total < to_copy) {
                    size_t chunk = to_copy - total;
                    if(chunk > sizeof(buf)) chunk = sizeof(buf);
                    size_t read = storage_file_read(src, buf, chunk);
                    if(read == 0) break;
                    storage_file_write(dst, buf, read);
                    total += read;
                }

                ok = (total == to_copy);
                storage_file_close(dst);

                if(ok) {
                    FURI_LOG_I(TAG, "Database copied to SD card (%zu bytes)", total);
                }
            }
        }
        storage_file_close(src);
    }

    storage_file_free(src);
    storage_file_free(dst);
    return ok;
}

bool character_db_init() {
    if(g_db != NULL) return true;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool success = false;

    if(character_db_try_open_and_parse(storage, DB_SD_PATH)) {
        success = true;
    } else if(character_db_copy_from_assets(storage)) {
        success = character_db_try_open_and_parse(storage, DB_SD_PATH);
    } else {
        FURI_LOG_E(TAG, "Database not found at SD path or assets");
    }

    furi_record_close(RECORD_STORAGE);

    if(!success) {
        FURI_LOG_W(TAG, "Database unavailable — lookups will return NULL");
        g_db = NULL;
        g_db_count = 0;
    }

    return success;
}

void character_db_free() {
    if(g_db != NULL) {
        free(g_db);
        g_db = NULL;
    }
    g_db_count = 0;
}

static const SkylanderInfo* character_db_find_by_id(uint16_t character_id) {
    for(size_t i = 0; i < g_db_count; i++) {
        if(g_db[i].id == character_id) {
            return &g_db[i];
        }
    }
    return NULL;
}

static const SkylanderInfo* character_db_find_by_id_and_variant(uint16_t character_id, uint16_t variant_id) {
    for(size_t i = 0; i < g_db_count; i++) {
        if(g_db[i].id == character_id && g_db[i].variant_id == variant_id) {
            return &g_db[i];
        }
    }
    return NULL;
}

const SkylanderInfo* character_db_lookup(uint16_t character_id) {
    const SkylanderInfo* info = character_db_find_by_id(character_id);
    if(info != NULL) {
        FURI_LOG_D(TAG, "lookup id=0x%04X OK name=%s element=%s",
                   character_id, info->name, info->element);
    } else {
        FURI_LOG_D(TAG, "lookup id=0x%04X FAIL (not in database)", character_id);
    }
    return info;
}

const SkylanderInfo* character_db_lookup_by_variant(uint16_t character_id, uint16_t variant_id) {
    const SkylanderInfo* info = character_db_find_by_id_and_variant(character_id, variant_id);
    if(info != NULL) {
        FURI_LOG_D(TAG, "lookup_variant id=0x%04X var=0x%04X OK name=%s",
                   character_id, variant_id, info->name);
    } else {
        FURI_LOG_D(TAG, "lookup_variant id=0x%04X var=0x%04X FAIL (falling back to base)",
                   character_id, variant_id);
    }
    return info;
}

uint8_t character_db_get_element(uint16_t character_id) {
    const SkylanderInfo* info = character_db_find_by_id(character_id);
    if(info == NULL) return 0;
    return info->element_id;
}

size_t character_db_get_count() {
    return g_db_count;
}
