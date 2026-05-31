# Skylandex
A Flipper Zero app for scanning, identifying, and emulating Skylander figures, with an on-device collection index and per-figure NFC dumps saved to SD card.

<img src="/screenshots/1.png" alt="screenshot-1" width="400"/> <img src="/screenshots/2.png" alt="screenshot-2" width="400"/>
<img src="/screenshots/3.png" alt="screenshot-3" width="400"/> <img src="/screenshots/4.png" alt="screenshot-4" width="400"/>

## Features
| Status | Feature |
|--------|---------|
| ✔ | Scan and identify Skylander figure |
| ✔ | Collection index saved to SD card |
| ✔ | Detail view: name, element, UID, scan date |
| ✔ | Save NFC dump to SD card per figure |
| ✔ | LED color themed per element |
| ✔ | Emulate a saved figure via NFC |
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

## Supported Skylanders

### NFC scanning

Skylandex is compatible with all mainline Skylanders figures across the entire franchise, including Characters, Vehicles, and Creation Crystals from *Spyro's Adventure* through *Imaginators*.

The app reads, parses, and interacts with the standard **MIFARE Classic 1K** NFC chips embedded in these figures.

> **Note on Traps:** *Trap Team* Traps use MIFARE Ultralight chips and are **not yet supported**.

### Character database
The internal character lookup database currently covers the **32 core Skylanders from *Spyro's Adventure***.

The database can be expanded in `character_db.c`. Use the [Skylander-IDs repo](https://github.com/Texthead1/Skylander-IDs) by [@Texthead1](https://github.com/Texthead1) to source IDs. Each entry needs the Skylander ID in hexadecimal, name, and element.

**Example:**
```c
{0x0010, "Spyro", "Magic"}
```

## Development

### Prerequisites
- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) — the micro Flipper Build Tool
- A Flipper Zero running [Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware) (other firmwares may work but are currently untested)
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

### Contributing
Pull requests are welcome. If you're adding Skylander entries to the character database, please source IDs from the [Skylander-IDs repo](https://github.com/Texthead1/Skylander-IDs) and include the hex ID, name, and element for each entry.

## Authors
- [Snofall](https://github.com/ssnofall)

## License
[MIT](LICENSE)

> **Note:** This app is a personal hobby project and is not affiliated with or endorsed by Activision.
