#include "../App/engine_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    LekakEngineResult r;
    int ok=LekakEngine_Run(&r);
    printf("engine: memory=%d scratchpad=%d bounds=%d OT=%d packets=%d validate=%d pixels=%d RNG=%d GTE=%d words=%u hash=%08X\n",
        r.memory_aliases,r.scratchpad_aliases,r.rejected_spans,r.ordering_table,r.packet_collection,
        r.packet_validation,r.rendered_pixels,r.rng,r.gte,r.snapshot_words,r.framebuffer_hash);
    assert(ok);
    uint8_t *rgba=malloc(320*240*4);assert(rgba);
    LekakEngine_CopyRGBA(rgba);
    assert(rgba[4*(100*320+50)]==255 && rgba[4*(100*320+50)+2]==0);
    assert(rgba[4*(100*320+150)]==0 && rgba[4*(100*320+150)+2]==255);
    free(rgba);return 0;
}
