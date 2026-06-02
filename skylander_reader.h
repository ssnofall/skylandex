#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include "skylander_keygen.h"

typedef enum {
    SkylanderSectorReadOK,
    SkylanderSectorReadFailed,
    SkylanderSectorReadLocked,
} SkylanderSectorStatus;

typedef struct {
    bool detected;
    NfcProtocol protocol;
    char protocol_name[24];
    char uid_hex[32];
    bool uid_available;
} ScanDetectionInfo;

typedef struct {
    bool read_ok;
    char uid_hex[32];
    bool uid_available;
    char block0_hex[48];
    char block1_hex[48];
    uint16_t character_id;
    uint16_t variant_id;
    uint8_t element_id;
    bool has_character_id;
    MfClassicBlock all_blocks[SKYLANDER_TOTAL_BLOCKS];
    MfClassicKey derived_keys[SKYLANDER_NUM_SECTORS];
    SkylanderSectorStatus sector_status[SKYLANDER_NUM_SECTORS];
    bool is_full_dump;
    uint16_t level;
    uint32_t xp;
    uint32_t gold;
    char nickname[20];
    uint16_t hero_points;
    uint16_t hat_id;
    uint16_t upgrade_path;
    uint8_t platform_flags;
    uint32_t heroic_challenges;
} ScanResult;

typedef void (*SkylanderReaderCallback)(void* context);

typedef enum {
    SkylanderReaderStateIdle,
    SkylanderReaderStateScanning,
    SkylanderReaderStateTagDetected,
    SkylanderReaderStateReading,
    SkylanderReaderStateDone,
    SkylanderReaderStateError,
} SkylanderReaderState;

typedef struct {
    Nfc* nfc;
    NfcScanner* scanner;
    SkylanderReaderCallback callback;
    void* callback_context;
    bool is_running;
    SkylanderReaderState state;
    NfcProtocol detected_protocol;
    bool has_detected_protocol;
    bool has_sector0_dump;
    MfClassicType mf_type;
} SkylanderReader;

SkylanderReader* skylander_reader_alloc();
void skylander_reader_free(SkylanderReader* reader);
void skylander_reader_start(SkylanderReader* reader, SkylanderReaderCallback callback, void* context);
void skylander_reader_stop(SkylanderReader* reader);
bool skylander_reader_get_detected_info(SkylanderReader* reader, ScanDetectionInfo* info);
bool skylander_reader_read_sector0(SkylanderReader* reader, ScanResult* result);
bool skylander_reader_read_all(SkylanderReader* reader, ScanResult* result);
void skylander_reader_parse_game_data(ScanResult* result);
bool skylander_reader_build_nfc_device(SkylanderReader* reader, NfcDevice* device);
