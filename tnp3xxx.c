/*
 * tnp3xxx.c - Compute a key A
 *
 * Ported from tnp3xxx.py by Snofall. (originally written 2016-2018 by Vitorio Miliano,
 * updated to Python 3 in 2022 by Adrian 'vifino' Pistol)
 *
 * To the extent possible under law, the author has dedicated all
 * copyright and related and neighboring rights to this software to
 * the public domain worldwide. This software is distributed without
 * any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication
 * along with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Magic numbers from the original */
static const uint64_t magic_nums[] = {
    2, 3, 73, 1103, 2017, 560381651ULL, 12868356821ULL
};

/*
 * pseudo_crc48 - CRC64 ECMA-182 trimmed to 48 bits
 * Mirrors the Python implementation exactly.
 */
static uint64_t pseudo_crc48(uint64_t crc, const uint8_t *data, size_t len)
{
    const uint64_t POLY = 0x42f0e1eba9ea3693ULL;
    const uint64_t MSB  = 0x800000000000ULL;
    const uint64_t TRIM = 0xffffffffffffULL;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint64_t)data[i] << 40;
        for (int k = 0; k < 8; k++) {
            if (crc & MSB)
                crc = (crc << 1) ^ POLY;
            else
                crc <<= 1;
            crc &= TRIM;
        }
    }
    return crc;
}

/* Returns 1 if uid is exactly 8 hex characters, 0 otherwise */
static int is_valid_uid(const char *uid)
{
    if (strlen(uid) != 8)
        return 0;
    for (int i = 0; i < 8; i++)
        if (!isxdigit((unsigned char)uid[i]))
            return 0;
    return 1;
}

/*
 * calc_keya - compute Key A for a given UID and sector
 *
 * out_key must point to a buffer of at least 13 bytes (12 hex chars + NUL).
 * Returns 0 on success, -1 on error (prints reason to stderr).
 */
static int calc_keya(const char *uid, int sector, char *out_key)
{
    if (sector == 0) {
        /* sector 0: magic_nums[2] * magic_nums[4] * magic_nums[5] */
        uint64_t val = magic_nums[2] * magic_nums[4] * magic_nums[5];
        snprintf(out_key, 13, "%012llx", (unsigned long long)val);
        return 0;
    }

    if (!is_valid_uid(uid)) {
        fprintf(stderr, "Error: invalid UID (must be four hex bytes, e.g. aabbccdd)\n");
        return -1;
    }

    if (sector < 0 || sector > 15) {
        fprintf(stderr, "Error: invalid sector (must be 0-15)\n");
        return -1;
    }

    /* PRE = 2^2 * 3 * 1103 * 12868356821 */
    uint64_t PRE = magic_nums[0] * magic_nums[0]
                 * magic_nums[1]
                 * magic_nums[3]
                 * magic_nums[6];

    /* Parse UID hex string into 4 bytes, append sector byte */
    uint8_t data[5];
    for (int i = 0; i < 4; i++) {
        char byte_str[3] = { uid[i * 2], uid[i * 2 + 1], '\0' };
        data[i] = (uint8_t)strtoul(byte_str, NULL, 16);
    }
    data[4] = (uint8_t)sector;

    uint64_t key = pseudo_crc48(PRE, data, 5);

    /*
     * Python: struct.pack('<Q', key).hex()[0:12]
     * Pack as 8-byte little-endian, take first 12 hex chars (= first 6 bytes).
     */
    uint8_t packed[8];
    for (int i = 0; i < 8; i++)
        packed[i] = (uint8_t)((key >> (8 * i)) & 0xff);

    snprintf(out_key, 13, "%02x%02x%02x%02x%02x%02x",
             packed[0], packed[1], packed[2],
             packed[3], packed[4], packed[5]);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <UID> [-eml]\n", argv[0]);
        fprintf(stderr, "  UID   : 4 hex bytes, e.g. aabbccdd\n");
        fprintf(stderr, "  -eml  : output in .eml (mfoc) format\n");
        return 1;
    }

    const char *uid = argv[1];
    int eml_mode = (argc > 2 && strcmp(argv[2], "-eml") == 0);

    /* Compute all 16 keys */
    char keysa[16][13];
    for (int sector = 0; sector < 16; sector++) {
        if (calc_keya(uid, sector, keysa[sector]) != 0)
            return 1;
        /* Convert to uppercase to match Python output */
        for (int j = 0; j < 12; j++)
            keysa[sector][j] = (char)toupper((unsigned char)keysa[sector][j]);
    }

    if (eml_mode) {
        /*
         * Python eml format:
         *   Line 1 : <UID> + 24 zeros  (sector 0, block 0)
         *   Lines 2-3: 32 zeros each   (data blocks)
         *   Line 4 : <keyA> + 20 zeros (trailer)
         *   Then for sectors 1-15, separated by blank lines internally:
         *     3 x "00..00\n" (32 zeros) then trailer
         *   Last line: 20 zeros
         *
         * Faithfully reproducing the Python join logic:
         *   print('0'*20+'\n'+('0'*32+'\n')*3).join(keysa)
         *         .join([(uid+'0'*24+'\n')+('0'*32+'\n')*2, '0'*20])
         */

        /* Prefix block */
        printf("%s%024d\n", uid, 0);
        printf("%032d\n%032d\n", 0, 0);

        for (int s = 0; s < 16; s++) {
            printf("%s%020d\n", keysa[s], 0);
            if (s < 15) {
                printf("%032d\n", 0);
                printf("%032d\n", 0);
                printf("%032d\n", 0);
            }
        }
        printf("%020d\n", 0);
    } else {
        for (int s = 0; s < 16; s++)
            printf("%s\n", keysa[s]);
    }

    return 0;
}