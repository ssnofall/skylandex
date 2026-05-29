#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

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
    uint8_t element_id;
    bool has_character_id;
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
bool skylander_reader_build_nfc_device(SkylanderReader* reader, NfcDevice* device);
