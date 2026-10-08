#ifndef LEKAK_ADDRESS_ADAPTER_H
#define LEKAK_ADDRESS_ADAPTER_H
#include <stdint.h>
#include <stddef.h>
/* Prototype: explicit guest addresses, never truncated native pointers. */
typedef struct { uint32_t data, callback; } GuestRecord;
typedef struct { uint32_t base; size_t size; unsigned char *host; } GuestRegion;
typedef int (*NativeCallback)(uint32_t);
typedef struct { uint32_t id; NativeCallback function; } CallbackEntry;
_Static_assert(sizeof(GuestRecord) == 8, "guest record layout");
_Static_assert(offsetof(GuestRecord, callback) == 4, "callback offset");
static inline void *guest_resolve(const GuestRegion *regions, size_t count,
                                uint32_t address, size_t bytes) {
    if (!bytes || bytes > UINT64_C(0x100000000) - address) return NULL;
    for (size_t i=0; i<count; ++i) {
        if (address < regions[i].base || !regions[i].host) continue;
        uint64_t offset = (uint64_t)address - regions[i].base;
        if (offset < regions[i].size && bytes <= regions[i].size-offset)
            return regions[i].host + (size_t)offset;
    }
    return NULL;
}
static inline int guest_dispatch(const CallbackEntry *entries, size_t count,
                                 uint32_t id, uint32_t argument, int *result) {
    if (!result) return 0;
    for (size_t i=0; i<count; ++i) {
        if (id == entries[i].id && entries[i].function) {
            *result = entries[i].function(argument);
            return 1;
        }
    }
    return 0;
}
#endif
