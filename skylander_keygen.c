#include "skylander_keygen.h"
#include <string.h>

static const uint64_t magic_nums[] = {
    2, 3, 73, 1103, 2017, 560381651ULL, 12868356821ULL};

static uint64_t pseudo_crc48(uint64_t crc, const uint8_t* data, size_t len) {
    const uint64_t POLY = 0x42f0e1eba9ea3693ULL;
    const uint64_t MSB = 0x800000000000ULL;
    const uint64_t TRIM = 0xffffffffffffULL;

    for(size_t i = 0; i < len; i++) {
        crc ^= (uint64_t)data[i] << 40;
        for(int k = 0; k < 8; k++) {
            if(crc & MSB)
                crc = (crc << 1) ^ POLY;
            else
                crc <<= 1;
            crc &= TRIM;
        }
    }
    return crc;
}

bool skylander_keygen_derive(const uint8_t uid[4], uint8_t sector, MfClassicKey* out_key) {
    if(sector > 15 || out_key == NULL) return false;

    uint64_t key_val;

    if(sector == 0) {
        key_val = magic_nums[2] * magic_nums[4] * magic_nums[5];
    } else {
        uint64_t PRE = magic_nums[0] * magic_nums[0] * magic_nums[1] * magic_nums[3] *
                       magic_nums[6];

        uint8_t data[5];
        memcpy(data, uid, 4);
        data[4] = (uint8_t)sector;

        key_val = pseudo_crc48(PRE, data, 5);
    }

    for(int i = 0; i < MF_CLASSIC_KEY_SIZE; i++) {
        out_key->data[i] = (uint8_t)((key_val >> (8 * i)) & 0xff);
    }
    return true;
}

void skylander_keygen_derive_all(const uint8_t uid[4], MfClassicKey keys[SKYLANDER_NUM_SECTORS]) {
    for(uint8_t s = 0; s < SKYLANDER_NUM_SECTORS; s++) {
        skylander_keygen_derive(uid, s, &keys[s]);
    }
}
