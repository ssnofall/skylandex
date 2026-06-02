#include "skylander_reader.h"
#include "skylander_crypto.h"
#include "character_db.h"
#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

static const char* TAG = "SkylanderReader";

// default Skylanders sector 0 key
static const MfClassicKey SKYLANDER_SECTOR0_KEY = {.data = {0x4B, 0x0B, 0x20, 0x10, 0x7C, 0xCB}};

// cached sector 0 blocks (kept for compatibility with non-Skylander tags)
static MfClassicBlock sector0_blocks[4];

// full dump storage for Skylander tags
static MfClassicBlock all_blocks[SKYLANDER_TOTAL_BLOCKS];

// convert MfClassicKey into uint64 format
static uint64_t mf_classic_key_to_uint64(const MfClassicKey* key) {
    uint64_t value = 0;
    for(size_t i = 0; i < MF_CLASSIC_KEY_SIZE; i++) {
        value |= ((uint64_t)key->data[i]) << (i * 8);
    }
    return value;
}

// format block 0 bytes into readable hex
static void format_block0_hex(const MfClassicBlock* block, char* out, size_t out_size) {
    snprintf(
        out,
        out_size,
        "%02X%02X%02X%02X %02X%02X%02X%02X",
        block->data[0],
        block->data[1],
        block->data[2],
        block->data[3],
        block->data[4],
        block->data[5],
        block->data[6],
        block->data[7]);
}

// format block 1 bytes into readable hex
static void format_block1_hex(const MfClassicBlock* block, char* out, size_t out_size) {
    snprintf(
        out,
        out_size,
        "%02X%02X%02X%02X %02X%02X%02X%02X",
        block->data[0],
        block->data[1],
        block->data[2],
        block->data[3],
        block->data[4],
        block->data[5],
        block->data[6],
        block->data[7]);
}

// extract character ID and variant ID from block 1
static void parse_ids_from_block1(
    const MfClassicBlock* block,
    ScanResult* result) {
    result->character_id = ((uint16_t)block->data[1] << 8) | block->data[0];
    result->variant_id = ((uint16_t)block->data[3] << 8) | block->data[2];

    FURI_LOG_D(
        TAG,
        "NFC decode id=0x%04X var=0x%04X b1[0..3]=%02X %02X %02X %02X",
        result->character_id,
        result->variant_id,
        block->data[0],
        block->data[1],
        block->data[2],
        block->data[3]);

    const SkylanderInfo* info = character_db_lookup_by_variant(result->character_id, result->variant_id);
    if(info == NULL) {
        info = character_db_lookup(result->character_id);
    }
    result->element_id = (info != NULL) ? info->element_id : 0;
}

// allocate and initialize reader state
SkylanderReader* skylander_reader_alloc() {
    SkylanderReader* reader = malloc(sizeof(SkylanderReader));
    memset(reader, 0, sizeof(SkylanderReader));
    reader->nfc = nfc_alloc();
    reader->scanner = nfc_scanner_alloc(reader->nfc);
    reader->state = SkylanderReaderStateIdle;
    reader->detected_protocol = NfcProtocolInvalid;
    reader->has_detected_protocol = false;
    reader->has_sector0_dump = false;
    reader->mf_type = MfClassicType1k;
    return reader;
}

// free reader resources
void skylander_reader_free(SkylanderReader* reader) {
    nfc_scanner_free(reader->scanner);
    nfc_free(reader->nfc);
    free(reader);
}

// scanner callback triggered when a tag is detected
static void nfc_scanner_callback(NfcScannerEvent event, void* context) {
    SkylanderReader* reader = (SkylanderReader*)context;

    if(event.type != NfcScannerEventTypeDetected) return;
    if(reader->state != SkylanderReaderStateScanning) return;

    reader->state = SkylanderReaderStateTagDetected;
    NfcProtocol protocol = (event.data.protocol_num > 0) ? event.data.protocols[0] : NfcProtocolInvalid;
    reader->detected_protocol = protocol;
    reader->has_detected_protocol = (protocol != NfcProtocolInvalid);
    FURI_LOG_D(
        TAG,
        "Tag detected protocols=%u first=%s",
        (unsigned)event.data.protocol_num,
        nfc_device_get_protocol_name(protocol));

    // notify app that a tag was detected
    if(reader->callback != NULL) reader->callback(reader->callback_context);
}

