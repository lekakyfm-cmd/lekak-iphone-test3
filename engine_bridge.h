#ifndef LEKAK_IOS_ENGINE_BRIDGE_H
#define LEKAK_IOS_ENGINE_BRIDGE_H
#include <stdint.h>
typedef struct {
    int allocation, memory_aliases, scratchpad_aliases, rejected_spans;
    int ordering_table, packet_collection, packet_validation, rendered_pixels;
    int gpu_invalid_rejection, rng, gte, callback_bank_dispatch;
    unsigned snapshot_words;
    uint32_t framebuffer_hash;
    uint16_t red_pixel, blue_pixel, violet_pixel;
} LekakEngineResult;
/* No game loop yet; fixture passes through genuine engine subsystems. */
int LekakEngine_Run(LekakEngineResult *result);
void LekakEngine_CopyRGBA(uint8_t *destination); /* 320 * 240 * 4 bytes */
#endif
