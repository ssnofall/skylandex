# Skylandex
A Flipper Zero app for scanning, identifying, indexing, and emulating Skylander figures, with an on-device collection index and per-figure NFC dumps saved to SD card.

<img src="/images/1.png" alt="screenshot-1" width="400"/> <img src="/images/2.png" alt="screenshot-2" width="400"/>
<img src="/images/3.png" alt="screenshot-3" width="400"/> <img src="/images/4.png" alt="screenshot-4" width="400"/>

## Features
| Status | Feature |
|--------|---------|
| ✔ | Scan and identify Skylander figure |
| ✔ | Collection index saved to SD card |
| ✔ | Detail view: name, element, uid, vid, xp, gold, hat, & more |
| ✔ | Save NFC dump to SD card per figure |
| ✔ | LED color themed per element; scan progress indicator (magenta blink during read) |
| ✔ | Saved figure NFC file emulation |
| ✔ | Database lookup support for ALL Skylander figures (684 entries) |
| ✔ | Delete saved Skylanders from collection + NFC dump (long press in My Skylandex) |
| ✔ | On-device key gen: derives all 16 keys from UID |
| ✔ | Full 16-sector read: use derived keys to dump everything |
| ✔ | AES-128 app-layer decryption: AES-128 key to decrypt the 16-byte block in place. |
| 🔧 | HALT state machine [*(requires firmware fix - see below)*](#emulation-status) |
| 🔧 | Playable Skylander Figure Emulation [*(blocked by HALT - see below)*](#emulation-status) |
| ❌ | Write back data: edit values of saved Skylanders (xp, gold, nickname & more) |

## Installation

### Manual install (pre-built)
1. Download the latest `.fap` from the [Releases](../../releases) page
2. Copy it to `/ext/apps/NFC/` on your Flipper's SD card
3. The app will appear under **Apps → NFC**

### Build and install from source
See the [Development](#development) section below.

## Usage
1. Open **Skylandex** from the Flipper Zero apps menu
2. Select **Scan Figure** and hold a Skylander to the back of your Flipper
3. Once identified, press **Save** to add it to your collection
4. Select **My Skylandex** to browse saved Skylanders
5. Select any Skylander to view its details. Press **Left** for sector data, **Right** to emulate

### SD card paths
| Data | Path |
|------|------|
| Collection index | `/ext/apps_data/skylandex/collection.bin` |
| NFC dumps | `/ext/apps_data/skylandex/nfc/` |
| Skylander Database | `/ext/apps_data/skylandex/skylander_db.bin` |

## Supported Skylanders

### NFC scanning

Skylandex is compatible with all mainline Skylanders figures across the entire franchise, including Characters, Vehicles, and Creation Crystals from *Spyro's Adventure* through *Imaginators*.

The app reads, parses, and interacts with the standard **MIFARE Classic 1K** NFC chips embedded in these figures.

> **Note on Traps:** *Trap Team* Traps use MIFARE Ultralight chips and are **not yet supported**.

### Skylander database
The Skylander lookup database covers **684 Skylanders across the entire franchise**, including variants (Series 2, LightCore, Eon's Elite, etc.)

**Thank You!** [@Texthead1](https://github.com/Texthead1) for putting together a [Skylander-IDs repo](https://github.com/Texthead1/Skylander-IDs).

## Development

### Prerequisites
- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) - the micro Flipper Build Tool
- A Flipper Zero running [Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware) (other firmwares may work but are currently untested)
- Python 3.14
- A Skylander figure

### Setup

```bash
# Install uFBT
pip install ufbt

# Clone the repo
git clone https://github.com/ssnofall/skylandex
cd skylandex

# Pull the SDK for Momentum firmware
ufbt update --channel release --index-url https://up.momentum-fw.dev/firmware/directory.json
```

### Build

```bash
# Compile the app
ufbt build
```

The compiled `.fap` will be placed in `/dist`.

### Deploy to Flipper

Connect your Flipper via USB, then:

```bash
# Build and launch directly on device
ufbt launch
```

Or copy `dist/skylandex.fap` to `/ext/apps/NFC/` on the SD card manually.

### Database

The Skylander lookup database `skylander_db.json` is compiled into a fixed-length binary `/resources/skylander_db.bin` using `compile_db.py`. The binary is bundled into the `.fap` via `fap_file_assets="resources"` in `application.fam`.

**Entry Example:**
```json
[
  {"char_id": "0x0010", "variant_id": "0x0000", "name": "Spyro", "element": 1},
  {"char_id": "0x0010", "variant_id": "0x3810", "name": "Spyro (Eon's Elite)", "element": 1},
]
```

#### DB version

The version field in the binary header must match `DB_VERSION` in `character_db.c`. If they differ, `character_db_init()` fails to parse the SD copy and falls back to copying a fresh version from the firmware assets. Bump `DB_VERSION` in both `compile_db.py` and `character_db.c` whenever you change the data or binary format.

#### Auto-deploy

When the app launches, `character_db_init()` does the following:

1. Tries to parse `/ext/apps_data/skylandex/skylander_db.bin` - on a match it loads directly
2. On failure, looks for `/ext/apps_assets/skylandex/skylander_db.bin` (auto-extracted by firmware from the `.fap`), copies it to the apps_data path, then parses
3. If neither is found, all lookups return `NULL`

Any rebuilt `.fap` automatically delivers the latest database to the SD card - no manual file copying needed.

### Application-layer decryption

After solving the MIFARE Classic Crypto-1 layer (via UID-derived keys), a second encryption layer protects the game data. Skylanders figures encrypt all runtime data with **AES-128 ECB** — this includes XP, gold, nickname, hat, hero points, upgrade path, and heroic challenges. Without decryption, these fields are ciphertext.

**Which blocks are encrypted:** Blocks `0x08`-`0x0F` (sector 2) and `0x24`-`0x2F` (sector 9). Sector 0 (blocks `0x00`-`0x07`) is not encrypted.

**Key derivation:** Each block gets its own AES key, derived by MD5-hashing an 86-byte input:
- Bytes 0-31: first 32 bytes of the tag (blocks 0x00 + 0x01, containing the UID)
- Byte 32: the block index being decrypted
- Bytes 33-85: the 53-byte Activision copyright constant `" Copyright (C) 2010 Activision. All Rights Reserved. "`

**Decryption:** The 16-byte MD5 output is used as an AES-128 key to ECB-decrypt the 16-byte block in place.

**Implementation note:** The Flipper firmware's mbedTLS symbols (AES, MD5) are disabled for external apps, so this module includes self-contained AES-128 ECB and MD5 implementations.

#### Using compile_db.py

```bash
# Default: reads skylander_db.json, writes resources/skylander_db.bin
python compile_db.py

# Custom paths
python compile_db.py --input my_db.json --output my_db.bin
```

### Emulation status

NFC file emulation is functional. You can save a Skylander's full 16-sector dump to the SD card, load it back, and emulate it via the NFC listener. The portal will detect the emulated figure and begin the loading animation.

**The problem:** The loading animation restarts endlessly in a loop. The portal detects the figure, starts loading, but then immediately re-detects it as if it were placed fresh - over and over until the Flipper is removed from the portal.

**Root cause:** This is a firmware-level bug in Momentum 12 (and likely other firmwares). The Flipper's NFC listener worker (`nfc_worker_listener` in `lib/nfc/nfc.c`) unconditionally calls `furi_hal_nfc_listener_idle()` when the reader's RF field drops (`FuriHalNfcEventFieldOff`). This sends `ST25R3916_CMD_GOTO_SENSE` to the ST25R3916 chip, resetting it from HALT state back to Idle/Sense mode. The chip then responds to REQA again, and the portal treats it as a new tag placement - restarting the animation.

**Why this can't be fixed at the app level:** The `FieldOff` handler is hardcoded in the firmware's worker loop. The callback's return value is ignored for `FieldOff` events. Any HALT state set by the app is immediately overridden the next time the portal cycles its RF field. There is no SDK function or app-level code path that can prevent `furi_hal_nfc_listener_idle()` from running on `FieldOff`.

**The fix (one line in firmware):** Change `furi_hal_nfc_listener_idle()` to `furi_hal_nfc_listener_sleep()` in the `FieldOff` handler. `furi_hal_nfc_listener_sleep()` sends `ST25R3916_CMD_GOTO_SLEEP` instead, keeping the chip in HALT state. The chip will only respond to WUPA (wake-up), which is the correct behavior - the portal uses WUPA to re-find tags, and a real Skylander figure stays silent after HLTA.

```c
// lib/nfc/nfc.c - nfc_worker_listener()
// BEFORE (broken):
if(event & FuriHalNfcEventFieldOff) {
    nfc_event.type = NfcEventTypeFieldOff;
    instance->callback(nfc_event, instance->context);
    furi_hal_nfc_listener_idle();   // sends GOTO_SENSE → chip responds to REQA → re-detection loop
}

// AFTER (fixed):
if(event & FuriHalNfcEventFieldOff) {
    nfc_event.type = NfcEventTypeFieldOff;
    instance->callback(nfc_event, instance->context);
    furi_hal_nfc_listener_sleep();  // sends GOTO_SLEEP → chip stays halted → no re-detection
}
```

**Status:** This fix needs to be applied upstream in the Momentum firmware. Until then, emulation will continue to loop. The app-level code is correct and ready - it just needs the firmware to maintain HALT state across RF field power cycles.

### Contributing
Pull requests are welcome. If you're adding Skylander entries, edit `skylander_db.json` and re-run `compile_db.py`. Source IDs from the [Skylander-IDs repo](https://github.com/Texthead1/Skylander-IDs).

## Authors
- [Snofall](https://github.com/ssnofall)

## License
[GNU General Public License v3.0](LICENSE)

> **Note:** This app is a personal hobby project and is not affiliated with or endorsed by Activision.
