#ifndef LEKAK_DISC_LOADER_H
#define LEKAK_DISC_LOADER_H
#include <stdio.h>
#include "pc/memory.h"
typedef struct LekakDiscResult {
    uint32_t sector_bytes, executable_bytes, load_address, load_bytes;
    uint32_t entry, gp, stack_base, stack_bytes, payload_hash;
    char error[160];
} LekakDiscResult;
/* Stream a user-owned USA image. Commit RAM only after full validation.
 * Accept ISO/2048 and single-track MODE2/2352. Never execute the entry point. */
int LekakDisc_Load(FILE *disc, MemoriesMemory *memory, LekakDiscResult *result);
#endif
