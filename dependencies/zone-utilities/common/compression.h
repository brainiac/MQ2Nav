
#pragma once

#include <cstdint>

namespace EQEmu {

uint32_t DeflateData(const char *buffer, uint32_t len, char *out_buffer, uint32_t out_len_max);
uint32_t InflateData(const char* buffer, uint32_t len, char* out_buffer, uint32_t out_len_max);

} // namespace EQEmu
