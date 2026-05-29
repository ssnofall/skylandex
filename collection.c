#include "collection.h"
#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <string.h>

#define COLLECTION_SAVE_PATH "/ext/apps_data/skylandex/collection.bin" // binary save file path on the SD card
#define COLLECTION_FILE_MAGIC 0x534B5943U // SKYC // file signature used to identify valid Skylandex collection files

// header written at the beginning of collection.bin
typedef struct {
    uint32_t magic;     // file signature (SKYC)
    uint16_t version;   // save file format version
    uint16_t count;     // number of saved collection entries
} CollectionFileHeader;

// allocate and zero-initialize a collection structure
Collection* collection_alloc() {
    Collection* collection = malloc(sizeof(Collection));
    if(collection != NULL) {

        // clear memory so all fields start at 0
        memset(collection, 0, sizeof(Collection));
    }
    return collection;
}

// free allocated collection memory
void collection_free(Collection* collection) {
    free(collection);
}

// check whether a UID already exists in the collection
bool collection_contains_uid(Collection* collection, const char* uid_hex) {
    if((collection == NULL) || (uid_hex == NULL)) return false;
    for(uint16_t i = 0; i < collection->count; i++) {

        // compare stored UID against requested UID
        if(strcmp(collection->entries[i].uid_hex, uid_hex) == 0) {
            return true;
        }
    }
    return false;
}

// get a collection entry by index
const CollectionEntry* collection_get_entry(Collection* collection, uint16_t index) {

    // prevent invalid index access
    if((collection == NULL) || (index >= collection->count)) return NULL;
    return &collection->entries[index];
}

// format current RTC date as YYYY-MM-DD
void collection_format_date_now(char* out, size_t out_size) {
    DateTime dt = {0};

    // read current system date/time from RTC
    furi_hal_rtc_get_datetime(&dt);
    snprintf(out, out_size, "%04u-%02u-%02u", (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
}

// add a new Skylander entry to the collection
bool collection_add(
    Collection* collection,
    uint16_t character_id,
    const char* name,
    const char* element,
    const char* uid_hex,
    const char* nfc_path) {

    // validate collection pointer
    if(collection == NULL) return false;

    // prevent exceeding max collection size
    if(collection->count >= MAX_FIGURES) return false;

    // reject duplicate or invalid UIDs
    if((uid_hex == NULL) || collection_contains_uid(collection, uid_hex)) return false;

    // get next free collection slot
    CollectionEntry* entry = &collection->entries[collection->count];

    // fill entry data
    entry->character_id = character_id;

    // copy strings safely into fixed-size buffers
    strlcpy(entry->name, (name != NULL) ? name : "Unknown", sizeof(entry->name));
    strlcpy(entry->element, (element != NULL) ? element : "Unknown", sizeof(entry->element));
    strlcpy(entry->uid_hex, uid_hex, sizeof(entry->uid_hex));
    strlcpy(entry->nfc_path, (nfc_path != NULL) ? nfc_path : "", sizeof(entry->nfc_path));

    // store scan date
    collection_format_date_now(entry->date_scanned, sizeof(entry->date_scanned));

    collection->count++;
    return true;
}

// ensure app data directory exists on SD card
static bool collection_ensure_data_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_mkdir(storage, "/ext/apps_data/skylandex");
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// save collection database to collection.bin
bool collection_save(Collection* collection) {

    // validate collection pointer
    if(collection == NULL) return false;

    // ensure save directory exists
    if(!collection_ensure_data_dir()) return false;

    // create file header
    CollectionFileHeader header = {
        .magic = COLLECTION_FILE_MAGIC,
        .version = COLLECTION_FILE_VERSION,
        .count = collection->count,
    };

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* fp = storage_file_alloc(storage);
    bool success = false;

    // open file for writing and overwrite existing save
    if(storage_file_open(fp, COLLECTION_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {

        // write file header first
        uint32_t header_written = storage_file_write(fp, &header, sizeof(header));
        uint32_t entries_written = 0;

        // write collection entries after the header
        if(collection->count > 0) {
            entries_written = storage_file_write(
                fp,
                collection->entries,
                (uint32_t)(collection->count * sizeof(CollectionEntry)));
        }

        // verify all expected bytes were written successfully
        success = (header_written == sizeof(header)) &&
                  (entries_written == (uint32_t)(collection->count * sizeof(CollectionEntry)));
    }

    // cleanup file/storage resources
    storage_file_close(fp);
    storage_file_free(fp);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// load collection database from collection.bin
bool collection_load(Collection* collection) {

    // validate collection pointer
    if(collection == NULL) return false;

    // clear collection before loading
    memset(collection, 0, sizeof(Collection));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* fp = storage_file_alloc(storage);
    bool success = false;

    // open existing save file
    if(storage_file_open(fp, COLLECTION_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CollectionFileHeader header = {0};

        // read file header
        uint32_t header_read = storage_file_read(fp, &header, sizeof(header));
        if(header_read == sizeof(header)) {

            // validate file signature, version, and entry count
            if((header.magic == COLLECTION_FILE_MAGIC) && (header.version == COLLECTION_FILE_VERSION) &&
               (header.count <= MAX_FIGURES)) {
                collection->count = header.count;

                // read collection entries
                if(header.count > 0) {
                    uint32_t entries_read = storage_file_read(
                        fp,
                        collection->entries,
                        (uint32_t)(header.count * sizeof(CollectionEntry)));

                    // verify all entries were loaded successfully
                    if(entries_read ==
                       (uint32_t)(header.count * sizeof(CollectionEntry))) {
                        success = true;
                    } else {

                        // reset collection if file data is incomplete/corrupted
                        memset(collection, 0, sizeof(Collection));
                    }
                } else {

                    // valid empty collection file
                    success = true;
                }
            }
        }
    }

    // cleanup file/storage resources
    storage_file_close(fp);
    storage_file_free(fp);
    furi_record_close(RECORD_STORAGE);
    return success;
}
