#ifndef LEKAK_IOS_ENGINE_CALLBACKS_H
#define LEKAK_IOS_ENGINE_CALLBACKS_H
#include "pc/memory.h"
typedef void (*IOSNativeFunction)(void);
/* Same guest/host/bank/identifier fields as the engine's function map. */
typedef struct {
    uint32_t guest;
    IOSNativeFunction host;
    uint32_t bank, identifier;
} IOSGuestFunction;
IOSNativeFunction IOSGuest_FindFunction(MemoriesMemory *memory,
    const IOSGuestFunction *map,size_t count,uint32_t address);
int IOSGuest_EncodeFunction(MemoriesMemory *memory,const IOSGuestFunction *map,
    size_t count,IOSNativeFunction native,uint32_t *guest);
#endif