// start NFC scanning
void skylander_reader_start(
    SkylanderReader* reader,
    SkylanderReaderCallback callback,
    void* context) {
    reader->callback = callback;
    reader->callback_context = context;
    reader->is_running = true;
    reader->state = SkylanderReaderStateScanning;
    reader->detected_protocol = NfcProtocolInvalid;
    reader->has_detected_protocol = false;
    reader->has_sector0_dump = false;
    FURI_LOG_D(TAG, "Starting scanner");
    nfc_scanner_start(reader->scanner, nfc_scanner_callback, reader);
}

// stop NFC scanning
void skylander_reader_stop(SkylanderReader* reader) {
    if(reader->is_running) {
        FURI_LOG_D(TAG, "Stopping scanner");
        nfc_scanner_stop(reader->scanner);
        reader->is_running = false;
    }
    reader->state = SkylanderReaderStateIdle;
    reader->detected_protocol = NfcProtocolInvalid;
    reader->has_detected_protocol = false;
}

// get detected NFC protocol and UID info
bool skylander_reader_get_detected_info(SkylanderReader* reader, ScanDetectionInfo* info) {
    if((reader->state != SkylanderReaderStateTagDetected) && (reader->state != SkylanderReaderStateDone) &&
       (reader->state != SkylanderReaderStateReading))
        return false;
    if((info == NULL) || !reader->has_detected_protocol) return false;

    memset(info, 0, sizeof(ScanDetectionInfo));
    info->detected = true;

    NfcProtocol protocol = reader->detected_protocol;
    info->protocol = protocol;
    strlcpy(info->protocol_name, nfc_device_get_protocol_name(protocol), sizeof(info->protocol_name));

    // populate UID if sector 0 data is available
    if(reader->has_sector0_dump) {
        snprintf(
            info->uid_hex,
            sizeof(info->uid_hex),
            "%02X:%02X:%02X:%02X",
            sector0_blocks[0].data[0],
            sector0_blocks[0].data[1],
            sector0_blocks[0].data[2],
            sector0_blocks[0].data[3]);
        info->uid_available = true;
    } else {
        strlcpy(info->uid_hex, "UID unavailable", sizeof(info->uid_hex));
        info->uid_available = false;
    }
    return true;
}

// parse game data fields from raw blocks
// Two data areas (area 0 at blocks 0x08-0x10, area 1 at blocks 0x24-0x2C).
// The one with the higher sequence byte (offset 0x09) is the active save.
static uint8_t area0_seq_block = 8;  // block 0x08
static uint8_t area1_seq_block = 36; // block 0x24
static uint8_t area_seq_offset = 9;  // sequence byte offset within block

static uint16_t read_16le(const uint8_t* data, uint8_t offset) {
    return (uint16_t)(data[offset] | (data[offset + 1] << 8));
}

static uint32_t read_24le(const uint8_t* data, uint8_t offset) {
    return (uint32_t)(data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16));
}

static uint32_t read_32le(const uint8_t* data, uint8_t offset) {
    return (uint32_t)(data[offset] | (data[offset + 1] << 8) |
                      (data[offset + 2] << 16) | (data[offset + 3] << 24));
}

