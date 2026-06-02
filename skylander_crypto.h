#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include "skylander_keygen.h"

void skylander_crypto_decrypt_blocks(
    const MfClassicBlock blocks[SKYLANDER_TOTAL_BLOCKS],
    MfClassicBlock out[SKYLANDER_TOTAL_BLOCKS]);
