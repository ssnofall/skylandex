#pragma once

#include <nfc/nfc_device.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SKYLANDEX_NFC_DIR "/ext/apps_data/skylandex/nfc"

bool skylander_nfc_ensure_dir(void);
bool skylander_nfc_save_device(NfcDevice* device, const char* path);
bool skylander_nfc_build_path(
    char* out,
    size_t out_size,
    const char* uid_hex,
    uint16_t character_id,
    const char* name);