void skylander_reader_parse_game_data(ScanResult* result) {
    if(!result->is_full_dump) return;
    if(result->all_blocks[0].data[0] == 0) return;

    // determine active area by sequence byte
    uint8_t seq0 = result->all_blocks[area0_seq_block].data[area_seq_offset];
    uint8_t seq1 = result->all_blocks[area1_seq_block].data[area_seq_offset];
    bool use_area1 = (seq1 > seq0);

    uint8_t exp_block = use_area1 ? 36 : 8;          // 0x24 or 0x08
    uint8_t nick_block_a = use_area1 ? 38 : 10;       // 0x26 or 0x0A
    uint8_t nick_block_b = use_area1 ? 40 : 12;       // 0x28 or 0x0C
    uint8_t stat_block = use_area1 ? 41 : 13;         // 0x29 or 0x0D
    uint8_t skill_block = use_area1 ? 37 : 9;         // 0x25 or 0x09
    uint8_t flags_block = use_area1 ? 44 : 16;        // 0x2C or 0x10

    // XP: 24-bit LE at offset 0 (max 33000)
    result->xp = read_24le(result->all_blocks[exp_block].data, 0);
    result->level = (uint16_t)result->xp;

    // Gold: 16-bit LE at offset 3 (max 65000)
    result->gold = read_16le(result->all_blocks[exp_block].data, 3);

    // Hero points: 16-bit LE at offset 0x0A (max 100)
    result->hero_points = read_16le(result->all_blocks[stat_block].data, 0x0A);

    // Hat ID: 16-bit LE at offset 4
    result->hat_id = read_16le(result->all_blocks[skill_block].data, 4);

    // Upgrade path: 16-bit LE at offset 0
    result->upgrade_path = read_16le(result->all_blocks[skill_block].data, 0);

    // Platform flags: 8-bit at offset 3
    result->platform_flags = result->all_blocks[skill_block].data[3];

    // Heroic challenges: 32-bit LE at offset 0x0C
    result->heroic_challenges = read_32le(result->all_blocks[flags_block].data, 0x0C);

    // Nickname: UTF-16LE in two consecutive blocks, decode as ASCII
    char nick_buf[32];
    size_t ni = 0;
    for(int b = 0; b < 2 && ni < sizeof(nick_buf) - 1; b++) {
        const uint8_t* block_data = (b == 0) ? result->all_blocks[nick_block_a].data
                                             : result->all_blocks[nick_block_b].data;
        for(int i = 0; i < 16 && ni < sizeof(nick_buf) - 1; i += 2) {
            uint16_t cp = (uint16_t)(block_data[i] | (block_data[i + 1] << 8));
            if(cp == 0) break;
            if(cp < 128) nick_buf[ni++] = (char)cp;
        }
    }
    nick_buf[ni] = '\0';
    strlcpy(result->nickname, nick_buf, sizeof(result->nickname));
}

