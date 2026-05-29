#include "skylander_reader.h"
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

// cached sector 0 blocks
static MfClassicBlock sector0_blocks[4];

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

// extract character ID from block 1
static void parse_character_id_from_block1(
    const MfClassicBlock* block,
    ScanResult* result) {
    result->character_id = ((uint16_t)block->data[1] << 8) | block->data[0];

    FURI_LOG_D(
        TAG,
        "NFC decode id=0x%04X b1[0]=0x%02X b1[1]=0x%02X",
        result->character_id,
        block->data[0],
        block->data[1]);

    const SkylanderInfo* info = character_db_lookup(result->character_id);
    result->element_id = (info != NULL) ? character_db_get_element(result->character_id) : 0;
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

// build an NFC device from cached sector 0 data
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

    // copy cached sector 0 blocks
    for(uint8_t block = 0; block < 4; block++) {
        mf_classic_set_block_read(mf_data, block, &sector0_blocks[block]);
    }

    // mark sector key as known
    mf_classic_set_key_found(
        mf_data,
        0,
        MfClassicKeyTypeA,
        mf_classic_key_to_uint64(&SKYLANDER_SECTOR0_KEY));

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
    parse_character_id_from_block1(&sector0_blocks[1], result);
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
