#include "crc32.h"

#define CRC32_POLYNOMIAL 0xEDB88320

CRC32_State crc32_create_engine() {
    CRC32_State state;
    state.crc = 0xFFFFFFFF;
    return state;
}

// CRC-32/ISO-HDLC (IEEE)
void crc32_update(CRC32_State *const state, const uint8_t *const data, const int64_t length) {
    for (int64_t i = 0; i < length; ++i) {
        state->crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (state->crc & 1) {
                state->crc = (state->crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                state->crc >>= 1;
            }
        }
    }
}

uint32_t crc32_finalize(const CRC32_State *const state) {
    return state->crc ^ 0xFFFFFFFF;
}
