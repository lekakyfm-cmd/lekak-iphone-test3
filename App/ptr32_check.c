/* Uses the real engine header, not a replacement pointer representation. */
#define MEMORIES_PC 1
#include "port_ptr.h"
struct GuestRecord { void *G32 value; void (*G32 callback)(void); };
_Static_assert(sizeof(struct GuestRecord)==8, "Guest record must remain eight bytes");
_Static_assert(sizeof(void *G32)==4, "G32 must remain four bytes");
_Static_assert(__builtin_offsetof(struct GuestRecord, callback)==4, "Callback must retain guest offset");
void StoreGuest(struct GuestRecord *r, void *value, void (*callback)(void)) {
    r->value=value; r->callback=callback;
}
void CallGuest(struct GuestRecord *r) { CALL32(void (*)(void), r->callback)(); }
void StoreIndexed(struct GuestRecord *r, unsigned index, void *value) { r[index].value=value; }
void StoreByte(unsigned char *G32 p, unsigned char value) { *p=value; }
unsigned char ReadByte(unsigned char *G32 p) { return *p; }
