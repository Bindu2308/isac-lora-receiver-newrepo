#pragma once
#include <cstdint>
#include <cstddef>

namespace isac_lora {

// Standard CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), applied to the
// fragment payload bytes before whitening. This is an APPLICATION-level CRC,
// independent of whatever PHY-level CRC the radio (e.g. RadioLib/SX1262)
// already provides on real hardware. Keeping our own CRC lets the offline
// PHY chain (whitening/Hamming/interleaving) be tested standalone without
// depending on the radio driver.
uint16_t crc16_ccitt(const uint8_t* data, size_t len);

} // namespace isac_lora
