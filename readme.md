# Skylandex
A Flipper Zero app for scanning, identifying, indexing, and emulating Skylander figures, with an on-device collection index and per-figure NFC dumps saved to SD card.

<img src="/images/1.png" alt="screenshot-1" width="400"/> <img src="/images/2.png" alt="screenshot-2" width="400"/>
<img src="/images/3.png" alt="screenshot-3" width="400"/> <img src="/images/4.png" alt="screenshot-4" width="400"/>

## Features
| Status | Feature |
|--------|---------|
| ✔ | Scan and identify Skylander figure |
| ✔ | Collection index saved to SD card |
| ✔ | Detail view: name, element, UID, VID, scan date |
| ✔ | Save NFC dump to SD card per figure |
| ✔ | LED color themed per element |
| ✔ | Emulate a saved figure via NFC |
| ✔ | Database lookup support for ALL Skylander figures (684 entries) |
| ❌ | On-device key gen: derives all 16 keys from UID |
| ❌ | Full 16-sector read: use derived keys to dump everything |
| ❌ | HALT state machine: go silent when portal says stop |
| ❌ | Write back saves: so progress actually saves to the dump |

### Emulation status
Emulation is present but limited. The portal will detect an emulated figure, but without on-device key generation only sector 0 is readable, meaning the character loads with no save data. Portal HALT handling is also not yet implemented.

Fully functioning emulation depends on:
- On-device key generation
- Full 16-sector backup
- HALT support

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
5. Select any Skylander to view its details or emulate it

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
- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) — the micro Flipper Build Tool
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

1. Tries to parse `/ext/apps_data/skylandex/skylander_db.bin` — on a match it loads directly
2. On failure, looks for `/ext/apps_assets/skylandex/skylander_db.bin` (auto-extracted by firmware from the `.fap`), copies it to the apps_data path, then parses
3. If neither is found, all lookups return `NULL`

Any rebuilt `.fap` automatically delivers the latest database to the SD card — no manual file copying needed.

#### Using compile_db.py

```bash
# Default: reads skylander_db.json, writes resources/skylander_db.bin
python compile_db.py

# Custom paths
python compile_db.py --input my_db.json --output my_db.bin
```

### Contributing
Pull requests are welcome. If you're adding Skylander entries, edit `skylander_db.json` and re-run `compile_db.py`. Source IDs from the [Skylander-IDs repo](https://github.com/Texthead1/Skylander-IDs).

## Authors
- [Snofall](https://github.com/ssnofall)

## License
[GNU General Public License v3.0](LICENSE)

> **Note:** This app is a personal hobby project and is not affiliated with or endorsed by Activision.
