#pragma once

#include <cstdint>
#include <string>

namespace lumaforge {

// Builds the public identifier from the complete 48-bit, factory-programmed
// ESP32 base MAC. The raw MAC never leaves this function.
std::string deviceIdFromHardwareMac(uint64_t hardwareMac);

}  // namespace lumaforge
