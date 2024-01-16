#include <stdint.h>

typedef struct {
    uint32_t crc;
} CRC32_State;

extern CRC32_State create_hash_engine();
extern void crc32_update(CRC32_State *state, const uint8_t *data, int64_t length);
extern uint32_t crc32_finalize(CRC32_State *state);
