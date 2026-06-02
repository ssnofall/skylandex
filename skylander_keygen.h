#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

#define SKYLANDER_NUM_SECTORS 16
#define SKYLANDER_BLOCKS_PER_SECTOR 4
#define SKYLANDER_TOTAL_BLOCKS 64

void skylander_keygen_derive_all(const uint8_t uid[4], MfClassicKey keys[SKYLANDER_NUM_SECTORS]);

bool skylander_keygen_derive(const uint8_t uid[4], uint8_t sector, MfClassicKey* out_key);
