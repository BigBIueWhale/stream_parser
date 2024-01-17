#ifndef CRC32_ENGINE_H
#define CRC32_ENGINE_H

#include <stdint.h>

typedef struct {
    uint32_t crc;
} CRC32_State;

extern CRC32_State crc32_create_engine();
// To test if this standard is compatible with your app, the string: "Hello, World!"
// without the null terminator produces result: 0xEC4AC3D0 which is the result seen in https://crccalc.com/
extern void crc32_update(CRC32_State *state, const uint8_t *data, int64_t length);
extern uint32_t crc32_finalize(const CRC32_State *state);

#endif