// read all 16 sectors from a Skylander tag using on-device key derivation
bool skylander_reader_read_all(SkylanderReader* reader, ScanResult* result) {
    if((reader->state != SkylanderReaderStateTagDetected) || (result == NULL)) return false;

    if(reader->detected_protocol != NfcProtocolMfClassic) {
        FURI_LOG_D(TAG, "Skipping full read for protocol=%s",
                   nfc_device_get_protocol_name(reader->detected_protocol));
        reader->state = SkylanderReaderStateDone;
        return false;
    }

    // initialize result
    memset(result, 0, sizeof(ScanResult));
    strlcpy(result->uid_hex, "UID unavailable", sizeof(result->uid_hex));
    strlcpy(result->block0_hex, "-------- --------", sizeof(result->block0_hex));
    strlcpy(result->block1_hex, "-------- --------", sizeof(result->block1_hex));

    reader->has_sector0_dump = false;
    reader->state = SkylanderReaderStateReading;

    if(reader->is_running) {
        FURI_LOG_D(TAG, "Stopping scanner before full read");
        nfc_scanner_stop(reader->scanner);
        reader->is_running = false;
    }

    // detect Mifare Classic type
    MfClassicType detected_type = MfClassicType1k;
    if(mf_classic_poller_sync_detect_type(reader->nfc, &detected_type) == MfClassicErrorNone) {
        reader->mf_type = detected_type;
    }

    // read sector 0 with the well-known key
    MfClassicKey key = SKYLANDER_SECTOR0_KEY;
    for(uint8_t block = 0; block < 4; block++) {
        FURI_LOG_D(TAG, "Reading sector0 block %u", block);
        MfClassicError error =
            mf_classic_poller_sync_read_block(reader->nfc, block, &key, MfClassicKeyTypeA, &all_blocks[block]);
        if(error != MfClassicErrorNone) {
            reader->state = SkylanderReaderStateError;
            FURI_LOG_W(TAG, "Sector0 block %u read failed: %d", block, error);
            return false;
        }
        sector0_blocks[block] = all_blocks[block];
    }

    reader->has_sector0_dump = true;
    result->read_ok = true;
    result->uid_available = true;

    // extract UID and derive keys
    const uint8_t* uid = all_blocks[0].data;
    snprintf(result->uid_hex, sizeof(result->uid_hex), "%02X:%02X:%02X:%02X",
             uid[0], uid[1], uid[2], uid[3]);
    format_block0_hex(&all_blocks[0], result->block0_hex, sizeof(result->block0_hex));
    format_block1_hex(&all_blocks[1], result->block1_hex, sizeof(result->block1_hex));
    parse_ids_from_block1(&all_blocks[1], result);
    result->has_character_id = true;

    // derive all 16 keys from UID
    skylander_keygen_derive_all(uid, result->derived_keys);

    // store sector 0 key in result
    result->derived_keys[0] = SKYLANDER_SECTOR0_KEY;
    result->sector_status[0] = SkylanderSectorReadOK;

    // read sectors 1-15
    for(uint8_t s = 1; s < SKYLANDER_NUM_SECTORS; s++) {
        bool all_ok = true;
        for(uint8_t bo = 0; bo < SKYLANDER_BLOCKS_PER_SECTOR; bo++) {
            uint8_t block_num = s * SKYLANDER_BLOCKS_PER_SECTOR + bo;
            FURI_LOG_D(TAG, "Reading sector %u block %u (abs %u)", s, bo, block_num);
            MfClassicError error = mf_classic_poller_sync_read_block(
                reader->nfc, block_num, &result->derived_keys[s],
                MfClassicKeyTypeA, &all_blocks[block_num]);
            if(error != MfClassicErrorNone) {
                all_ok = false;
                FURI_LOG_W(TAG, "Sector %u block %u read failed: %d", s, bo, error);
                // zero the failed block
                memset(&all_blocks[block_num], 0, sizeof(MfClassicBlock));
            }
        }
        result->sector_status[s] = all_ok ? SkylanderSectorReadOK : SkylanderSectorReadFailed;
    }

    result->is_full_dump = true;

    // copy all blocks back to result
    memcpy(result->all_blocks, all_blocks, sizeof(all_blocks));

    // decrypt application-layer AES-128 ECB encrypted blocks
    skylander_crypto_decrypt_blocks(result->all_blocks, result->all_blocks);

    // parse game data from decrypted blocks
    skylander_reader_parse_game_data(result);

    reader->state = SkylanderReaderStateDone;
    FURI_LOG_D(TAG, "Full read OK uid=%s id=0x%04X is_full=%d",
               result->uid_hex, result->character_id, result->is_full_dump);
    return true;
}

// build an NFC device from cached sector data (all blocks if available)
bool skylander_reader_build_nfc_device(SkylanderReader* reader, NfcDevice* device) {
    if((reader == NULL) || (device == NULL) || !reader->has_sector0_dump) return false;

    MfClassicData* mf_data = mf_classic_alloc();
    mf_classic_reset(mf_data);
    mf_data->type = reader->mf_type;

    // extract tag UID
    const uint8_t uid[4] = {
        sector0_blocks[0].data[0],
        sector0_blocks[0].data[1],
        sector0_blocks[0].data[2],
        sector0_blocks[0].data[3],
    };
    if(!mf_classic_set_uid(mf_data, uid, sizeof(uid))) {
        mf_classic_free(mf_data);
        return false;
    }

    // Set ATQA and SAK for MIFARE Classic 1K (standard values for Skylanders)
    iso14443_3a_set_atqa(mf_data->iso14443_3a_data, (const uint8_t[]){0x00, 0x04});
    iso14443_3a_set_sak(mf_data->iso14443_3a_data, 0x08);

    // copy all blocks from the full dump buffer
    for(uint8_t block = 0; block < SKYLANDER_TOTAL_BLOCKS; block++) {
        mf_classic_set_block_read(mf_data, block, &all_blocks[block]);
    }

    // derive keys from UID and mark all as known
    MfClassicKey keys[SKYLANDER_NUM_SECTORS];
    skylander_keygen_derive_all(uid, keys);
    for(uint8_t s = 0; s < SKYLANDER_NUM_SECTORS; s++) {
        mf_classic_set_key_found(mf_data, s, MfClassicKeyTypeA, mf_classic_key_to_uint64(&keys[s]));
    }

    nfc_device_set_data(device, NfcProtocolMfClassic, mf_data);
    mf_classic_free(mf_data);
    return true;
}

