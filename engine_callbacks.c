#include "engine_callbacks.h"
static int resident(MemoriesMemory *m,const IOSGuestFunction *entry) {
    if(!entry->bank)return 1;
    const uint8_t *slot=Memories_Resolve(m,entry->bank,4,4);
    return slot && Memories_ReadLE32(slot)==entry->identifier;
}
IOSNativeFunction IOSGuest_FindFunction(MemoriesMemory *m,const IOSGuestFunction *map,
    size_t count,uint32_t address) {
    if(!map || !address)return NULL;
    for(size_t i=0;i<count;i++)
        if(map[i].guest==address && resident(m,&map[i]))return map[i].host;
    return NULL;
}
int IOSGuest_EncodeFunction(MemoriesMemory *m,const IOSGuestFunction *map,
    size_t count,IOSNativeFunction native,uint32_t *guest) {
    if(!map || !native || !guest)return 0;
    for(size_t i=0;i<count;i++)
        if(map[i].host==native && map[i].guest && resident(m,&map[i])) {
            *guest=map[i].guest;return 1;
        }
    return 0;
}
