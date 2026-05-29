#include "skylander_nfc.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>

// remove unsupported filename characters from a token
static void skylander_nfc_sanitize_token(char* token, size_t token_size) {
    size_t j = 0;
    for(size_t i = 0; token[i] != '\0' && j + 1 < token_size; i++) {
        char c = token[i];

        // keep only alphanumeric characters
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            token[j++] = c;
        }
    }

    token[j] = '\0';

    // fallback name if token becomes empty
    if(j == 0) {
        strlcpy(token, "Figure", token_size);
    }
}

// ensure NFC save directory exists on SD card
bool skylander_nfc_ensure_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_mkdir(storage, SKYLANDEX_NFC_DIR);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// save NFC device data to a .nfc file
bool skylander_nfc_save_device(NfcDevice* device, const char* path) {

    // validate input pointers
    if((device == NULL) || (path == NULL)) return false;

    // ensure NFC storage directory exists
    if(!skylander_nfc_ensure_dir()) return false;
    return nfc_device_save(device, path);
}

// build a safe .nfc filename/path for a Skylander
bool skylander_nfc_build_path(
    char* out,
    size_t out_size,
    const char* uid_hex,
    uint16_t character_id,
    const char* name) {

    // validate output buffer and UID
    if((out == NULL) || (out_size == 0) || (uid_hex == NULL)) return false;

    char safe_name[24];

    // copy and sanitize Skylander name for filesystem safety
    strlcpy(safe_name, (name != NULL) ? name : "Unknown", sizeof(safe_name));
    skylander_nfc_sanitize_token(safe_name, sizeof(safe_name));

    char uid_compact[16] = {0};
    size_t j = 0;

    // remove separators from UID string
    for(size_t i = 0; uid_hex[i] != '\0' && j + 1 < sizeof(uid_compact); i++) {
        char c = uid_hex[i];
        if(c != ':' && c != ' ') {
            uid_compact[j++] = c;
        }
    }
    uid_compact[j] = '\0';

    // fallback UID if conversion fails
    if(j == 0) {
        strlcpy(uid_compact, "00000000", sizeof(uid_compact));
    }

    // build final .nfc file path
    snprintf(
        out,
        out_size,
        SKYLANDEX_NFC_DIR "/%s_%04X_%s.nfc",
        safe_name,
        character_id,
        uid_compact);
    return true;
}