// read sector 0 from a Skylanders NFC tag
bool skylander_reader_read_sector0(SkylanderReader* reader, ScanResult* result) {
    if((reader->state != SkylanderReaderStateTagDetected) || (result == NULL)) return false;

    // only Mifare Classic tags are supported
    if(reader->detected_protocol != NfcProtocolMfClassic) {
        FURI_LOG_D(
            TAG,
            "Skipping sector0 read for protocol=%s",
            nfc_device_get_protocol_name(reader->detected_protocol));
        reader->state = SkylanderReaderStateDone;
        return false;
    }

    // initialize default result values
    memset(result, 0, sizeof(ScanResult));
    strlcpy(result->uid_hex, "UID unavailable", sizeof(result->uid_hex));
    strlcpy(result->block0_hex, "-------- --------", sizeof(result->block0_hex));
    strlcpy(result->block1_hex, "-------- --------", sizeof(result->block1_hex));

    reader->has_sector0_dump = false;
    reader->state = SkylanderReaderStateReading;

    // stop scanner before direct tag access
    if(reader->is_running) {
        FURI_LOG_D(TAG, "Stopping scanner before read");
        nfc_scanner_stop(reader->scanner);
        reader->is_running = false;
    }

    // detect Mifare Classic tag type
    MfClassicType detected_type = MfClassicType1k;
    if(mf_classic_poller_sync_detect_type(reader->nfc, &detected_type) == MfClassicErrorNone) {
        reader->mf_type = detected_type;
    }

    MfClassicKey key = SKYLANDER_SECTOR0_KEY;

    // read all sector 0 blocks
    for(uint8_t block = 0; block < 4; block++) {
        FURI_LOG_D(TAG, "Reading sector0 block %u", block);
        MfClassicError error =
            mf_classic_poller_sync_read_block(reader->nfc, block, &key, MfClassicKeyTypeA, &sector0_blocks[block]);
        if(error != MfClassicErrorNone) {
            reader->state = SkylanderReaderStateError;
            FURI_LOG_W(TAG, "Sector0 block %u read failed: %d", block, error);
            return false;
        }
        // also populate all_blocks for full dump
        all_blocks[block] = sector0_blocks[block];
    }

    reader->has_sector0_dump = true;
    result->read_ok = true;
    result->uid_available = true;

    // format tag UID
    snprintf(
        result->uid_hex,
        sizeof(result->uid_hex),
        "%02X:%02X:%02X:%02X",
        sector0_blocks[0].data[0],
        sector0_blocks[0].data[1],
        sector0_blocks[0].data[2],
        sector0_blocks[0].data[3]);
    format_block0_hex(&sector0_blocks[0], result->block0_hex, sizeof(result->block0_hex));
    format_block1_hex(&sector0_blocks[1], result->block1_hex, sizeof(result->block1_hex));
    parse_ids_from_block1(&sector0_blocks[1], result);
    result->has_character_id = true;

    reader->state = SkylanderReaderStateDone;
    FURI_LOG_D(
        TAG,
        "Sector0 read OK uid=%s id=0x%04X elem=0x%02X b0=%s b1=%s",
        result->uid_hex,
        result->character_id,
        result->element_id,
        result->block0_hex,
        result->block1_hex);
    return true;
}